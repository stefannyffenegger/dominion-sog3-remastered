/* Our NETPLAY.R32: the game's multiplayer module.
 *
 * The original was a thin layer over DirectPlay. This one keeps that layer's logic
 * (player slots 1..8, its own small messages for slot numbers, file transfer and
 * latency, the messages it hands to the menu script) and runs it on our own network
 * code (net.c) instead of DirectPlay.
 *
 * Who calls what:
 *  - The menu script (DOMINION.bin) calls most exports by name: provider and session
 *    lists, create/join, the lobby chat and settings (DPSendMsg/DPReceiveMsg).
 *  - The game logic (ISO.R32) calls DPSendMsg, DPReceiveMsgDirect from its network
 *    thread, and a few getters. It only reacts to two system messages: 5 (a player
 *    left) and 0x31 (connection lost).
 * Every export is cdecl; the first argument is the context RtLibInit returned.
 * Behavior kept from the original on purpose is marked "original:". */
#include "net.h"
#include <mmsystem.h>

/* engine (Rtlib32.dll) */
char *__cdecl RtGetString(void *app, unsigned id);
int __cdecl RtRunMsg(void *app, void *msg);
HWND __cdecl RtGetWindow(void *app);
void __cdecl RtShowGDISurface(void *app);
void __cdecl RtRestoreFromGDISurface(void *app);

#define MAX_SLOTS 16
#define BUF_SIZE 0x10000 /* send/receive buffers; the game is told 0x800 */
#define MAX_BUF 0x800
#define MAX_SESS 32
#define PROVIDER_NAME "Internet TCP/IP Connection For DirectPlay"

/* The module's own messages; the first dword of every message is its type.
 * The game's messages start at 0x7d1. */
enum {
    MSG_FILE = 1,       /* file transfer chunk or acknowledgement */
    MSG_DESTROY = 2,    /* "leave the session" */
    MSG_SLOT = 3,       /* {3, slot, id}: player id has slot number */
    MSG_PING = 4,       /* latency measurement, see DPSyncPlayer */
    MSG_YOUR_SLOT = 6,  /* host -> new player: {6, slot, id} */
    MSG_DISCONNECT = 7, /* {7, slot}: that player is gone */
};

typedef struct Slot {
    uint32_t id; /* 0 = free */
    char name[21];
    uint32_t latency;     /* ms, averaged by DPSyncPlayer */
    uint32_t latency_sum;
} Slot;

typedef struct Session {
    NetSessionInfo info;
    int seen;    /* answered in this search */
    int gone;    /* original: entries are kept but hidden once they stop answering */
    int version_ok;
} Session;

/* File transfer header (0x35 bytes, packed as in the original) */
#pragma pack(push, 1)
typedef struct FileChunk {
    uint32_t type;   /* MSG_FILE */
    uint32_t chunk;  /* number of this chunk */
    uint32_t chunks; /* total */
    uint32_t last;
    uint32_t size;   /* bytes of file data in this chunk */
    uint32_t offset; /* file offset */
    uint8_t data;    /* 1 = data (sender -> receiver), 0 = acknowledgement */
    FILETIME created, accessed, written;
    uint32_t msg;    /* script message for progress, 0 = none */
} FileChunk;
#pragma pack(pop)

typedef struct Ctx {
    void *app;
    int provider;             /* a provider was picked (DPInit) */
    int auto_player_add;      /* DPSetAutoPlayerAdd; original: stored, never used */
    uint32_t max_players;     /* from DPInit */
    uint32_t sess_flags, sess_max; /* our copy of the session description */
    Session sessions[MAX_SESS];
    int n_sessions;
    char *enum_string;        /* DPEnumSessions' output string */
    Slot slots[MAX_SLOTS];
    uint32_t n_slots;         /* highest slot in use */
    int have_slots;
    uint32_t my_id, my_slot;
    uint32_t file_msg;
    HANDLE event;             /* signaled when a message arrives (DPGetPlayerEvent) */
    uint32_t max_buf, max_latency;
    HANDLE file;              /* file being sent or received */
    int file_refs;
    int is_host;
    uint32_t host_slot;
    uint8_t *recv_buf, *send_buf;
    CRITICAL_SECTION send_lock;
    int send_lock_ready;
    int(__stdcall *msg_proc)(void *, unsigned, unsigned, unsigned);
    void *msg_proc_ctx;
    HANDLE msg_thread;
    uint32_t send_bytes, recv_bytes, send_msgs, recv_msgs;
    uint32_t game_data[4];    /* DPSetGameData; [0] is the game's version check */
} Ctx;

static Ctx *g_ctx; /* original: DPIsMPlayer and NetTallyMPlayerScores use the last context */

int __cdecl DPDestroyPlayer(Ctx *c);
int __cdecl DPDisconnectPlayer(Ctx *c, uint32_t slot);
int __cdecl DPDestroyPlayerRemote(Ctx *c, uint32_t slot);
int __cdecl DPSendMsg(Ctx *c, uint32_t slot, uint32_t flags, uint32_t type, const void *data,
                      uint32_t size);

/* ---- helpers ---- */

static int post(Ctx *c, uint32_t id, uint8_t type, const uint32_t *args, int n)
{
    uint8_t m[0x22];
    memset(m, 0, sizeof m);
    memcpy(m + 2, &id, 4);
    m[9] = type;
    memcpy(m + 10, args, (size_t)n * 4);
    return RtRunMsg(c->app, m);
}

static uint32_t slot_id(Ctx *c, uint32_t slot) /* slot number -> player id */
{
    if (slot == 0 || slot > c->n_slots)
        return 0;
    return c->slots[slot - 1].id;
}

static uint32_t id_slot(Ctx *c, uint32_t id) /* player id -> slot number, 0 = none */
{
    for (uint32_t i = 0; i < c->n_slots; i++)
        if (c->slots[i].id == id)
            return i + 1;
    return 0;
}

static uint32_t add_player(Ctx *c, uint32_t id, const char *name)
{
    uint32_t i;
    for (i = 0; i < c->n_slots && c->slots[i].id != id; i++) {
        if (c->slots[i].id == 0) {
            c->slots[i].id = id;
            np_strncpy(c->slots[i].name, name, 20);
            c->slots[i].name[20] = 0;
            return i + 1;
        }
    }
    if (i == c->n_slots && i < c->max_players && i < MAX_SLOTS) {
        c->slots[i].id = id;
        np_strncpy(c->slots[i].name, name, 20);
        c->slots[i].name[20] = 0;
        c->n_slots++;
        return i + 1;
    }
    np_debug("NETPLAY: add_player failed\n");
    return 0; /* original: also when the id already has a slot */
}

static int add_player_at(Ctx *c, uint32_t slot, uint32_t id)
{
    char name[24];
    if ((int)slot < 1 || slot > MAX_SLOTS)
        return 0;
    if (net_get_player_name(id, name, 21, NULL, 0) != NET_OK)
        return 0;
    c->slots[slot - 1].id = id;
    np_strncpy(c->slots[slot - 1].name, name, 20);
    c->slots[slot - 1].name[20] = 0;
    if (c->n_slots < slot)
        c->n_slots = slot;
    return 1;
}

static uint32_t remove_player(Ctx *c, uint32_t id)
{
    uint32_t i;
    for (i = 0; i < c->n_slots; i++) {
        if (c->slots[i].id == id) {
            memset(&c->slots[i], 0, sizeof c->slots[i]);
            break;
        }
    }
    return i + 1; /* original: n_slots + 1 if the id has no slot */
}

static int send_raw(Ctx *c, uint32_t to, const void *data, uint32_t size)
{
    return net_send(c->my_id, to, data, size);
}

static void free_buffers(Ctx *c)
{
    np_free(c->recv_buf);
    np_free(c->send_buf);
    c->recv_buf = c->send_buf = NULL;
}

static void close_file(Ctx *c)
{
    if (c->file) {
        CloseHandle(c->file);
        c->file = NULL;
        c->file_refs = 0;
    }
}

/* What DPDestroyPlayer and DPDisconnectPlayer do after destroying our player */
static void after_leave(Ctx *c)
{
    if (c->send_lock_ready) {
        DeleteCriticalSection(&c->send_lock);
        c->send_lock_ready = 0;
    }
    net_close();
    free_buffers(c);
    close_file(c);
    c->my_id = 0;
    c->n_slots = 0;
    memset(c->slots, 0, sizeof c->slots);
    c->have_slots = 0;
    c->file_msg = 0;
    c->event = NULL; /* original: forgotten, not closed */
}

static uint32_t ini_int(const char *key, int def)
{
    return GetPrivateProfileIntA("Dominion", key, def, "Dominion.ini");
}

/* ---- file transfer (maps), message type 1 ---- */

/* Receiver: a data chunk arrived; write it and acknowledge. */
static int file_receive(Ctx *c, uint32_t from)
{
    FileChunk *h = (FileChunk *)c->recv_buf;
    char *data = (char *)c->recv_buf + sizeof *h;
    char path[300];
    uint32_t name_len = 0;
    DWORD n;
    if (h->chunk == 0) {
        /* original: MapsDir + name, with doubled backslashes made single */
        char dir[256];
        uint32_t i = 0, j = 0;
        GetPrivateProfileStringA("Dominion", "MapsDir", "", dir, 255, "Dominion.ini");
        np_strcpy(path, dir);
        np_strcat(path, data);
        while (path[i]) {
            path[j++] = path[i];
            i += (path[i] == '\\' && path[i + 1] == '\\') ? 2 : 1;
        }
        path[j] = 0;
        if (c->file_refs == 0) {
            c->file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, NULL);
            if (c->file == INVALID_HANDLE_VALUE) {
                SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
                c->file = CreateFileA(data, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL, NULL);
                if (c->file == INVALID_HANDLE_VALUE) {
                    c->file = NULL;
                    RtShowGDISurface(c->app);
                    MessageBoxA(NULL, "Can't open destination file.", "Dominion", 0);
                    RtRestoreFromGDISurface(c->app);
                }
            }
        }
        c->file_refs++;
        name_len = (uint32_t)np_strlen(data) + 1;
    }
    if (!c->file)
        return 0;
    SetFilePointer(c->file, (LONG)h->offset, NULL, FILE_BEGIN);
    WriteFile(c->file, data + name_len, h->size, &n, NULL);
    if (!h->last) {
        h->offset += h->size;
        h->chunk++;
        h->size += name_len;
        h->data = 0;
        memcpy(c->send_buf, h, sizeof *h);
        if (send_raw(c, from, c->send_buf, sizeof *h) != NET_OK)
            return 0;
    } else {
        SetFileTime(c->file, &h->created, &h->accessed, &h->written);
        if (--c->file_refs == 0) {
            CloseHandle(c->file);
            c->file = NULL;
        }
    }
    if (h->msg) {
        uint32_t a[3] = {h->chunk, h->chunks, id_slot(c, from) - 1};
        post(c, h->msg, 3, a, 3);
    }
    return 1;
}

/* Sender: the receiver acknowledged a chunk; send the next one. */
static int file_send_next(Ctx *c, uint32_t to)
{
    FileChunk *h = (FileChunk *)c->recv_buf;
    DWORD n;
    if (h->chunks < h->chunk) {
        if (c->file && --c->file_refs == 0) {
            CloseHandle(c->file);
            c->file = NULL;
        }
        return 1;
    }
    if (!c->file)
        return 0;
    SetFilePointer(c->file, (LONG)h->offset, NULL, FILE_BEGIN);
    h->data = 1;
    ReadFile(c->file, c->send_buf + sizeof *h, h->size, &n, NULL);
    if (n < h->size) {
        h->size = n;
        h->last = 1;
        if (--c->file_refs == 0) {
            CloseHandle(c->file);
            c->file = NULL;
        }
    }
    memcpy(c->send_buf, h, sizeof *h);
    if (send_raw(c, to, c->send_buf, h->size + sizeof *h) != NET_OK)
        return 0;
    if (h->msg) {
        uint32_t a[3] = {h->chunk, h->chunks, id_slot(c, to) - 1};
        post(c, h->msg, 3, a, 3);
    }
    return 1;
}

/* ---- receiving ---- */

/* MSG_PING: our own ping came back, or someone else's goes back to them. */
static void handle_ping(Ctx *c, uint32_t from, uint32_t *ping)
{
    /* {4, sender id, rounds, done, time sent, script message} */
    if (ping[1] == c->my_id) {
        DWORD now = timeGetTime();
        uint32_t slot = id_slot(c, from), rtt;
        Slot *s;
        if (slot == 0)
            return;
        rtt = now < ping[4] ? now + (uint32_t)(-1 - (int32_t)ping[4]) : now - ping[4];
        s = &c->slots[slot - 1];
        s->latency_sum += rtt;
        ping[3]++;
        if (ping[3] < ping[2]) {
            ping[4] = timeGetTime();
            send_raw(c, from, ping, 0x18);
        } else {
            s->latency = s->latency_sum / ping[2];
            s->latency_sum = 0;
            if (c->max_latency < s->latency)
                c->max_latency = s->latency;
            post(c, ping[5], 0, NULL, 0);
        }
    } else {
        send_raw(c, from, ping, 0x18);
    }
}

/* MSG_YOUR_SLOT {6, slot, id} on a joining player: take the slot and tell everyone.
 * original: the message is rewritten in place to the {3, slot, id} that is sent on. */
static int take_slot(Ctx *c, uint32_t *m)
{
    uint32_t data[2];
    uint32_t size = sizeof data;
    c->my_slot = m[1];
    if (add_player_at(c, c->my_slot, m[2]) && net_get_player_data(c->my_id, data, &size) == NET_OK) {
        data[0] = c->my_slot;
        net_set_player_data(c->my_id, data, size);
        m[0] = MSG_SLOT;
        m[1] = c->my_slot;
        m[2] = c->my_id;
        if (send_raw(c, 0, m, 12) != NET_OK)
            return 0;
    }
    return 1;
}

/* Host: DPSYS_CREATEPLAYER -> give the new player a slot and tell it.
 * Returns the slot, 0 if none. */
static uint32_t give_slot(Ctx *c, uint32_t *m)
{
    uint32_t id = m[2];
    uint32_t slot = add_player(c, id, (const char *)(uintptr_t)m[8]);
    uint32_t msg[3] = {MSG_YOUR_SLOT, slot, id};
    np_debug("NETPLAY: MSGTYPE_REQUESTIDX %d\n", slot);
    if (slot == 0 || send_raw(c, id, msg, sizeof msg) != NET_OK)
        return 0;
    return slot;
}

/* A player left (DPSYS_DESTROYPLAYER): free its slot, pick a new host slot. */
static uint32_t player_left(Ctx *c, uint32_t id)
{
    uint32_t slot = remove_player(c, id);
    if (c->host_slot == slot) {
        for (uint32_t i = 0; i < c->n_slots; i++) {
            if (c->slots[i].id) {
                if (c->slots[i].id == c->my_id)
                    c->is_host = 1;
                c->host_slot = i + 1;
                break;
            }
        }
    }
    return slot;
}

static int receive(Ctx *c, uint32_t *from, uint8_t *buf, uint32_t *size, uint32_t want_slot)
{
    uint32_t to = c->my_id;
    int flags = 0;
    *from = slot_id(c, want_slot);
    if (to == 0) {
        if (*from == 0)
            flags = NET_RECEIVE_ALL;
    } else {
        flags = NET_RECEIVE_TOPLAYER;
    }
    if (*from)
        flags |= NET_RECEIVE_FROMPLAYER;
    return net_receive(from, &to, flags, buf, size);
}

/* Messages that only concern this module: handled here, then still reported to the caller.
 * Returns 0 if a reply could not be sent. */
static int handle_own(Ctx *c, uint32_t from, uint32_t *m, int direct)
{
    switch (m[0]) {
    case MSG_FILE:
        /* original: the file code always reads the module's receive buffer, also for
         * DPReceiveMsgDirect, where the message is in the caller's buffer */
        if (((FileChunk *)m)->data == 0)
            file_send_next(c, from);
        else
            file_receive(c, from);
        break;
    case MSG_DESTROY:
        DPDestroyPlayer(c);
        break;
    case MSG_SLOT:
        if (!c->is_host)
            add_player_at(c, m[1], m[2]);
        break;
    case MSG_PING:
        handle_ping(c, from, m);
        break;
    case MSG_YOUR_SLOT:
        if (!take_slot(c, m))
            return 0;
        break;
    case MSG_DISCONNECT:
        DPDisconnectPlayer(c, m[1]);
        break;
    }
    (void)direct;
    return 1;
}

/* ---- exports: setup ---- */

void *__cdecl RtLibInit(void *app)
{
    Ctx *c = np_alloc(sizeof *c);
    if (!c)
        return NULL;
    c->app = app;
    g_ctx = c;
    if (net_init() != NET_OK)
        np_debug("NETPLAY: Winsock not available\n");
    return c;
}

void __cdecl DPDeInit(Ctx *c);

void __cdecl RtLibFree(Ctx *c)
{
    if (!c)
        return;
    if (c->msg_thread) {
        TerminateThread(c->msg_thread, 0);
        CloseHandle(c->msg_thread);
    }
    if (c->my_id)
        net_destroy_player(c->my_id);
    net_shutdown();
    free_buffers(c);
    close_file(c);
    if (c->event)
        CloseHandle(c->event);
    if (g_ctx == c)
        g_ctx = NULL;
    np_free(c);
}

/* Was the game started by a lobby (MPlayer.com, a DirectPlay lobby)? Never: 0. */
int __cdecl DPCheckForConnect(Ctx *c)
{
    (void)c;
    return 0;
}

int __cdecl DPIsMPlayer(void)
{
    return 0;
}

int __cdecl NetTallyMPlayerScores(void *scores)
{
    (void)scores;
    return 0;
}

int __cdecl DPIsValidVersion(Ctx *c)
{
    (void)c;
    return 5; /* original: "DirectPlay 5 or later" */
}

void __cdecl DPSetAutoPlayerAdd(Ctx *c, int on)
{
    c->auto_player_add = on;
}

int __cdecl DPSetGameData(Ctx *c, uint32_t a, uint32_t b, uint32_t d, uint32_t e)
{
    c->game_data[0] = a;
    c->game_data[1] = b;
    c->game_data[2] = d;
    c->game_data[3] = e;
    return 1;
}

int __cdecl DPGetGameData(Ctx *c, uint32_t *a, uint32_t *b, uint32_t *d, uint32_t *e)
{
    *a = c->game_data[0];
    *b = c->game_data[1];
    *d = c->game_data[2];
    *e = c->game_data[3];
    return 1;
}

/* Settings the game asks for at startup (original: from a lobby, else Dominion.ini) */
int __cdecl DPGetStartUpInfo(Ctx *c, int what, unsigned str)
{
    (void)c;
    (void)str;
    switch (what) {
    case 1: return (int)ini_int("MultiMen", 300);
    case 2: return (int)ini_int("MultiMat", 12000);
    case 3: return 1;
    case 4: return 1;
    case 5: return 0;
    case 6: return 1;
    case 7: return (int)ini_int("NumStartUnits", 10);
    case 8: return 0;
    default: return -1;
    }
}

/* The connection types for the menu: only TCP/IP. One message per type:
 * {msg, 2, major version, minor version}, name in string str. Returns the count. */
int __cdecl DPEnumProviders(Ctx *c, unsigned str, uint32_t msg)
{
    char *s = RtGetString(c->app, str);
    uint32_t a[2] = {6, 0};
    if (!s)
        return 0;
    np_strncpy(s, PROVIDER_NAME, 0x29);
    s[0x29] = 0;
    post(c, msg, 2, a, 2);
    return 1;
}

/* Pick a connection type. Returns its kind of address: 1 = internet address, which
 * makes the menu show its own "Enter host address" box (then DPInitINetAddress).
 * original: 4 with the DirectPlay 6 files in the game folder, which have no such box
 * and showed DirectPlay's own dialog instead. */
int __cdecl DPInit(Ctx *c, unsigned name_str, int index, uint32_t max_players)
{
    const char *name = RtGetString(c->app, name_str);
    c->max_players = max_players;
    net_close();
    c->provider = 0;
    if (name && name[0] ? np_strcmp(name, PROVIDER_NAME) == 0 : index == 0)
        c->provider = 1;
    if (!c->provider)
        return 0;
    net_set_target("");
    return 1;
}

int __cdecl DPInitINetAddress(Ctx *c, unsigned str)
{
    const char *s = RtGetString(c->app, str);
    if (!s || !c->provider)
        return 0;
    np_debug("NETPLAY: address \"%s\"\n", s);
    net_set_target(s);
    return 1;
}

int __cdecl DPInitPhoneAddress(Ctx *c, unsigned number, unsigned modem)
{
    (void)c, (void)number, (void)modem;
    return 0; /* no modems */
}

int __cdecl DPInitSerialAddress(Ctx *c, int port, int baud, int stop, int parity, int flow,
                                unsigned str)
{
    (void)c, (void)port, (void)baud, (void)stop, (void)parity, (void)flow, (void)str;
    return 0; /* no serial cables */
}

void __cdecl DPDeInit(Ctx *c)
{
    if (!c->provider)
        return;
    if (c->my_id)
        DPDestroyPlayer(c);
    net_close();
    c->max_players = 0;
    c->have_slots = 0;
    c->my_id = 0;
    c->file_msg = 0;
    c->event = NULL;
    c->provider = 0;
}

int __cdecl DPClose(Ctx *c)
{
    if (!c->provider)
        return 0;
    net_close();
    return 1;
}

/* ---- sessions ---- */

static void post_sessions(Ctx *c, uint32_t msg, char *out, int short_names, int *count)
{
    for (int i = 0; i < c->n_sessions; i++) {
        Session *s = &c->sessions[i];
        uint32_t a[5];
        char num[16];
        if (s->gone)
            continue;
        np_debug("NETPLAY: Session %d, %s\n", c->n_sessions, s->info.name);
        np_strcpy(out, s->info.name);
        if (!short_names) {
            /* "name|max|current" */
            np_strcat(out, "|");
            wsprintfA(num, "%d", s->info.max_players);
            np_strcat(out, num);
            np_strcat(out, "|");
            wsprintfA(num, "%d", s->info.cur_players);
            np_strcat(out, num);
        } else {
            out[40] = 0;
        }
        a[0] = (uint32_t)i;
        a[1] = s->info.max_players;
        a[2] = s->info.cur_players;
        a[3] = (s->info.flags & DPSESSION_JOINDISABLED) || s->info.max_players == s->info.cur_players;
        a[4] = (uint32_t)s->version_ok;
        (*count)++;
        if (!post(c, msg, 5, a, 5))
            return;
    }
}

/* Looks for sessions and sends the menu one message per session:
 * {msg, 5, index, max players, players, full, same version}, text in string str.
 * Searching never waits: each call asks again and lists what has answered. */
int __cdecl DPEnumSessions(Ctx *c, unsigned str, int timeout, uint32_t msg, int short_names)
{
    NetSessionInfo found[MAX_SESS];
    int n, count = 0;
    (void)timeout;
    /* Not while in a session: the menu's search timer can fire once more after joining,
     * and it joins any session it is shown. */
    if (!c->provider || net_in_session())
        return 0;
    c->enum_string = RtGetString(c->app, str);
    if (!c->enum_string)
        return 0;
    for (int i = 0; i < c->n_sessions; i++)
        c->sessions[i].seen = 0;
    n = net_enum_sessions(found, MAX_SESS);
    for (int k = 0; k < n; k++) {
        NetSessionInfo *f = &found[k];
        Session *s = NULL;
        if (!f->name[0])
            continue;
        for (int i = 0; i < c->n_sessions && !s; i++)
            if (!memcmp(c->sessions[i].info.guid, f->guid, 16))
                s = &c->sessions[i];
        for (int i = 0; i < c->n_sessions && !s; i++)
            if (c->sessions[i].gone)
                s = &c->sessions[i];
        if (!s && c->n_sessions < MAX_SESS)
            s = &c->sessions[c->n_sessions++];
        if (!s)
            break;
        s->info = *f;
        s->seen = 1;
        s->gone = 0;
        s->version_ok = f->user[0] == c->game_data[0] || ini_int("DisableISOCheck", 0);
    }
    for (int i = 0; i < c->n_sessions; i++)
        if (!c->sessions[i].seen)
            c->sessions[i].gone = 1;
    post_sessions(c, msg, c->enum_string, short_names, &count);
    return count;
}

/* Names of the players in a session that we have not joined: one message per player,
 * {msg, 2, id, count}; short name in short_str, long name in long_str. */
int __cdecl DPEnumPlayersInSession(Ctx *c, int index, unsigned short_str, unsigned long_str,
                                   uint32_t msg)
{
    char *sn, *ln;
    int count = 0;
    if (!c->provider || index < 0 || index >= c->n_sessions)
        return 0;
    sn = RtGetString(c->app, short_str);
    ln = RtGetString(c->app, long_str);
    if (!sn || !ln)
        return 0;
    for (int i = 0; i < c->sessions[index].info.n_players; i++) {
        NetSessionInfo *s = &c->sessions[index].info;
        uint32_t a[2];
        np_strncpy(sn, s->player_short[i], 20);
        sn[20] = 0;
        np_strncpy(ln, s->player_long[i], 20);
        ln[20] = 0;
        count++;
        if (msg) {
            a[0] = s->player_id[i];
            a[1] = (uint32_t)count;
            post(c, msg, 2, a, 2);
        }
    }
    return count;
}

int __cdecl DPJoinSession(Ctx *c, int index, unsigned password_str)
{
    const char *pw = RtGetString(c->app, password_str);
    Session *s;
    np_debug("NETPLAY: Attempting to join session %d\n", index);
    if (index < 0 || index >= c->n_sessions)
        return 0;
    s = &c->sessions[index];
    memset(c->slots, 0, sizeof c->slots);
    c->n_slots = 0;
    c->have_slots = 1;
    c->is_host = 0;
    if (net_join_session(&s->info, pw ? pw : "") != NET_OK) {
        np_debug("NETPLAY: join failed\n");
        return 0;
    }
    memcpy(c->game_data, s->info.user, sizeof c->game_data);
    c->sess_flags = s->info.flags;
    c->sess_max = s->info.max_players;
    c->recv_bytes = c->recv_msgs = c->send_bytes = c->send_msgs = 0;
    c->n_sessions = 0;
    c->host_slot = 1;
    return 1;
}

/* Creates a session named "name|host_info|our IP" (name cut to 40 characters). */
int __cdecl DPCreateSession(Ctx *c, unsigned name_str, unsigned password_str, unsigned info_str)
{
    char *name = RtGetString(c->app, name_str);
    const char *pw = RtGetString(c->app, password_str);
    const char *info = RtGetString(c->app, info_str);
    char ip[20];
    uint32_t a;
    if (!name)
        return 0;
    if (np_strlen(name) > 0x29)
        name[40] = 0;
    np_strcat(name, "|");
    np_strcat(name, info ? info : "");
    np_strcat(name, "|");
    a = net_local_ip();
    if (a)
        net_ip_string(a, ip);
    else
        np_strcpy(ip, "NO_ADDRESS");
    np_strcat(name, ip);
    if (!c->provider)
        return 0;
    c->sess_flags = 0x40; /* DPSESSION_KEEPALIVE */
    c->sess_max = c->max_players;
    if (net_create_session(name, pw ? pw : "", c->sess_flags, c->sess_max, c->game_data) != NET_OK)
        return 0;
    c->n_sessions = 0;
    memset(c->slots, 0, sizeof c->slots);
    c->n_slots = 0;
    c->have_slots = 1;
    c->is_host = 1;
    net_enable_new_players(0);
    c->host_slot = 1;
    return 1;
}

int __cdecl DPEnableNewPlayers(Ctx *c, int on)
{
    if (!net_in_session())
        return 0;
    (void)c;
    net_enable_new_players(on);
    return 1;
}

/* Host: drop players above the new maximum, then set it. */
static int set_max(Ctx *c, uint32_t max, int lock, int count_by_name)
{
    if (!c->is_host)
        return 0;
    if (max < c->max_players) {
        int over = 0;
        uint32_t n = 0;
        for (uint32_t i = 0; i < 8; i++) {
            if (over && c->slots[i].id) {
                DPDestroyPlayerRemote(c, i);
            } else {
                /* original: DPSetMaxPlayers counts named slots, DPLockGame used ones,
                 * and both pass the 0-based index to DPDestroyPlayerRemote */
                if (count_by_name ? c->slots[i].name[0] != 0 : c->slots[i].id != 0)
                    n++;
                if (count_by_name ? max <= n : n == max)
                    over = 1;
            }
        }
    }
    c->sess_max = max;
    if (lock)
        c->sess_flags |= DPSESSION_NEWPLAYERSDISABLED | DPSESSION_JOINDISABLED;
    return net_set_session(c->sess_flags, c->sess_max) == NET_OK;
}

int __cdecl DPSetMaxPlayers(Ctx *c, uint32_t max)
{
    return set_max(c, max, 0, 1);
}

/* Host: no one else may join. */
int __cdecl DPLockGame(Ctx *c, uint32_t max)
{
    return set_max(c, max, 1, 0);
}

/* ---- players ---- */

/* Creates our player. The host gets slot 1 at once and returns it. A joining player
 * learns the slots of the others and returns -1; its own slot comes later
 * (MSG_YOUR_SLOT, reported as message type 6). */
int __cdecl DPCreatePlayer(Ctx *c, unsigned name_str, unsigned long_str)
{
    char *name = RtGetString(c->app, name_str);
    const char *lname = RtGetString(c->app, long_str);
    uint32_t data[2] = {0xffffffff, 0};
    uint32_t id;
    np_debug("NETPLAY: DPCreatePlayer\n");
    if (!name || !lname || !c->provider)
        return 0;
    if (!c->event)
        c->event = CreateEventA(NULL, FALSE, FALSE, NULL); /* original: named "DominionNet" */
    if (net_create_player(name, lname, c->event, data, sizeof data, &id) != NET_OK)
        return 0;
    c->max_buf = MAX_BUF;
    if (!c->recv_buf)
        c->recv_buf = np_alloc(BUF_SIZE);
    if (!c->send_buf)
        c->send_buf = np_alloc(BUF_SIZE);
    c->my_id = id;
    if (!c->send_lock_ready) {
        InitializeCriticalSection(&c->send_lock);
        c->send_lock_ready = 1;
    }
    if (c->is_host) {
        np_debug("NETPLAY: Creating Host Player\n");
        c->my_slot = add_player(c, id, name);
        data[0] = c->my_slot;
        data[1] = 0;
        net_set_player_data(id, data, sizeof data);
        if (c->my_slot)
            c->slots[c->my_slot - 1].latency = 0;
        DPEnableNewPlayers(c, 1);
        return (int)c->my_slot;
    } else {
        uint32_t ids[NET_MAX_PLAYERS];
        int n = net_enum_players(ids, NET_MAX_PLAYERS, 1);
        for (int i = 0; i < n; i++) {
            uint32_t d[2] = {0, 0}, size = sizeof d;
            char pname[24];
            if (net_get_player_data(ids[i], d, &size) != NET_OK || (int32_t)d[0] < 1 ||
                d[0] > MAX_SLOTS || net_get_player_name(ids[i], pname, 21, NULL, 0) != NET_OK)
                continue;
            c->slots[d[0] - 1].id = ids[i];
            np_strncpy(c->slots[d[0] - 1].name, pname, 20);
            c->slots[d[0] - 1].name[20] = 0;
            if (c->n_slots < d[0])
                c->n_slots = d[0];
            c->slots[d[0] - 1].latency = d[1];
        }
        return -1;
    }
}

/* Destroys our player and leaves the session. */
int __cdecl DPDestroyPlayer(Ctx *c)
{
    if (!c->provider || net_destroy_player(c->my_id) != NET_OK)
        return 0;
    after_leave(c);
    return 1;
}

/* Slot number "slot" left. If it was the host slot, the first used slot becomes the
 * host. If it was us, leave. */
int __cdecl DPDisconnectPlayer(Ctx *c, uint32_t slot)
{
    if (!c->provider || !c->have_slots || slot < 1 || slot > MAX_SLOTS)
        return 0;
    if (c->host_slot == slot) {
        for (uint32_t i = 0; i < c->n_slots; i++) {
            if (c->slots[i].id) {
                /* original: may pick the leaving slot itself */
                if (c->slots[i].id == c->my_id)
                    c->is_host = 1;
                c->host_slot = i + 1;
                break;
            }
        }
    }
    if (c->slots[slot - 1].id == c->my_id && net_destroy_player(c->my_id) == NET_OK) {
        after_leave(c);
        return 1;
    }
    return 0;
}

/* Host: tell player "slot" (0 = everyone else) to leave. */
int __cdecl DPDestroyPlayerRemote(Ctx *c, uint32_t slot)
{
    if (!c->have_slots || !c->is_host)
        return 0;
    if (slot == 0) {
        for (uint32_t i = 0; i < 8; i++)
            if (c->slots[i].id && c->slots[i].id != c->my_id)
                DPSendMsg(c, i + 1, 1, MSG_DESTROY, NULL, 0);
        return 1;
    }
    np_debug("NETPLAY: DPDestroyPlayerRemote(%d)\n", slot);
    if (slot <= MAX_SLOTS && c->slots[slot - 1].id != c->my_id)
        return DPSendMsg(c, slot, 1, MSG_DESTROY, NULL, 0);
    return 0;
}

/* The same, sent by a client that takes over as host when the host slot leaves. */
int __cdecl DPDestroyPlayerRemoteClient(Ctx *c, uint32_t slot)
{
    int host = c->is_host;
    if (!c->have_slots)
        return 0;
    if (c->host_slot == slot) {
        for (uint32_t i = 0; i < c->n_slots; i++) {
            if (i + 1 != slot && c->slots[i].id) {
                if (c->slots[i].id == c->my_id)
                    host = 1;
                break;
            }
        }
    }
    if (host && slot != 0 && slot <= MAX_SLOTS && c->slots[slot - 1].id != c->my_id)
        return DPSendMsg(c, slot, 1, MSG_DESTROY, NULL, 0);
    return 0;
}

/* Tell everyone that player "slot" is gone (they call DPDisconnectPlayer).
 * Returns 0 when the message was sent. original: inverted result. */
int __cdecl DPDisconnectPlayerRemote(Ctx *c, uint32_t slot)
{
    int host = c->is_host;
    uint32_t msg[2] = {MSG_DISCONNECT, slot};
    if (!c->have_slots)
        return 0;
    if (c->host_slot == slot) {
        for (uint32_t i = 0; i < c->n_slots; i++) {
            if (i + 1 != slot && c->slots[i].id) {
                if (c->slots[i].id == c->my_id)
                    host = 1;
                break;
            }
        }
    }
    if (!host)
        return 1;
    np_debug("NETPLAY: DPDisconnectPlayer(%d)\n", slot);
    return send_raw(c, 0, msg, sizeof msg) == NET_OK ? 0 : -1;
}

/* Host: give an existing DirectPlay player a slot by hand. */
int __cdecl DPAddPlayerDPID(Ctx *c, uint32_t id)
{
    char name[24];
    uint32_t msg[3], slot;
    np_debug("NETPLAY: DPAddPlayerDPID(%d)\n", id);
    for (uint32_t i = 0; i < c->n_slots; i++) {
        if (c->slots[i].id) {
            msg[0] = MSG_SLOT;
            msg[1] = i + 1;
            msg[2] = c->slots[i].id;
            send_raw(c, id, msg, 12);
        }
    }
    if (net_get_player_name(id, name, 21, NULL, 0) != NET_OK)
        return 0;
    slot = add_player(c, id, name);
    msg[0] = MSG_SLOT;
    msg[1] = slot;
    msg[2] = id;
    send_raw(c, 0, msg, 12);
    msg[1] = 0xffffffff;
    if (send_raw(c, 0, msg, 12) != NET_OK)
        return 0;
    return (int)slot;
}

int __cdecl DPGetNumPlayers(Ctx *c)
{
    int n = 0;
    for (uint32_t i = 0; i < c->n_slots; i++)
        n += c->slots[i].id != 0;
    return n;
}

int __cdecl DPGetMaxPlayers(Ctx *c)
{
    return (int)c->max_players;
}

/* Name of slot "slot" into string str and its id into *id. */
int __cdecl DPGetPlayerInfo(Ctx *c, uint32_t slot, uint32_t *id, unsigned str)
{
    char *s;
    if (slot > c->n_slots || slot == 0)
        return 0;
    s = RtGetString(c->app, str);
    if (!s)
        return 0;
    *id = c->slots[slot - 1].id;
    np_strcpy(s, c->slots[slot - 1].name);
    return 1;
}

int __cdecl DPGetPlayerIdx(Ctx *c)
{
    return (int)id_slot(c, c->my_id);
}

/* For every used slot: {msg, 2, id, slot}, name in string name_str. */
int __cdecl DPEnumPlayers(Ctx *c, unsigned name_str, unsigned long_str, uint32_t msg)
{
    char *name, *lname;
    int count = 0;
    if (!c->provider)
        return 0;
    name = RtGetString(c->app, name_str);
    lname = RtGetString(c->app, long_str);
    if (!name || !lname)
        return 0;
    for (uint32_t i = 0; i < c->max_players && i < MAX_SLOTS; i++) {
        if (c->slots[i].id) {
            uint32_t a[2] = {c->slots[i].id, i + 1};
            np_strcpy(name, c->slots[i].name);
            lname[0] = 0;
            count++;
            if (!post(c, msg, 2, a, 2))
                return count;
        }
    }
    return count;
}

int __cdecl DPPlayerIsHost(Ctx *c)
{
    return c->is_host;
}

int __cdecl DPGetHostPlayer(Ctx *c)
{
    return (int)c->host_slot;
}

HANDLE __cdecl DPGetPlayerEvent(Ctx *c)
{
    return c->event;
}

/* ---- addresses and the clipboard ---- */

/* This machine's address as text into string str (if given) and as a number in *ip. */
int __cdecl DPGetSystemAddress(Ctx *c, unsigned str, uint32_t *ip)
{
    char *s = RtGetString(c->app, str);
    *ip = net_local_ip();
    if (s) {
        if (*ip)
            net_ip_string(*ip, s);
        else
            np_strcpy(s, "NO_ADDRESS");
    }
    return 0;
}

/* Address of slot "slot" (0 = us) into string str.
 * original: DirectPlay's binary address block; ours is "a.b.c.d". */
int __cdecl DPGetPlayerAddress(Ctx *c, uint32_t slot, unsigned str)
{
    char *s = RtGetString(c->app, str);
    uint32_t ip;
    if (!s || net_get_player_ip(slot ? slot_id(c, slot) : c->my_id, &ip) != NET_OK)
        return 0;
    net_ip_string(ip, s);
    return 1;
}

int __cdecl DPCopyHostAddress(Ctx *c)
{
    char text[32];
    uint32_t ip = 0;
    HGLOBAL h;
    DPGetSystemAddress(c, 0, &ip);
    if (ip)
        net_ip_string(ip, text);
    else
        np_strcpy(text, "NO_ADDRESS");
    h = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, np_strlen(text) + 1);
    if (h) {
        char *p = GlobalLock(h);
        np_strcpy(p, text);
        GlobalUnlock(h);
        OpenClipboard(RtGetWindow(c->app));
        SetClipboardData(CF_TEXT, h);
        CloseClipboard();
    }
    return 0;
}

int __cdecl DPPasteString(Ctx *c, unsigned str)
{
    char *s = RtGetString(c->app, str);
    HANDLE h;
    if (!s)
        return 0;
    s[0] = 0;
    OpenClipboard(RtGetWindow(c->app));
    h = GetClipboardData(CF_TEXT);
    if (h) {
        const char *p = GlobalLock(h);
        if (p) {
            np_strncpy(s, p, 0x1ff); /* original: unlimited copy */
            s[0x1ff] = 0;
        }
        GlobalUnlock(h);
    }
    CloseClipboard();
    return 0;
}

/* ---- messages ---- */

/* Sends {type, data} to slot "slot" (0 = everyone else). Returns 1 on success. */
int __cdecl DPSendMsg(Ctx *c, uint32_t slot, uint32_t flags, uint32_t type, const void *data,
                      uint32_t size)
{
    uint32_t to = slot_id(c, slot);
    int r;
    (void)flags; /* always "guaranteed" (TCP) */
    if (slot == 0)
        to = 0;
    else if (to == 0)
        return 0;
    if (!c->send_buf || size > BUF_SIZE - 4)
        return 0;
    EnterCriticalSection(&c->send_lock);
    c->send_bytes += size + 4;
    c->send_msgs++;
    memcpy(c->send_buf, &type, 4);
    if (data)
        memcpy(c->send_buf + 4, data, size);
    r = send_raw(c, to, c->send_buf, size + 4);
    LeaveCriticalSection(&c->send_lock);
    return r == NET_OK;
}

/* For the menu script. Gets the next message into the module's buffer (read it with
 * DPGetTransferBuf / DPCopyTransferBuf). *is_sys: 1 = system message; *slot: sender
 * (in: only from this slot, 0 = any); *type: message type. Returns 1 if there was one.
 * If copy is set, also copies the message (without its type) to out. */
int __cdecl DPReceiveMsg(Ctx *c, int *is_sys, uint32_t *slot, uint32_t *type, void *out,
                         uint32_t *out_size, int copy)
{
    uint32_t from, size = c->max_buf, *m, t;
    if (!c->my_id)
        return 0;
    if (receive(c, &from, c->recv_buf, &size, *slot) != NET_OK)
        return 0;
    c->recv_bytes += size;
    c->recv_msgs++;
    m = (uint32_t *)c->recv_buf;
    if (from == 0) {
        if (m[0] == DPSYS_CREATEPLAYERORGROUP) {
            if (c->is_host) {
                uint32_t s = give_slot(c, m);
                if (s) {
                    /* original: *is_sys is left as it was */
                    *type = m[0];
                    m[2] = s;
                    *slot = s;
                    return 1;
                }
            }
            if (copy) {
                uint32_t n = *out_size < 0x30 ? *out_size : 0x30;
                memcpy(out, c->recv_buf + 4, n);
            }
            *type = m[0];
        } else if (m[0] == DPSYS_DESTROYPLAYERORGROUP) {
            uint32_t s = player_left(c, m[2]);
            m[2] = s;
            if (copy) {
                uint32_t n = *out_size < 0x34 ? *out_size : 0x34;
                memcpy(out, c->recv_buf + 4, n);
            }
            *type = m[0];
            *slot = s;
        } else if (m[0] == DPSYS_SESSIONLOST) {
            *type = DPSYS_SESSIONLOST;
        } else {
            if (copy) {
                memcpy(out, c->recv_buf + 4, *out_size);
                *out_size = size;
            }
            *slot = id_slot(c, from);
            *type = m[0];
        }
        *is_sys = 1;
        return 1;
    }
    *is_sys = 0;
    *slot = id_slot(c, from);
    t = m[0];
    if (t >= MSG_FILE && t <= MSG_DISCONNECT && t != 5) {
        if (!handle_own(c, from, m, 0))
            return 0;
        /* original: these sizes were written even without a size pointer */
        if (t == MSG_FILE) {
            if (copy)
                memcpy(out, c->recv_buf + 4, sizeof(FileChunk));
            if (out_size)
                *out_size = sizeof(FileChunk);
        } else if (t == MSG_DESTROY) {
            if (out_size)
                *out_size = 0;
        } else if (t == MSG_SLOT && copy) {
            uint32_t n = *out_size < 12 ? *out_size : 12;
            memcpy(out, c->recv_buf + 4, n);
        }
    } else {
        if (copy)
            memcpy(out, c->recv_buf + 4, *out_size);
        if (out_size)
            *out_size = size;
    }
    *type = t;
    return 1;
}

/* For the game's network thread: like DPReceiveMsg, but into the caller's buffer
 * (type included) and *size gets the message size. */
int __cdecl DPReceiveMsgDirect(Ctx *c, int *is_sys, uint32_t *slot, uint32_t *buf,
                               uint32_t *size)
{
    uint32_t from, n = c->max_buf;
    int r;
    if (!c->my_id)
        return 0;
    r = receive(c, &from, (uint8_t *)buf, &n, *slot);
    if (r == NET_OK) {
        c->recv_bytes += n;
        c->recv_msgs++;
    }
    *size = n;
    if (r != NET_OK)
        return 0;
    if (from == 0) {
        if (buf[0] == DPSYS_CREATEPLAYERORGROUP) {
            if (c->is_host) {
                uint32_t s = give_slot(c, buf);
                if (s) {
                    buf[2] = s;
                    *slot = s;
                    return 1;
                }
            }
        } else if (buf[0] == DPSYS_DESTROYPLAYERORGROUP) {
            uint32_t s = player_left(c, buf[2]);
            buf[2] = s;
            *slot = s;
        } else if (buf[0] != DPSYS_SESSIONLOST) {
            *slot = id_slot(c, 0);
        }
        *is_sys = 1;
        return 1;
    }
    *is_sys = 0;
    *slot = id_slot(c, from);
    if (buf[0] >= MSG_FILE && buf[0] <= MSG_DISCONNECT && buf[0] != 5)
        return handle_own(c, from, buf, 1);
    return 1;
}

uint8_t *__cdecl DPGetTransferBuf(Ctx *c)
{
    return c->recv_buf + 4;
}

void __cdecl DPCopyTransferBuf(Ctx *c, void *dst, uint32_t size)
{
    memcpy(dst, c->recv_buf + 4, size < c->max_buf ? size : c->max_buf);
}

void __cdecl DPCopyTransferString(Ctx *c, unsigned str, uint32_t size)
{
    char *s = RtGetString(c->app, str);
    if (!s)
        return;
    if (size > 0x1ff)
        size = 0x200;
    memcpy(s, c->recv_buf + 4, size < c->max_buf ? size : c->max_buf);
}

void __cdecl DPStrToBuf(Ctx *c, void *dst, unsigned str, uint32_t size)
{
    const char *s = RtGetString(c->app, str);
    if (!s)
        return;
    if (size > 0x1ff)
        size = 0x200;
    memcpy(dst, s, size);
}

void __cdecl DPBufToStr(Ctx *c, unsigned str, const void *src, uint32_t size)
{
    char *s = RtGetString(c->app, str);
    if (!s)
        return;
    if (size > 0x1ff)
        size = 0x200;
    memcpy(s, src, size);
}

uint32_t __cdecl DPGetMaxBufLen(Ctx *c)
{
    return c->max_buf;
}

uint32_t __cdecl DPGetSendBytes(Ctx *c) { return c->send_bytes; }
uint32_t __cdecl DPGetReceiveBytes(Ctx *c) { return c->recv_bytes; }
uint32_t __cdecl DPGetSendMsgs(Ctx *c) { return c->send_msgs; }
uint32_t __cdecl DPGetReceiveMsgs(Ctx *c) { return c->recv_msgs; }

DWORD __cdecl DPGetTime(Ctx *c)
{
    (void)c;
    return timeGetTime();
}

/* A thread that waits for messages and calls proc(ctx, is_sys, slot, type) for each. */
static DWORD WINAPI msg_thread(LPVOID p)
{
    Ctx *c = p;
    for (;;) {
        if (c->event)
            WaitForSingleObject(c->event, INFINITE);
        else
            Sleep(10);
        for (;;) {
            int is_sys = 0;
            uint32_t slot = 0, type = 0, size = 0xfc;
            if (!DPReceiveMsg(c, &is_sys, &slot, &type, NULL, &size, 0))
                break;
            c->msg_proc(c->msg_proc_ctx, (unsigned)is_sys, slot, type);
        }
    }
    return 0;
}

unsigned __cdecl DPInitMsgProcess(Ctx *c, void *proc_ctx,
                                  int(__stdcall *proc)(void *, unsigned, unsigned, unsigned))
{
    c->msg_proc = proc;
    c->msg_proc_ctx = proc_ctx;
    c->msg_thread = CreateThread(NULL, 0, msg_thread, c, 0, NULL);
    return 1;
}

/* ---- latency ---- */

/* Measures the latency to every player: "rounds" pings each. When a player's last ping
 * is back, message msg is run. */
int __cdecl DPSyncPlayer(Ctx *c, uint32_t rounds, uint32_t msg)
{
    uint32_t ping[6];
    if (rounds == 0 || !c->provider)
        return 0;
    for (uint32_t i = 0; i < c->n_slots; i++)
        if (c->slots[i].id)
            c->slots[i].latency_sum = 0;
    ping[0] = MSG_PING;
    ping[1] = c->my_id;
    ping[2] = rounds;
    ping[3] = 0;
    ping[4] = timeGetTime();
    ping[5] = msg;
    return send_raw(c, 0, ping, sizeof ping) == NET_OK;
}

uint32_t __cdecl DPGetMaxLatency(Ctx *c)
{
    return c->max_latency;
}

uint32_t __cdecl DPGetPlayerLatency(Ctx *c, uint32_t slot)
{
    if (slot == 0 || slot > MAX_SLOTS)
        return 0;
    return c->slots[slot - 1].latency;
}

int __cdecl DPSetPlayerLatency(Ctx *c, uint32_t slot, uint32_t latency)
{
    if (slot == 0 || slot > c->n_slots || c->slots[slot - 1].id == 0)
        return 0;
    c->slots[slot - 1].latency = latency;
    return 1;
}

/* ---- file transfer ---- */

/* Sends a file (a custom map) to slot "slot". The receiver stores it in MapsDir.
 * *chunks gets the number of pieces; message msg reports progress on both sides. */
int __cdecl DPSendFile(Ctx *c, uint32_t slot, unsigned path_str, uint32_t *chunks, uint32_t msg)
{
    const char *path = RtGetString(c->app, path_str);
    const char *name;
    FileChunk h;
    uint32_t to, name_len, room;
    DWORD size, got;
    if (!path || !c->send_buf)
        return 0;
    name = path + np_strlen(path);
    while (name > path && name[-1] != '\\')
        name--;
    if (c->file_refs == 0) {
        c->file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              NULL);
        if (c->file == INVALID_HANDLE_VALUE)
            c->file = NULL;
    }
    if (!c->file)
        return 0;
    c->file_refs++;
    memset(&h, 0, sizeof h);
    GetFileTime(c->file, &h.created, &h.accessed, &h.written);
    size = GetFileSize(c->file, NULL);
    to = slot_id(c, slot);
    name_len = (uint32_t)np_strlen(name) + 1;
    room = c->max_buf - sizeof h;
    h.type = MSG_FILE;
    h.chunks = (size - (room + name_len)) / room + 2; /* original formula */
    h.size = room - name_len;
    if (h.chunks == 1)
        h.size = size;
    h.data = 1;
    h.msg = msg;
    memcpy(c->send_buf + sizeof h, name, name_len);
    SetFilePointer(c->file, 0, NULL, FILE_BEGIN);
    ReadFile(c->file, c->send_buf + sizeof h + name_len, h.size, &got, NULL);
    if (got < h.size) {
        h.size = got;
        h.chunks = 1;
        h.last = 1;
    }
    memcpy(c->send_buf, &h, sizeof h);
    if (send_raw(c, to, c->send_buf, h.size + name_len + sizeof h) != NET_OK)
        return 0;
    c->file_msg = msg;
    *chunks = h.chunks;
    return 1;
}

int __cdecl DPStopSendFile(Ctx *c)
{
    close_file(c);
    return 1;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved)
{
    (void)h, (void)reason, (void)reserved;
    return TRUE;
}
