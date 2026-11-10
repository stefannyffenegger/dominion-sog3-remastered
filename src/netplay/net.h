/* Our replacement for the part of DirectPlay that NETPLAY.R32 used.
 *
 * Model (the same as a DirectPlay peer-to-peer session):
 *  - One game creates a session and is its host. Others find it on the LAN (UDP
 *    broadcast) or at an address, and join over TCP.
 *  - Every game is connected to every other game. A message goes straight from the
 *    sender to the receiver, so messages between two players arrive in the order sent.
 *  - The host only admits new games and owns the session description. If it leaves,
 *    the others keep playing, but nobody can join any more (no host migration).
 *  - Players have ids. Changes to a player (created, destroyed, new data) are sent by
 *    the game that owns the player, and the others get DirectPlay-style system
 *    messages for them.
 * All functions are thread-safe. */
#ifndef DOMINION_NET_H
#define DOMINION_NET_H

#include <stdint.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#define NET_DEFAULT_PORT 47624 /* DirectPlay's port, so old firewall rules still apply */
#define NET_NAME_LEN 64
#define NET_MAX_PLAYERS 16
#define NET_MAX_PDATA 64
#define NET_LIST_PLAYERS 8 /* player names sent with a session's LAN answer */

/* Results, like DirectPlay's (0 = OK) */
#define NET_OK 0
#define NET_ERR (-1)
#define NET_NOMESSAGES 1

/* DirectPlay system message types and session flags the game sees */
#define DPSYS_CREATEPLAYERORGROUP 0x0003
#define DPSYS_DESTROYPLAYERORGROUP 0x0005
#define DPSYS_SESSIONLOST 0x0031
#define DPSYS_SETPLAYERORGROUPDATA 0x0102
#define DPSYS_SETSESSIONDESC 0x0104
#define DPPLAYERTYPE_PLAYER 1
#define DPSESSION_NEWPLAYERSDISABLED 0x01
#define DPSESSION_JOINDISABLED 0x20

/* DirectPlay receive flags */
#define NET_RECEIVE_ALL 1
#define NET_RECEIVE_TOPLAYER 2
#define NET_RECEIVE_FROMPLAYER 4

typedef struct NetSessionInfo {
    uint8_t guid[16];
    uint32_t flags, max_players, cur_players;
    uint32_t user[4];
    char name[128];
    uint32_t ip;   /* network byte order */
    uint16_t port; /* host byte order */
    DWORD seen;    /* GetTickCount() of the last answer */
    int n_players;
    uint32_t player_id[NET_LIST_PLAYERS];
    char player_short[NET_LIST_PLAYERS][21];
    char player_long[NET_LIST_PLAYERS][21];
} NetSessionInfo;

int net_init(void);
void net_shutdown(void);

/* Sessions */
void net_set_target(const char *address); /* "" = search the LAN; "host[:port]" */
int net_enum_sessions(NetSessionInfo *out, int max);
int net_create_session(const char *name, const char *password, uint32_t flags,
                       uint32_t max_players, const uint32_t user[4]);
int net_join_session(const NetSessionInfo *s, const char *password);
void net_close(void);
int net_in_session(void);
void net_enable_new_players(int on);
int net_get_session(uint32_t *flags, uint32_t *max_players);
int net_set_session(uint32_t flags, uint32_t max_players); /* host only */

/* Players */
int net_create_player(const char *short_name, const char *long_name, HANDLE event,
                      const void *data, uint32_t size, uint32_t *id);
int net_destroy_player(uint32_t id);
int net_get_player_name(uint32_t id, char *short_name, int short_size, char *long_name,
                        int long_size);
int net_get_player_data(uint32_t id, void *data, uint32_t *size);
int net_set_player_data(uint32_t id, const void *data, uint32_t size);
int net_get_player_ip(uint32_t id, uint32_t *ip);
int net_enum_players(uint32_t *ids, int max, int remote_only);

/* Messages. to = 0 sends to every player but the sender. */
int net_send(uint32_t from, uint32_t to, const void *data, uint32_t size);
/* Gets the oldest matching message. System messages come from id 0 and are built in
 * the DirectPlay DPMSG_* layouts. */
int net_receive(uint32_t *from, uint32_t *to, int flags, void *buf, uint32_t *size);

/* Address of this machine as the other players see it (network byte order), 0 if none */
uint32_t net_local_ip(void);
void net_ip_string(uint32_t ip, char *out); /* "a.b.c.d" */

void np_debug(const char *fmt, ...);
void *np_alloc(size_t n);
void np_free(void *p);
size_t np_strlen(const char *s);
void np_strncpy(char *dst, const char *src, size_t n); /* like strncpy */
void np_strcpy(char *dst, const char *src);
void np_strcat(char *dst, const char *src);
int np_strcmp(const char *a, const char *b);

#endif
