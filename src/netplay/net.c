/* Network sessions for our NETPLAY.R32: see net.h for the model.
 *
 * TCP frames: u32 length (of what follows), u8 type, body. Integers are little-endian,
 * strings are a u8 length plus the bytes. UDP (LAN search): "DOMQ" + version asks,
 * "DOMS" + version + session answers. */
#include "net.h"
#include <stdarg.h>
#include <ws2tcpip.h>

#define NET_VERSION 1
#define MAX_PEERS 32
#define MAX_SESSIONS 32
#define MAX_FRAME 0x10000
#define CONNECT_TIMEOUT 5000
#define SESSION_TIMEOUT 4000 /* a LAN session that stops answering disappears */

enum {
    F_JOIN_HELLO = 1, /* joiner -> host: magic, version, guid, listen port, password */
    F_JOIN_WELCOME,   /* host -> joiner: proc, session, peers, players */
    F_JOIN_REFUSE,    /* host -> joiner: reason */
    F_PEER_HELLO,     /* joiner -> other games: magic, version, guid, proc, listen port */
    F_PEER_PLAYERS,   /* answer: that game's own players */
    F_PLAYER_CREATE,  /* id, owner, names, data */
    F_PLAYER_DESTROY, /* id */
    F_PLAYER_DATA,    /* id, data */
    F_SESSION_DESC,   /* host -> all: flags, max */
    F_USER,           /* from, to, data */
    F_BYE,
};
#define MAGIC 0x4e4d4f44u /* "DOMN" */

typedef struct Player {
    int used;
    uint32_t id;
    int owner; /* game ("proc") id; the host is 1 */
    char sname[NET_NAME_LEN], lname[NET_NAME_LEN];
    uint8_t data[NET_MAX_PDATA];
    uint32_t dsize;
} Player;

typedef struct Peer {
    int used;
    SOCKET s;
    int proc;    /* 0 until its hello arrived */
    uint32_t ip; /* network order */
    uint16_t port;
    uint8_t *rbuf;
    uint32_t rlen, rcap;
} Peer;

typedef struct Msg {
    struct Msg *next;
    uint32_t from, to, size;
    uint8_t data[];
} Msg;

static struct {
    CRITICAL_SECTION lock;
    int ready;
    /* session */
    int in_session, is_host, new_players;
    uint8_t guid[16];
    char sname[128], password[64];
    uint32_t sflags, smax, user[4];
    int my_proc, next_proc;
    uint32_t next_seq;
    SOCKET listen_s, udp_s;
    uint16_t listen_port;
    Peer peers[MAX_PEERS];
    Player players[NET_MAX_PLAYERS];
    uint32_t local_id;
    HANDLE local_event;
    Msg *qhead, *qtail;
    HANDLE thread;
    volatile LONG running;
    /* LAN search */
    SOCKET enum_s;
    char target[128];
    NetSessionInfo cache[MAX_SESSIONS];
} N;

static const uint8_t APP_GUID[16] = {0x80, 0x72, 0x9e, 0x41, 0x04, 0xdf, 0xcf, 0x11,
                                     0x98, 0x4f, 0x00, 0xa0, 0x24, 0x32, 0xae, 0xeb};

/* ---- small helpers (no C runtime) ---- */

void np_debug(const char *fmt, ...)
{
    char buf[300];
    va_list ap;
    va_start(ap, fmt);
    wvsprintfA(buf, fmt, ap);
    va_end(ap);
    OutputDebugStringA(buf);
}

void *np_alloc(size_t n)
{
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n ? n : 1);
}

void np_free(void *p)
{
    if (p)
        HeapFree(GetProcessHeap(), 0, p);
}

size_t np_strlen(const char *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}

void np_strncpy(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    for (; i < n && src[i]; i++)
        dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = 0;
}

void np_strcpy(char *dst, const char *src)
{
    while ((*dst++ = *src++))
        ;
}

void np_strcat(char *dst, const char *src)
{
    np_strcpy(dst + np_strlen(dst), src);
}

int np_strcmp(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return (uint8_t)*a - (uint8_t)*b;
}

static void copy_name(char *dst, size_t size, const char *src)
{
    np_strncpy(dst, src ? src : "", size - 1);
    dst[size - 1] = 0;
}

void net_ip_string(uint32_t ip, char *out)
{
    const uint8_t *b = (const uint8_t *)&ip;
    wsprintfA(out, "%d.%d.%d.%d", b[0], b[1], b[2], b[3]);
}

/* ---- frame building and reading ---- */

typedef struct Buf {
    uint8_t d[MAX_FRAME];
    uint32_t n;
    int bad;
} Buf;

static void put(Buf *b, const void *p, uint32_t n)
{
    if (b->n + n > sizeof b->d) {
        b->bad = 1;
        return;
    }
    memcpy(b->d + b->n, p, n);
    b->n += n;
}
static void put8(Buf *b, uint8_t v) { put(b, &v, 1); }
static void put16(Buf *b, uint16_t v) { put(b, &v, 2); }
static void put32(Buf *b, uint32_t v) { put(b, &v, 4); }
static void putstr(Buf *b, const char *s)
{
    uint32_t n = s ? (uint32_t)np_strlen(s) : 0;
    if (n > 255)
        n = 255;
    put8(b, (uint8_t)n);
    put(b, s, n);
}
static void frame_start(Buf *b, uint8_t type)
{
    b->n = 0;
    b->bad = 0;
    put32(b, 0);
    put8(b, type);
}
static void frame_end(Buf *b)
{
    uint32_t len = b->n - 4;
    memcpy(b->d, &len, 4);
}

typedef struct Rd {
    const uint8_t *p;
    uint32_t n, at;
    int bad;
} Rd;

static void get(Rd *r, void *out, uint32_t n)
{
    if (r->at + n > r->n) {
        r->bad = 1;
        memset(out, 0, n);
        return;
    }
    memcpy(out, r->p + r->at, n);
    r->at += n;
}
static uint8_t get8(Rd *r) { uint8_t v; get(r, &v, 1); return v; }
static uint16_t get16(Rd *r) { uint16_t v; get(r, &v, 2); return v; }
static uint32_t get32(Rd *r) { uint32_t v; get(r, &v, 4); return v; }
static void getstr(Rd *r, char *out, uint32_t size)
{
    uint32_t n = get8(r);
    char tmp[256];
    get(r, tmp, n);
    tmp[n] = 0;
    copy_name(out, size, tmp);
}

/* ---- sockets ---- */

static int send_all(SOCKET s, const void *p, uint32_t n)
{
    const char *c = p;
    while (n) {
        int k = send(s, c, (int)n, 0);
        if (k <= 0)
            return NET_ERR;
        c += k;
        n -= (uint32_t)k;
    }
    return NET_OK;
}

static void set_blocking(SOCKET s, int on)
{
    u_long nb = on ? 0 : 1;
    ioctlsocket(s, FIONBIO, &nb);
}

static void tune(SOCKET s)
{
    int one = 1, timeout = 10000;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof one);
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof timeout);
}

static SOCKET connect_to(uint32_t ip, uint16_t port, DWORD timeout)
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in a;
    fd_set w, e;
    struct timeval tv;
    if (s == INVALID_SOCKET)
        return s;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = ip;
    a.sin_port = htons(port);
    set_blocking(s, 0);
    if (connect(s, (struct sockaddr *)&a, sizeof a) != 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
        closesocket(s);
        return INVALID_SOCKET;
    }
    FD_ZERO(&w);
    FD_ZERO(&e);
    FD_SET(s, &w);
    FD_SET(s, &e);
    tv.tv_sec = (long)(timeout / 1000);
    tv.tv_usec = (long)(timeout % 1000) * 1000;
    if (select(0, NULL, &w, &e, &tv) != 1 || FD_ISSET(s, &e)) {
        closesocket(s);
        return INVALID_SOCKET;
    }
    set_blocking(s, 1);
    tune(s);
    return s;
}

/* Reads one whole frame with a timeout (used while joining, before the thread runs). */
static int recv_frame(SOCKET s, uint8_t **out, uint32_t *len, DWORD timeout)
{
    DWORD start = GetTickCount();
    uint8_t hdr[4];
    uint32_t got = 0, need = 4;
    uint8_t *body = NULL;
    while (got < need) {
        fd_set r;
        struct timeval tv;
        DWORD left, el = GetTickCount() - start;
        int k;
        if (el >= timeout)
            goto fail;
        left = timeout - el;
        FD_ZERO(&r);
        FD_SET(s, &r);
        tv.tv_sec = (long)(left / 1000);
        tv.tv_usec = (long)(left % 1000) * 1000;
        if (select(0, &r, NULL, NULL, &tv) != 1)
            goto fail;
        if (!body) {
            k = recv(s, (char *)hdr + got, (int)(4 - got), 0);
            if (k <= 0)
                goto fail;
            got += (uint32_t)k;
            if (got == 4) {
                memcpy(&need, hdr, 4);
                if (need == 0 || need > MAX_FRAME)
                    goto fail;
                body = np_alloc(need);
                got = 0;
            }
        } else {
            k = recv(s, (char *)body + got, (int)(need - got), 0);
            if (k <= 0)
                goto fail;
            got += (uint32_t)k;
        }
    }
    *out = body;
    *len = need;
    return NET_OK;
fail:
    np_free(body);
    return NET_ERR;
}

static uint32_t parse_ip(const char *s)
{
    struct hostent *h;
    uint32_t ip = inet_addr(s);
    if (ip != INADDR_NONE)
        return ip;
    h = gethostbyname(s);
    if (h && h->h_addrtype == AF_INET && h->h_addr_list[0])
        return *(uint32_t *)h->h_addr_list[0];
    return 0;
}

static uint16_t ini_port(void)
{
    UINT p = GetPrivateProfileIntA("Dominion", "NetPort", NET_DEFAULT_PORT, "Dominion.ini");
    return (uint16_t)(p ? p : NET_DEFAULT_PORT);
}

uint32_t net_local_ip(void)
{
    /* original: the first address of the host name; if there is none, the address a
     * UDP socket would use to reach Ion Storm's old server (no packet is sent). We
     * skip loopback addresses, which some systems list for their own name. */
    char host[256];
    uint32_t ip = 0;
    if (gethostname(host, sizeof host) == 0) {
        struct hostent *h = gethostbyname(host);
        if (h && h->h_addrtype == AF_INET) {
            for (int i = 0; h->h_addr_list[i]; i++) {
                uint32_t a = *(uint32_t *)h->h_addr_list[i];
                if ((a & 0xff) != 127) {
                    ip = a;
                    break;
                }
            }
        }
    }
    if (!ip) {
        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s != INVALID_SOCKET) {
            struct sockaddr_in a;
            int len = sizeof a;
            memset(&a, 0, sizeof a);
            a.sin_family = AF_INET;
            a.sin_port = htons(7);
            a.sin_addr.s_addr = inet_addr("204.96.11.27");
            if (connect(s, (struct sockaddr *)&a, sizeof a) == 0 &&
                getsockname(s, (struct sockaddr *)&a, &len) == 0)
                ip = a.sin_addr.s_addr;
            closesocket(s);
        }
    }
    return ip;
}

/* ---- state helpers (lock held) ---- */

static Player *find_player(uint32_t id)
{
    for (int i = 0; i < NET_MAX_PLAYERS; i++)
        if (N.players[i].used && N.players[i].id == id)
            return &N.players[i];
    return NULL;
}

static int count_players(void)
{
    int n = 0;
    for (int i = 0; i < NET_MAX_PLAYERS; i++)
        n += N.players[i].used;
    return n;
}

static Player *add_player(uint32_t id, int owner, const char *sname, const char *lname,
                          const void *data, uint32_t dsize)
{
    Player *p = find_player(id);
    if (!p) {
        for (int i = 0; i < NET_MAX_PLAYERS && !p; i++)
            if (!N.players[i].used)
                p = &N.players[i];
        if (!p)
            return NULL;
    }
    memset(p, 0, sizeof *p);
    p->used = 1;
    p->id = id;
    p->owner = owner;
    copy_name(p->sname, sizeof p->sname, sname);
    copy_name(p->lname, sizeof p->lname, lname);
    if (dsize > NET_MAX_PDATA)
        dsize = NET_MAX_PDATA;
    if (data)
        memcpy(p->data, data, dsize);
    p->dsize = dsize;
    return p;
}

static void queue_msg(uint32_t from, uint32_t to, const void *data, uint32_t size)
{
    Msg *m;
    if (!N.local_id)
        return; /* like DirectPlay: messages only exist for players */
    m = np_alloc(sizeof *m + size);
    if (!m)
        return;
    m->from = from;
    m->to = to;
    m->size = size;
    memcpy(m->data, data, size);
    if (N.qtail)
        N.qtail->next = m;
    else
        N.qhead = m;
    N.qtail = m;
    if (N.local_event)
        SetEvent(N.local_event);
}

static void flush_queue(void)
{
    while (N.qhead) {
        Msg *m = N.qhead;
        N.qhead = m->next;
        np_free(m);
    }
    N.qtail = NULL;
}

/* System messages are stored as small records and turned into DPMSG_* structures when
 * received, because those hold pointers into the receive buffer. */
enum { SYS_CREATE = 1, SYS_DESTROY, SYS_DATA, SYS_SESSION, SYS_LOST };

static void queue_sys_player(int kind, const Player *p)
{
    Buf *b = np_alloc(sizeof *b);
    if (!b)
        return;
    put32(b, (uint32_t)kind);
    put32(b, p->id);
    putstr(b, p->sname);
    putstr(b, p->lname);
    put32(b, p->dsize);
    put(b, p->data, p->dsize);
    put32(b, (uint32_t)count_players());
    queue_msg(0, N.local_id, b->d, b->n);
    np_free(b);
}

static void queue_sys_session(void)
{
    uint32_t rec[1] = {SYS_SESSION};
    queue_msg(0, N.local_id, rec, sizeof rec);
}

static void send_frame_to(Peer *p, Buf *b)
{
    frame_end(b);
    if (p->used && p->proc && !b->bad)
        send_all(p->s, b->d, b->n);
}

static void send_frame_all(Buf *b)
{
    frame_end(b);
    if (b->bad)
        return;
    for (int i = 0; i < MAX_PEERS; i++)
        if (N.peers[i].used && N.peers[i].proc)
            send_all(N.peers[i].s, b->d, b->n);
}

static void put_player(Buf *b, const Player *p)
{
    put32(b, p->id);
    put32(b, (uint32_t)p->owner);
    putstr(b, p->sname);
    putstr(b, p->lname);
    put8(b, (uint8_t)p->dsize);
    put(b, p->data, p->dsize);
}

static Player *get_player(Rd *r, int silent)
{
    uint32_t id = get32(r);
    int owner = (int)get32(r);
    char sname[NET_NAME_LEN], lname[NET_NAME_LEN];
    uint8_t data[256];
    uint32_t dsize;
    Player *p;
    getstr(r, sname, sizeof sname);
    getstr(r, lname, sizeof lname);
    dsize = get8(r);
    get(r, data, dsize);
    if (r->bad || !id)
        return NULL;
    if (find_player(id))
        silent = 1; /* already known */
    p = add_player(id, owner, sname, lname, data, dsize);
    if (p && !silent && id != N.local_id)
        queue_sys_player(SYS_CREATE, p);
    return p;
}

static void put_session(Buf *b)
{
    put(b, N.guid, 16);
    put32(b, N.sflags);
    put32(b, N.smax);
    put32(b, (uint32_t)count_players());
    for (int i = 0; i < 4; i++)
        put32(b, N.user[i]);
    putstr(b, N.sname);
}

static void remove_peer(Peer *p, int notify)
{
    if (!p->used)
        return;
    np_debug("NETPLAY: game %d left\n", p->proc);
    if (p->s != INVALID_SOCKET)
        closesocket(p->s);
    if (p->proc) {
        for (int i = 0; i < NET_MAX_PLAYERS; i++) {
            Player *pl = &N.players[i];
            if (pl->used && pl->owner == p->proc) {
                if (notify)
                    queue_sys_player(SYS_DESTROY, pl);
                pl->used = 0;
            }
        }
    }
    np_free(p->rbuf);
    memset(p, 0, sizeof *p);
    p->s = INVALID_SOCKET;
}

static Peer *new_peer(SOCKET s, uint32_t ip)
{
    for (int i = 0; i < MAX_PEERS; i++) {
        if (!N.peers[i].used) {
            Peer *p = &N.peers[i];
            memset(p, 0, sizeof *p);
            p->used = 1;
            p->s = s;
            p->ip = ip;
            return p;
        }
    }
    closesocket(s);
    return NULL;
}

/* ---- frames arriving while in a session (network thread, lock held) ---- */

static void handle_join_hello(Peer *p, Rd *r)
{
    Buf *b;
    uint8_t guid[16];
    char pw[64];
    uint32_t magic = get32(r), version = get32(r);
    uint16_t port;
    uint32_t reason = 0;
    get(r, guid, 16);
    port = get16(r);
    getstr(r, pw, sizeof pw);
    if (r->bad || magic != MAGIC || version != NET_VERSION || memcmp(guid, N.guid, 16))
        reason = 3;
    else if (np_strcmp(pw, N.password))
        reason = 2;
    else if (!N.is_host || !N.new_players ||
             (N.sflags & (DPSESSION_NEWPLAYERSDISABLED | DPSESSION_JOINDISABLED)) ||
             (uint32_t)count_players() >= N.smax)
        reason = 1;
    b = np_alloc(sizeof *b);
    if (!b)
        return;
    if (reason) {
        np_debug("NETPLAY: join refused (%d)\n", reason);
        frame_start(b, F_JOIN_REFUSE);
        put32(b, reason);
        frame_end(b);
        send_all(p->s, b->d, b->n);
        np_free(b);
        remove_peer(p, 0);
        return;
    }
    p->proc = N.next_proc++;
    p->port = port;
    np_debug("NETPLAY: game %d joined\n", p->proc);
    frame_start(b, F_JOIN_WELCOME);
    put32(b, (uint32_t)p->proc);
    put_session(b);
    {
        int n = 0;
        for (int i = 0; i < MAX_PEERS; i++)
            n += N.peers[i].used && N.peers[i].proc && &N.peers[i] != p;
        put32(b, (uint32_t)n);
        for (int i = 0; i < MAX_PEERS; i++) {
            Peer *q = &N.peers[i];
            if (q->used && q->proc && q != p) {
                put32(b, (uint32_t)q->proc);
                put32(b, q->ip);
                put16(b, q->port);
            }
        }
    }
    put32(b, (uint32_t)count_players());
    for (int i = 0; i < NET_MAX_PLAYERS; i++)
        if (N.players[i].used)
            put_player(b, &N.players[i]);
    send_frame_to(p, b);
    np_free(b);
}

static void handle_peer_hello(Peer *p, Rd *r)
{
    Buf *b;
    uint8_t guid[16];
    uint32_t magic = get32(r), version = get32(r);
    int proc;
    get(r, guid, 16);
    proc = (int)get32(r);
    p->port = get16(r);
    if (r->bad || magic != MAGIC || version != NET_VERSION || memcmp(guid, N.guid, 16) || !proc) {
        remove_peer(p, 0);
        return;
    }
    for (int i = 0; i < MAX_PEERS; i++)
        if (N.peers[i].used && N.peers[i].proc == proc && &N.peers[i] != p)
            remove_peer(&N.peers[i], 1);
    p->proc = proc;
    np_debug("NETPLAY: connected to game %d\n", proc);
    b = np_alloc(sizeof *b);
    if (!b)
        return;
    frame_start(b, F_PEER_PLAYERS);
    {
        int n = 0;
        for (int i = 0; i < NET_MAX_PLAYERS; i++)
            n += N.players[i].used && N.players[i].owner == N.my_proc;
        put32(b, (uint32_t)n);
        for (int i = 0; i < NET_MAX_PLAYERS; i++)
            if (N.players[i].used && N.players[i].owner == N.my_proc)
                put_player(b, &N.players[i]);
    }
    send_frame_to(p, b);
    np_free(b);
}

static void handle_frame(Peer *p, const uint8_t *d, uint32_t n)
{
    Rd r = {d + 1, n - 1, 0, 0};
    uint8_t type = d[0];
    if (!p->proc) {
        if (type == F_JOIN_HELLO)
            handle_join_hello(p, &r);
        else if (type == F_PEER_HELLO)
            handle_peer_hello(p, &r);
        else
            remove_peer(p, 0);
        return;
    }
    switch (type) {
    case F_PLAYER_CREATE:
        get_player(&r, 0);
        break;
    case F_PEER_PLAYERS: {
        uint32_t k = get32(&r);
        for (uint32_t i = 0; i < k && !r.bad; i++)
            get_player(&r, 1);
        break;
    }
    case F_PLAYER_DESTROY: {
        Player *pl = find_player(get32(&r));
        if (pl && pl->id != N.local_id) {
            queue_sys_player(SYS_DESTROY, pl);
            pl->used = 0;
        }
        break;
    }
    case F_PLAYER_DATA: {
        Player *pl = find_player(get32(&r));
        uint32_t size = get32(&r);
        uint8_t data[NET_MAX_PDATA];
        if (size > NET_MAX_PDATA || r.bad)
            break;
        get(&r, data, size);
        if (pl && !r.bad) {
            memcpy(pl->data, data, size);
            pl->dsize = size;
            queue_sys_player(SYS_DATA, pl);
        }
        break;
    }
    case F_SESSION_DESC:
        N.sflags = get32(&r);
        N.smax = get32(&r);
        if (!r.bad)
            queue_sys_session();
        break;
    case F_USER: {
        uint32_t from = get32(&r), to = get32(&r);
        if (r.bad)
            break;
        if (to == 0 || to == N.local_id)
            queue_msg(from, N.local_id, d + 9, n - 9);
        break;
    }
    case F_BYE:
        remove_peer(p, 1);
        break;
    }
}

static void read_peer(Peer *p)
{
    char tmp[8192];
    int k = recv(p->s, tmp, sizeof tmp, 0);
    if (k <= 0) {
        remove_peer(p, 1);
        return;
    }
    if (p->rlen + (uint32_t)k > p->rcap) {
        uint32_t cap = p->rcap ? p->rcap : 16384;
        uint8_t *nb;
        while (cap < p->rlen + (uint32_t)k)
            cap *= 2;
        nb = np_alloc(cap);
        if (!nb) {
            remove_peer(p, 1);
            return;
        }
        memcpy(nb, p->rbuf, p->rlen);
        np_free(p->rbuf);
        p->rbuf = nb;
        p->rcap = cap;
    }
    memcpy(p->rbuf + p->rlen, tmp, (size_t)k);
    p->rlen += (uint32_t)k;
    while (p->used && p->rlen >= 4) {
        uint32_t len;
        memcpy(&len, p->rbuf, 4);
        if (len == 0 || len > MAX_FRAME) {
            remove_peer(p, 1);
            return;
        }
        if (p->rlen < 4 + len)
            break;
        handle_frame(p, p->rbuf + 4, len);
        if (!p->used)
            return;
        memmove(p->rbuf, p->rbuf + 4 + len, p->rlen - 4 - len);
        p->rlen -= 4 + len;
    }
}

static void answer_query(void)
{
    char q[64];
    struct sockaddr_in from;
    int fl = sizeof from;
    int k = recvfrom(N.udp_s, q, sizeof q, 0, (struct sockaddr *)&from, &fl);
    Buf *b;
    uint32_t version;
    if (k < 8 || memcmp(q, "DOMQ", 4))
        return;
    memcpy(&version, q + 4, 4);
    if (version != NET_VERSION)
        return;
    b = np_alloc(sizeof *b);
    if (!b)
        return;
    put(b, "DOMS", 4);
    put32(b, NET_VERSION);
    put16(b, N.listen_port);
    put_session(b);
    {
        int n = 0;
        for (int i = 0; i < NET_MAX_PLAYERS && n < NET_LIST_PLAYERS; i++)
            n += N.players[i].used;
        put8(b, (uint8_t)n);
        for (int i = 0, m = 0; i < NET_MAX_PLAYERS && m < n; i++) {
            if (N.players[i].used) {
                put32(b, N.players[i].id);
                putstr(b, N.players[i].sname);
                putstr(b, N.players[i].lname);
                m++;
            }
        }
    }
    sendto(N.udp_s, (const char *)b->d, (int)b->n, 0, (struct sockaddr *)&from, fl);
    np_free(b);
}

static DWORD WINAPI net_thread(LPVOID unused)
{
    (void)unused;
    while (N.running) {
        fd_set r;
        struct timeval tv = {0, 50000};
        FD_ZERO(&r);
        EnterCriticalSection(&N.lock);
        if (N.listen_s != INVALID_SOCKET)
            FD_SET(N.listen_s, &r);
        if (N.udp_s != INVALID_SOCKET)
            FD_SET(N.udp_s, &r);
        for (int i = 0; i < MAX_PEERS; i++)
            if (N.peers[i].used)
                FD_SET(N.peers[i].s, &r);
        LeaveCriticalSection(&N.lock);
        if (r.fd_count == 0) {
            Sleep(50);
            continue;
        }
        if (select(0, &r, NULL, NULL, &tv) <= 0)
            continue;
        EnterCriticalSection(&N.lock);
        if (N.running) {
            if (N.listen_s != INVALID_SOCKET && FD_ISSET(N.listen_s, &r)) {
                struct sockaddr_in a;
                int al = sizeof a;
                SOCKET s = accept(N.listen_s, (struct sockaddr *)&a, &al);
                if (s != INVALID_SOCKET) {
                    tune(s);
                    new_peer(s, a.sin_addr.s_addr);
                }
            }
            if (N.udp_s != INVALID_SOCKET && FD_ISSET(N.udp_s, &r))
                answer_query();
            for (int i = 0; i < MAX_PEERS; i++)
                if (N.peers[i].used && FD_ISSET(N.peers[i].s, &r))
                    read_peer(&N.peers[i]);
        }
        LeaveCriticalSection(&N.lock);
    }
    return 0;
}

/* ---- public ---- */

int net_init(void)
{
    WSADATA w;
    if (N.ready)
        return NET_OK;
    if (WSAStartup(MAKEWORD(2, 2), &w) != 0)
        return NET_ERR;
    InitializeCriticalSection(&N.lock);
    N.listen_s = N.udp_s = N.enum_s = INVALID_SOCKET;
    for (int i = 0; i < MAX_PEERS; i++)
        N.peers[i].s = INVALID_SOCKET;
    N.ready = 1;
    return NET_OK;
}

void net_shutdown(void)
{
    if (!N.ready)
        return;
    net_close();
    if (N.enum_s != INVALID_SOCKET)
        closesocket(N.enum_s);
    N.enum_s = INVALID_SOCKET;
    DeleteCriticalSection(&N.lock);
    WSACleanup();
    N.ready = 0;
}

void net_set_target(const char *address)
{
    if (!N.ready)
        return;
    EnterCriticalSection(&N.lock);
    copy_name(N.target, sizeof N.target, address);
    memset(N.cache, 0, sizeof N.cache);
    LeaveCriticalSection(&N.lock);
}

static void read_answers(void)
{
    for (;;) {
        uint8_t d[2048];
        struct sockaddr_in from;
        int fl = sizeof from, k;
        u_long avail = 0;
        Rd r;
        NetSessionInfo s, *slot = NULL;
        if (ioctlsocket(N.enum_s, FIONREAD, &avail) != 0 || avail == 0)
            return;
        k = recvfrom(N.enum_s, (char *)d, sizeof d, 0, (struct sockaddr *)&from, &fl);
        if (k < 8 || memcmp(d, "DOMS", 4))
            continue;
        r.p = d + 4;
        r.n = (uint32_t)k - 4;
        r.at = 0;
        r.bad = 0;
        if (get32(&r) != NET_VERSION)
            continue;
        memset(&s, 0, sizeof s);
        s.port = get16(&r);
        get(&r, s.guid, 16);
        s.flags = get32(&r);
        s.max_players = get32(&r);
        s.cur_players = get32(&r);
        for (int i = 0; i < 4; i++)
            s.user[i] = get32(&r);
        getstr(&r, s.name, sizeof s.name);
        s.n_players = get8(&r);
        if (s.n_players > NET_LIST_PLAYERS)
            s.n_players = NET_LIST_PLAYERS;
        for (int i = 0; i < s.n_players; i++) {
            s.player_id[i] = get32(&r);
            getstr(&r, s.player_short[i], sizeof s.player_short[i]);
            getstr(&r, s.player_long[i], sizeof s.player_long[i]);
        }
        if (r.bad)
            continue;
        s.ip = from.sin_addr.s_addr;
        s.seen = GetTickCount();
        for (int i = 0; i < MAX_SESSIONS && !slot; i++)
            if (N.cache[i].seen && !memcmp(N.cache[i].guid, s.guid, 16))
                slot = &N.cache[i];
        for (int i = 0; i < MAX_SESSIONS && !slot; i++)
            if (!N.cache[i].seen || GetTickCount() - N.cache[i].seen > SESSION_TIMEOUT)
                slot = &N.cache[i];
        if (slot)
            *slot = s;
    }
}

int net_enum_sessions(NetSessionInfo *out, int max)
{
    char q[8];
    struct sockaddr_in a;
    int n = 0;
    uint16_t port = ini_port();
    if (!N.ready)
        return 0;
    EnterCriticalSection(&N.lock);
    if (N.enum_s == INVALID_SOCKET) {
        int one = 1;
        N.enum_s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (N.enum_s != INVALID_SOCKET)
            setsockopt(N.enum_s, SOL_SOCKET, SO_BROADCAST, (const char *)&one, sizeof one);
    }
    if (N.enum_s == INVALID_SOCKET) {
        LeaveCriticalSection(&N.lock);
        return 0;
    }
    memcpy(q, "DOMQ", 4);
    {
        uint32_t v = NET_VERSION;
        memcpy(q + 4, &v, 4);
    }
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    if (N.target[0]) {
        /* "host" or "host:port", like DirectPlay's address box */
        char host[128];
        char *colon;
        uint16_t p = port;
        np_strcpy(host, N.target);
        colon = host;
        while (*colon && *colon != ':')
            colon++;
        if (*colon) {
            const char *c = colon + 1;
            UINT v = 0;
            *colon = 0;
            while (*c >= '0' && *c <= '9')
                v = v * 10 + (UINT)(*c++ - '0');
            if (v && v < 65536)
                p = (uint16_t)v;
        }
        a.sin_addr.s_addr = parse_ip(host);
        a.sin_port = htons(p);
        if (a.sin_addr.s_addr)
            sendto(N.enum_s, q, 8, 0, (struct sockaddr *)&a, sizeof a);
    } else {
        a.sin_port = htons(port);
        a.sin_addr.s_addr = INADDR_BROADCAST;
        sendto(N.enum_s, q, 8, 0, (struct sockaddr *)&a, sizeof a);
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* a host on this machine */
        sendto(N.enum_s, q, 8, 0, (struct sockaddr *)&a, sizeof a);
    }
    LeaveCriticalSection(&N.lock);
    Sleep(150); /* give hosts on the LAN time to answer */
    EnterCriticalSection(&N.lock);
    read_answers();
    for (int i = 0; i < MAX_SESSIONS && n < max; i++)
        if (N.cache[i].seen && GetTickCount() - N.cache[i].seen <= SESSION_TIMEOUT)
            out[n++] = N.cache[i];
    LeaveCriticalSection(&N.lock);
    return n;
}

static void make_guid(uint8_t *g)
{
    LARGE_INTEGER t;
    uint32_t x;
    QueryPerformanceCounter(&t);
    x = (uint32_t)t.LowPart ^ (GetCurrentProcessId() << 16) ^ GetTickCount();
    for (int i = 0; i < 16; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        g[i] = (uint8_t)(x >> 7);
    }
}

static int start_thread(void)
{
    N.running = 1;
    N.thread = CreateThread(NULL, 0, net_thread, NULL, 0, NULL);
    return N.thread ? NET_OK : NET_ERR;
}

static SOCKET listen_on(uint16_t port, uint16_t *actual)
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in a;
    int len = sizeof a;
    if (s == INVALID_SOCKET)
        return s;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    if (bind(s, (struct sockaddr *)&a, sizeof a) != 0 || listen(s, 8) != 0 ||
        getsockname(s, (struct sockaddr *)&a, &len) != 0) {
        closesocket(s);
        return INVALID_SOCKET;
    }
    *actual = ntohs(a.sin_port);
    return s;
}

int net_create_session(const char *name, const char *password, uint32_t flags,
                       uint32_t max_players, const uint32_t user[4])
{
    struct sockaddr_in a;
    uint16_t port = ini_port();
    if (!N.ready)
        return NET_ERR;
    net_close();
    EnterCriticalSection(&N.lock);
    N.listen_s = listen_on(port, &N.listen_port);
    if (N.listen_s == INVALID_SOCKET) {
        np_debug("NETPLAY: cannot listen on TCP port %d\n", port);
        LeaveCriticalSection(&N.lock);
        return NET_ERR;
    }
    N.udp_s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    if (N.udp_s != INVALID_SOCKET && bind(N.udp_s, (struct sockaddr *)&a, sizeof a) != 0) {
        np_debug("NETPLAY: UDP port %d busy, the game can only be joined by address\n", port);
        closesocket(N.udp_s);
        N.udp_s = INVALID_SOCKET;
    }
    make_guid(N.guid);
    copy_name(N.sname, sizeof N.sname, name);
    copy_name(N.password, sizeof N.password, password);
    N.sflags = flags;
    N.smax = max_players;
    memcpy(N.user, user, sizeof N.user);
    N.is_host = 1;
    N.new_players = 1;
    N.my_proc = 1;
    N.next_proc = 2;
    N.in_session = 1;
    np_debug("NETPLAY: hosting \"%s\" on port %d\n", N.sname, port);
    LeaveCriticalSection(&N.lock);
    return start_thread();
}

static int join_peer(uint32_t proc, uint32_t ip, uint16_t port)
{
    Buf *b = np_alloc(sizeof *b);
    SOCKET s = connect_to(ip, port, 3000);
    uint8_t *d = NULL;
    uint32_t n;
    Peer *p;
    if (!b || s == INVALID_SOCKET) {
        np_free(b);
        return NET_ERR;
    }
    frame_start(b, F_PEER_HELLO);
    put32(b, MAGIC);
    put32(b, NET_VERSION);
    put(b, N.guid, 16);
    put32(b, (uint32_t)N.my_proc);
    put16(b, N.listen_port);
    frame_end(b);
    if (send_all(s, b->d, b->n) || recv_frame(s, &d, &n, CONNECT_TIMEOUT) || d[0] != F_PEER_PLAYERS) {
        np_free(b);
        np_free(d);
        closesocket(s);
        return NET_ERR;
    }
    np_free(b);
    EnterCriticalSection(&N.lock);
    p = new_peer(s, ip);
    if (p) {
        p->proc = (int)proc;
        p->port = port;
        handle_frame(p, d, n);
    }
    LeaveCriticalSection(&N.lock);
    np_free(d);
    return p ? NET_OK : NET_ERR;
}

int net_join_session(const NetSessionInfo *s, const char *password)
{
    Buf *b;
    SOCKET hs;
    uint8_t *d = NULL;
    uint32_t n, np, nplayers;
    Rd r;
    Peer *host;
    struct {
        uint32_t proc, ip;
        uint16_t port;
    } list[MAX_PEERS];
    if (!N.ready)
        return NET_ERR;
    net_close();
    {
        UINT pp = GetPrivateProfileIntA("Dominion", "NetPeerPort", 0, "Dominion.ini");
        N.listen_s = listen_on((uint16_t)pp, &N.listen_port);
    }
    if (N.listen_s == INVALID_SOCKET)
        return NET_ERR;
    hs = connect_to(s->ip, s->port, CONNECT_TIMEOUT);
    b = np_alloc(sizeof *b);
    if (hs == INVALID_SOCKET || !b)
        goto fail;
    frame_start(b, F_JOIN_HELLO);
    put32(b, MAGIC);
    put32(b, NET_VERSION);
    put(b, s->guid, 16);
    put16(b, N.listen_port);
    putstr(b, password);
    frame_end(b);
    if (send_all(hs, b->d, b->n) || recv_frame(hs, &d, &n, CONNECT_TIMEOUT))
        goto fail;
    if (d[0] != F_JOIN_WELCOME) {
        np_debug("NETPLAY: the host refused the join\n");
        goto fail;
    }
    r.p = d + 1;
    r.n = n - 1;
    r.at = 0;
    r.bad = 0;
    EnterCriticalSection(&N.lock);
    N.my_proc = (int)get32(&r);
    get(&r, N.guid, 16);
    N.sflags = get32(&r);
    N.smax = get32(&r);
    get32(&r); /* current players */
    for (int i = 0; i < 4; i++)
        N.user[i] = get32(&r);
    getstr(&r, N.sname, sizeof N.sname);
    np = get32(&r);
    if (np > MAX_PEERS - 1)
        np = MAX_PEERS - 1;
    for (uint32_t i = 0; i < np; i++) {
        list[i].proc = get32(&r);
        list[i].ip = get32(&r);
        list[i].port = get16(&r);
    }
    nplayers = get32(&r);
    for (uint32_t i = 0; i < nplayers && !r.bad; i++)
        get_player(&r, 1);
    if (r.bad) {
        LeaveCriticalSection(&N.lock);
        goto fail;
    }
    host = new_peer(hs, s->ip);
    hs = INVALID_SOCKET;
    if (host) {
        host->proc = 1;
        host->port = s->port;
    }
    LeaveCriticalSection(&N.lock);
    for (uint32_t i = 0; i < np; i++)
        if (join_peer(list[i].proc, list[i].ip, list[i].port))
            np_debug("NETPLAY: cannot reach game %d\n", list[i].proc);
    np_free(d);
    np_free(b);
    EnterCriticalSection(&N.lock);
    N.is_host = 0;
    N.in_session = 1;
    LeaveCriticalSection(&N.lock);
    np_debug("NETPLAY: joined \"%s\" as game %d\n", N.sname, N.my_proc);
    return start_thread();
fail:
    np_free(d);
    np_free(b);
    if (hs != INVALID_SOCKET)
        closesocket(hs);
    net_close();
    return NET_ERR;
}

void net_close(void)
{
    if (!N.ready)
        return;
    if (N.thread) {
        Buf *b = np_alloc(sizeof *b);
        EnterCriticalSection(&N.lock);
        if (b) {
            frame_start(b, F_BYE);
            send_frame_all(b);
            np_free(b);
        }
        N.running = 0;
        LeaveCriticalSection(&N.lock);
        WaitForSingleObject(N.thread, 2000);
        CloseHandle(N.thread);
        N.thread = NULL;
    }
    EnterCriticalSection(&N.lock);
    for (int i = 0; i < MAX_PEERS; i++)
        if (N.peers[i].used)
            remove_peer(&N.peers[i], 0);
    if (N.listen_s != INVALID_SOCKET)
        closesocket(N.listen_s);
    if (N.udp_s != INVALID_SOCKET)
        closesocket(N.udp_s);
    N.listen_s = N.udp_s = INVALID_SOCKET;
    memset(N.players, 0, sizeof N.players);
    flush_queue();
    N.local_id = 0;
    N.local_event = NULL;
    N.in_session = N.is_host = 0;
    LeaveCriticalSection(&N.lock);
}

int net_in_session(void)
{
    return N.ready && N.in_session;
}

void net_enable_new_players(int on)
{
    if (N.ready)
        N.new_players = on;
}

int net_get_session(uint32_t *flags, uint32_t *max_players)
{
    if (!net_in_session())
        return NET_ERR;
    EnterCriticalSection(&N.lock);
    *flags = N.sflags;
    *max_players = N.smax;
    LeaveCriticalSection(&N.lock);
    return NET_OK;
}

int net_set_session(uint32_t flags, uint32_t max_players)
{
    Buf *b;
    if (!net_in_session() || !N.is_host)
        return NET_ERR;
    b = np_alloc(sizeof *b);
    if (!b)
        return NET_ERR;
    EnterCriticalSection(&N.lock);
    N.sflags = flags;
    N.smax = max_players;
    frame_start(b, F_SESSION_DESC);
    put32(b, flags);
    put32(b, max_players);
    send_frame_all(b);
    queue_sys_session(); /* DirectPlay tells the local player too */
    LeaveCriticalSection(&N.lock);
    np_free(b);
    return NET_OK;
}

int net_create_player(const char *short_name, const char *long_name, HANDLE event,
                      const void *data, uint32_t size, uint32_t *id)
{
    Buf *b;
    Player *p;
    if (!net_in_session())
        return NET_ERR;
    b = np_alloc(sizeof *b);
    if (!b)
        return NET_ERR;
    EnterCriticalSection(&N.lock);
    if (N.local_id || (uint32_t)count_players() >= N.smax) {
        LeaveCriticalSection(&N.lock);
        np_free(b);
        return NET_ERR;
    }
    /* ids are unique without asking the host: the game number is unique */
    p = add_player(((uint32_t)N.my_proc << 16) | ++N.next_seq, N.my_proc, short_name, long_name,
                   data, size);
    if (!p) {
        LeaveCriticalSection(&N.lock);
        np_free(b);
        return NET_ERR;
    }
    N.local_id = p->id;
    N.local_event = event;
    *id = p->id;
    frame_start(b, F_PLAYER_CREATE);
    put_player(b, p);
    send_frame_all(b);
    LeaveCriticalSection(&N.lock);
    np_free(b);
    return NET_OK;
}

int net_destroy_player(uint32_t id)
{
    Buf *b;
    Player *p;
    if (!net_in_session())
        return NET_ERR;
    b = np_alloc(sizeof *b);
    if (!b)
        return NET_ERR;
    EnterCriticalSection(&N.lock);
    p = find_player(id);
    if (!p || p->owner != N.my_proc) {
        LeaveCriticalSection(&N.lock);
        np_free(b);
        return NET_ERR;
    }
    frame_start(b, F_PLAYER_DESTROY);
    put32(b, id);
    send_frame_all(b);
    p->used = 0;
    if (id == N.local_id) {
        N.local_id = 0;
        N.local_event = NULL;
        flush_queue();
    }
    LeaveCriticalSection(&N.lock);
    np_free(b);
    return NET_OK;
}

int net_get_player_name(uint32_t id, char *short_name, int short_size, char *long_name,
                        int long_size)
{
    Player *p;
    int r = NET_ERR;
    if (!net_in_session())
        return r;
    EnterCriticalSection(&N.lock);
    p = find_player(id);
    if (p) {
        if (short_name)
            copy_name(short_name, (size_t)short_size, p->sname);
        if (long_name)
            copy_name(long_name, (size_t)long_size, p->lname);
        r = NET_OK;
    }
    LeaveCriticalSection(&N.lock);
    return r;
}

int net_get_player_data(uint32_t id, void *data, uint32_t *size)
{
    Player *p;
    int r = NET_ERR;
    if (!net_in_session())
        return r;
    EnterCriticalSection(&N.lock);
    p = find_player(id);
    if (p) {
        uint32_t n = p->dsize < *size ? p->dsize : *size;
        memcpy(data, p->data, n);
        *size = p->dsize;
        r = NET_OK;
    }
    LeaveCriticalSection(&N.lock);
    return r;
}

int net_set_player_data(uint32_t id, const void *data, uint32_t size)
{
    Buf *b;
    Player *p;
    if (!net_in_session() || size > NET_MAX_PDATA)
        return NET_ERR;
    b = np_alloc(sizeof *b);
    if (!b)
        return NET_ERR;
    EnterCriticalSection(&N.lock);
    p = find_player(id);
    if (!p || p->owner != N.my_proc) {
        LeaveCriticalSection(&N.lock);
        np_free(b);
        return NET_ERR;
    }
    memcpy(p->data, data, size);
    p->dsize = size;
    frame_start(b, F_PLAYER_DATA);
    put32(b, id);
    put32(b, size);
    put(b, data, size);
    send_frame_all(b);
    queue_sys_player(SYS_DATA, p); /* DirectPlay tells the local player too */
    LeaveCriticalSection(&N.lock);
    np_free(b);
    return NET_OK;
}

int net_get_player_ip(uint32_t id, uint32_t *ip)
{
    Player *p;
    int r = NET_ERR;
    if (!net_in_session())
        return r;
    EnterCriticalSection(&N.lock);
    p = find_player(id);
    if (p) {
        if (p->owner == N.my_proc) {
            *ip = net_local_ip();
            r = NET_OK;
        } else {
            for (int i = 0; i < MAX_PEERS; i++) {
                if (N.peers[i].used && N.peers[i].proc == p->owner) {
                    *ip = N.peers[i].ip;
                    r = NET_OK;
                }
            }
        }
    }
    LeaveCriticalSection(&N.lock);
    return r;
}

int net_enum_players(uint32_t *ids, int max, int remote_only)
{
    int n = 0;
    if (!net_in_session())
        return 0;
    EnterCriticalSection(&N.lock);
    for (int i = 0; i < NET_MAX_PLAYERS && n < max; i++)
        if (N.players[i].used && !(remote_only && N.players[i].owner == N.my_proc))
            ids[n++] = N.players[i].id;
    LeaveCriticalSection(&N.lock);
    return n;
}

int net_send(uint32_t from, uint32_t to, const void *data, uint32_t size)
{
    Buf *b;
    Player *p;
    int r = NET_OK;
    if (!net_in_session() || size > MAX_FRAME - 16)
        return NET_ERR;
    b = np_alloc(sizeof *b);
    if (!b)
        return NET_ERR;
    EnterCriticalSection(&N.lock);
    frame_start(b, F_USER);
    put32(b, from);
    put32(b, to);
    put(b, data, size);
    if (to == 0) {
        send_frame_all(b);
    } else if ((p = find_player(to)) == NULL) {
        r = NET_ERR;
    } else if (p->owner == N.my_proc) {
        if (to != from)
            queue_msg(from, to, data, size);
    } else {
        r = NET_ERR;
        for (int i = 0; i < MAX_PEERS; i++) {
            if (N.peers[i].used && N.peers[i].proc == p->owner) {
                send_frame_to(&N.peers[i], b);
                r = NET_OK;
            }
        }
    }
    LeaveCriticalSection(&N.lock);
    np_free(b);
    return r;
}

/* Builds the DirectPlay system message for one of our records into buf. */
static uint32_t build_sys(const Msg *m, uint8_t *buf, uint32_t cap)
{
    Rd r = {m->data, m->size, 0, 0};
    uint32_t kind = get32(&r);
    uint32_t *w = (uint32_t *)buf;
    if (kind == SYS_SESSION) {
        if (cap < 0x54)
            return 0x54;
        memset(buf, 0, 0x54);
        w[0] = DPSYS_SETSESSIONDESC;
        w[1] = 0x50; /* DPSESSIONDESC2 */
        w[2] = N.sflags;
        memcpy(buf + 0xc, N.guid, 16);
        memcpy(buf + 0x1c, APP_GUID, 16);
        w[0x2c / 4] = N.smax;
        w[0x30 / 4] = (uint32_t)count_players();
        for (int i = 0; i < 4; i++)
            w[0x44 / 4 + i] = N.user[i];
        return 0x54;
    }
    if (kind == SYS_LOST) {
        if (cap < 4)
            return 4;
        w[0] = DPSYS_SESSIONLOST;
        return 4;
    }
    {
        uint32_t id = get32(&r), dsize, cur;
        char sname[NET_NAME_LEN], lname[NET_NAME_LEN];
        uint8_t data[NET_MAX_PDATA];
        uint32_t need, at;
        getstr(&r, sname, sizeof sname);
        getstr(&r, lname, sizeof lname);
        dsize = get32(&r);
        if (dsize > NET_MAX_PDATA)
            dsize = NET_MAX_PDATA;
        get(&r, data, dsize);
        cur = get32(&r);
        need = 0x40 + dsize + (uint32_t)np_strlen(sname) + 1 + (uint32_t)np_strlen(lname) + 1;
        if (cap < need)
            return need;
        memset(buf, 0, 0x40);
        at = 0x40;
        if (kind == SYS_DATA) {
            /* DPMSG_SETPLAYERORGROUPDATA */
            w[0] = DPSYS_SETPLAYERORGROUPDATA;
            w[1] = DPPLAYERTYPE_PLAYER;
            w[2] = id;
            w[3] = (uint32_t)(uintptr_t)(buf + at);
            w[4] = dsize;
            memcpy(buf + at, data, dsize);
            return 0x14;
        }
        if (kind == SYS_CREATE) {
            /* DPMSG_CREATEPLAYERORGROUP */
            w[0] = DPSYS_CREATEPLAYERORGROUP;
            w[1] = DPPLAYERTYPE_PLAYER; /* the menu script reads this field */
            w[2] = id;
            w[3] = cur;
            w[4] = (uint32_t)(uintptr_t)(buf + at);
            w[5] = dsize;
            memcpy(buf + at, data, dsize);
            at += dsize;
            w[6] = 0x10; /* DPNAME */
            w[8] = (uint32_t)(uintptr_t)(buf + at);
            np_strcpy((char *)buf + at, sname);
            at += (uint32_t)np_strlen(sname) + 1;
            w[9] = (uint32_t)(uintptr_t)(buf + at);
            np_strcpy((char *)buf + at, lname);
            return 0x30;
        }
        /* DPMSG_DESTROYPLAYERORGROUP */
        w[0] = DPSYS_DESTROYPLAYERORGROUP;
        w[1] = DPPLAYERTYPE_PLAYER;
        w[2] = id;
        w[5] = (uint32_t)(uintptr_t)(buf + at);
        w[6] = dsize;
        memcpy(buf + at, data, dsize);
        at += dsize;
        w[7] = 0x10;
        w[9] = (uint32_t)(uintptr_t)(buf + at);
        np_strcpy((char *)buf + at, sname);
        at += (uint32_t)np_strlen(sname) + 1;
        w[10] = (uint32_t)(uintptr_t)(buf + at);
        np_strcpy((char *)buf + at, lname);
        return 0x34;
    }
}

int net_receive(uint32_t *from, uint32_t *to, int flags, void *buf, uint32_t *size)
{
    Msg *m, *prev = NULL;
    int r = NET_NOMESSAGES;
    if (!N.ready)
        return NET_ERR;
    EnterCriticalSection(&N.lock);
    for (m = N.qhead; m; prev = m, m = m->next) {
        if (!(flags & NET_RECEIVE_ALL)) {
            if ((flags & NET_RECEIVE_TOPLAYER) && m->to != *to)
                continue;
            if ((flags & NET_RECEIVE_FROMPLAYER) && m->from != *from)
                continue;
        }
        break;
    }
    if (m) {
        uint32_t n;
        if (m->from == 0) {
            uint8_t tmp[0x200];
            n = build_sys(m, tmp, sizeof tmp);
            if (n > *size) {
                *size = n;
                r = NET_ERR;
                goto out;
            }
            n = build_sys(m, buf, *size);
        } else {
            n = m->size;
            if (n > *size) {
                *size = n;
                r = NET_ERR;
                goto out;
            }
            memcpy(buf, m->data, n);
        }
        *size = n;
        *from = m->from;
        *to = m->to;
        if (prev)
            prev->next = m->next;
        else
            N.qhead = m->next;
        if (N.qtail == m)
            N.qtail = prev;
        np_free(m);
        r = NET_OK;
    }
out:
    LeaveCriticalSection(&N.lock);
    return r;
}
