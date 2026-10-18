#include "net.h"
#include "raylib.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Little-endian packed wire structs (same binary on both ends; LAN only).
// ---------------------------------------------------------------------------

#define NET_MAGIC 0x4E474131u // "NGA1"
#define NET_RECV_BUF 1200

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t  type;
    uint8_t  from;
    uint16_t seq;
} PktHdr;

typedef struct __attribute__((packed)) {
    PktHdr h;
    char   name[16];
} PktHello;

typedef struct __attribute__((packed)) {
    PktHdr h;
    uint8_t id, spawn, count;
} PktWelcome;

typedef struct __attribute__((packed)) {
    PktHdr h;
    float  x, y, z, yaw, pitch;
    uint8_t health, weapon, flags, kills, deaths;
} PktState;

typedef struct __attribute__((packed)) {
    PktHdr h;
    uint8_t pcount;
    NetPlayerSnap players[NET_MAX_PLAYERS];
    uint8_t ecount;
    NetEnemySnap  enemies[MAX_ENEMIES];
    uint8_t barrelMask, pickupMask;
} PktSnapshot;

typedef struct __attribute__((packed)) {
    PktHdr h;
    uint8_t target, head, attacker;
    float   dmg, ax, ay, az;
} PktDamage;

typedef struct __attribute__((packed)) {
    PktHdr h;
    uint8_t idx;
    int8_t  zone;
    float   dmg, hx, hy, hz;
} PktEnemyHit;

typedef struct __attribute__((packed)) {
    PktHdr h;
    uint8_t killer, victim, kind, head;
} PktKill;

typedef struct __attribute__((packed)) {
    PktHdr h;
    uint8_t idx;
    float   x, z;
} PktBarrel;

typedef struct __attribute__((packed)) {
    PktHdr h;
    uint8_t idx;
    float   dmg;
} PktBarrelHit;

typedef struct __attribute__((packed)) {
    PktHdr h;
    float   x, y, z;
} PktNoise;

typedef struct __attribute__((packed)) {
    PktHdr h;
    uint8_t idx;
} PktPickup;

typedef struct __attribute__((packed)) {
    PktHdr h;
} PktBye;

// ---------------------------------------------------------------------------
// Config (set from main() before the game window exists)
// ---------------------------------------------------------------------------

static int  g_cfgMode = NET_OFF;
static char g_cfgAddr[64] = "";
static int  g_cfgPort = NET_PORT_DEFAULT;
static char g_cfgName[16] = "";

void Net_Configure(int mode, const char* addr, int port, const char* name) {
    g_cfgMode = mode;
    g_cfgAddr[0] = 0;
    if (addr && addr[0]) snprintf(g_cfgAddr, sizeof(g_cfgAddr), "%s", addr);
    g_cfgPort = (port > 0 && port < 65536) ? port : NET_PORT_DEFAULT;
    g_cfgName[0] = 0;
    if (name && name[0]) {
        snprintf(g_cfgName, sizeof(g_cfgName), "%s", name);
        for (char* c = g_cfgName; *c; c++) if (*c < 32 || *c > 126) *c = '?';
    }
}

// ---------------------------------------------------------------------------
// Spawns / colors / names
// ---------------------------------------------------------------------------

static const Vector3 kSpawns[NET_MAX_PLAYERS] = {
    { 0.0f, 0.0f, 12.0f },
    { -11.0f, 0.0f, -6.0f },
    { 10.0f, 0.0f, 22.0f },
    { 0.0f, 0.0f, -20.0f },
};

Vector3 Net_SpawnPos(int idx) {
    idx %= NET_MAX_PLAYERS;
    if (idx < 0) idx += NET_MAX_PLAYERS;
    return kSpawns[idx];
}

float Net_SpawnYaw(int idx) {
    Vector3 p = Net_SpawnPos(idx);
    return atan2f(-p.x, -p.z); // face the map center
}

Color Net_PlayerColor(int id) {
    switch (id) {
        case 1:  return (Color){ 90, 140, 235, 255 };  // blue (host)
        case 2:  return (Color){ 235, 90, 70, 255 };   // red
        case 3:  return (Color){ 90, 205, 110, 255 };  // green
        case 4:  return (Color){ 235, 200, 70, 255 };  // yellow
        default: return (Color){ 200, 200, 200, 255 };
    }
}

const char* Net_PeerName(const NetState* n, int id) {
    static char ring[4][20];
    static int rr = 0;
    char* b = ring[rr = (rr + 1) & 3];
    if (id == n->myId) { snprintf(b, 20, "YOU"); return b; }
    if (id <= 0) { snprintf(b, 20, "THE ENEMY"); return b; }
    if (id <= NET_MAX_PLAYERS && n->peers[id - 1].used && n->peers[id - 1].name[0])
        snprintf(b, 20, "%s", n->peers[id - 1].name);
    else snprintf(b, 20, "PLAYER %d", id);
    return b;
}

void Net_Feed(NetState* n, const char* text) {
    for (int i = NET_FEED_LINES - 1; i > 0; i--) n->feed[i] = n->feed[i - 1];
    snprintf(n->feed[0].text, sizeof(n->feed[0].text), "%s", text);
    n->feed[0].timer = 5.0f;
}

// ---------------------------------------------------------------------------
// Socket helpers
// ---------------------------------------------------------------------------

static void net_set_nonblock(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl >= 0) fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

static void net_send_raw(NetState* n, const struct sockaddr_in* to, const void* buf, int len) {
    if (n->sock <= 0) return;
    if (sendto(n->sock, buf, (size_t)len, 0, (const struct sockaddr*)to, sizeof(*to)) >= 0)
        n->packetsOut++;
}

static void net_send_hostaddr(NetState* n, const void* buf, int len) {
    net_send_raw(n, &n->host, buf, len);
}

static void net_send_peer(NetState* n, int id, const void* buf, int len) {
    if (id < 1 || id > NET_MAX_PLAYERS) return;
    const NetPeer* p = &n->peers[id - 1];
    if (!p->used || p->id == n->myId) return;
    net_send_raw(n, &p->addr, buf, len);
}

static void net_send_all(NetState* n, int exceptId, const void* buf, int len) {
    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        const NetPeer* p = &n->peers[i];
        if (!p->used || p->id == n->myId || p->id == exceptId) continue;
        net_send_raw(n, &p->addr, buf, len);
    }
}

static void net_fill_hdr(NetState* n, PktHdr* h, uint8_t type) {
    h->magic = NET_MAGIC;
    h->type = type;
    h->from = (uint8_t)n->myId;
    h->seq = ++n->seq;
}

// Events are idempotency-safe; sending twice (same bytes/seq) survives one
// lost datagram. Receivers drop duplicates by per-sender sequence.
static void net_send_event(NetState* n, const void* buf, int len, int onlyTo) {
    if (!n->active || n->sock <= 0) return;
    if (n->mode == NET_CLIENT) {
        net_send_hostaddr(n, buf, len);
        net_send_hostaddr(n, buf, len);
    } else if (n->mode == NET_HOST) {
        if (onlyTo > 0) {
            net_send_peer(n, onlyTo, buf, len);
            net_send_peer(n, onlyTo, buf, len);
        } else {
            net_send_all(n, 0, buf, len);
            net_send_all(n, 0, buf, len);
        }
    }
}

static void net_push_event(NetState* n, int type, Vector3 a, Vector3 b, int i0, int i1, int i2, float f0) {
    if (n->eventCount >= (int)(sizeof(n->events) / sizeof(n->events[0]))) return;
    NetEvent* e = &n->events[n->eventCount++];
    e->type = type; e->a = a; e->b = b; e->i0 = i0; e->i1 = i1; e->i2 = i2; e->f0 = f0;
}

static bool net_is_stale(NetState* n, int from, uint16_t seq) {
    if (from < 1 || from > NET_MAX_PLAYERS) return true;
    NetPeer* p = &n->peers[from - 1];
    if (p->lastSeq != 0 && (int16_t)(seq - p->lastSeq) <= 0) return true;
    p->lastSeq = seq;
    return false;
}

// ---------------------------------------------------------------------------
// Event senders (called from game.c)
// ---------------------------------------------------------------------------

void Net_SendEnemyHit(NetState* n, int enemyIdx, int zone, float dmg, Vector3 hitPos) {
    if (!n->active || n->mode != NET_CLIENT) return;
    PktEnemyHit pk;
    net_fill_hdr(n, &pk.h, P_ENEMY_HIT);
    pk.idx = (uint8_t)enemyIdx;
    pk.zone = (int8_t)zone;
    pk.dmg = dmg;
    pk.hx = hitPos.x; pk.hy = hitPos.y; pk.hz = hitPos.z;
    net_send_event(n, &pk, (int)sizeof(pk), 0);
}

void Net_SendDamageTo(NetState* n, int targetId, float dmg, bool head, Vector3 attackerPos, int attackerId) {
    if (!n->active || targetId == n->myId) return;
    PktDamage pk;
    net_fill_hdr(n, &pk.h, P_DAMAGE);
    pk.target = (uint8_t)targetId;
    pk.head = head ? 1 : 0;
    pk.attacker = (uint8_t)(attackerId < 0 ? 0 : attackerId);
    pk.dmg = dmg;
    pk.ax = attackerPos.x; pk.ay = attackerPos.y; pk.az = attackerPos.z;
    int only = (n->mode == NET_HOST) ? targetId : 0;
    net_send_event(n, &pk, (int)sizeof(pk), only);
}

void Net_SendBarrelHit(NetState* n, int barrelIdx, float dmg) {
    if (!n->active || n->mode != NET_CLIENT) return;
    PktBarrelHit pk;
    net_fill_hdr(n, &pk.h, P_BARREL_HIT);
    pk.idx = (uint8_t)barrelIdx;
    pk.dmg = dmg;
    net_send_event(n, &pk, (int)sizeof(pk), 0);
}

void Net_NotifyBarrel(NetState* n, int barrelIdx, Vector3 pos) {
    if (!n->active || n->mode != NET_HOST) return;
    PktBarrel pk;
    net_fill_hdr(n, &pk.h, P_BARREL);
    pk.idx = (uint8_t)barrelIdx;
    pk.x = pos.x; pk.z = pos.z;
    net_send_event(n, &pk, (int)sizeof(pk), 0);
}

static void net_feed_kill(NetState* n, int killer, int victim, int kind, bool head) {
    char t[sizeof(n->feed[0].text)];
    if (kind == 1) {
        if (killer == n->myId)
            snprintf(t, sizeof(t), "%sYOU KILLED AN ENEMY", head ? "HEADSHOT! " : "");
        else
            snprintf(t, sizeof(t), "%s %sKILLED AN ENEMY", Net_PeerName(n, killer), head ? "HEADSHOT " : "");
        Net_Feed(n, t);
        return;
    }
    if (victim == n->myId) {
        if (killer <= 0) snprintf(t, sizeof(t), "YOU DIED");
        else snprintf(t, sizeof(t), "YOU WERE KILLED BY %s", Net_PeerName(n, killer));
    } else if (killer == n->myId) {
        snprintf(t, sizeof(t), "%sYOU KILLED %s", head ? "HEADSHOT! " : "", Net_PeerName(n, victim));
    } else {
        snprintf(t, sizeof(t), "%s KILLED %s", Net_PeerName(n, killer), Net_PeerName(n, victim));
    }
    Net_Feed(n, t);
}

void Net_NotifyEnemyKilled(NetState* n, int killerId, bool head) {
    if (!n->active || n->mode != NET_HOST) return;
    net_feed_kill(n, killerId, -1, 1, head);
    PktKill pk;
    net_fill_hdr(n, &pk.h, P_KILL);
    pk.killer = (uint8_t)killerId;
    pk.victim = 0;
    pk.kind = 1;
    pk.head = head ? 1 : 0;
    net_send_event(n, &pk, (int)sizeof(pk), 0);
}

void Net_SendNoise(NetState* n, Vector3 pos) {
    if (!n->active || n->mode != NET_CLIENT) return;
    PktNoise pk;
    net_fill_hdr(n, &pk.h, P_NOISE);
    pk.x = pos.x; pk.y = pos.y; pk.z = pos.z;
    net_send_event(n, &pk, (int)sizeof(pk), 0);
}

void Net_RequestPickup(NetState* n, int idx) {
    if (!n->active || n->mode != NET_CLIENT) return;
    PktPickup pk;
    net_fill_hdr(n, &pk.h, P_PICKUP);
    pk.idx = (uint8_t)idx;
    net_send_event(n, &pk, (int)sizeof(pk), 0);
}

void Net_NoteLocalPickup(NetState* n, int idx) {
    if (idx >= 0 && idx < MAX_PICKUPS) n->pickupTaken[idx] = n->netTime;
}

void Net_SendKill(NetState* n, int killerId, int kind, bool head) {
    if (!n->active) return;
    net_feed_kill(n, killerId < 0 ? 0 : killerId, n->myId, kind, head); // own feed line
    PktKill pk;
    net_fill_hdr(n, &pk.h, P_KILL);
    pk.killer = (uint8_t)(killerId < 0 ? 0 : killerId);
    pk.victim = (uint8_t)n->myId;
    pk.kind = (uint8_t)kind;
    pk.head = head ? 1 : 0;
    net_send_event(n, &pk, (int)sizeof(pk), 0);
}

// ---------------------------------------------------------------------------
// Host bookkeeping
// ---------------------------------------------------------------------------

static int host_find_addr(NetState* n, const struct sockaddr_in* a) {
    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        const NetPeer* p = &n->peers[i];
        if (!p->used || p->id == n->myId) continue;
        if (p->addr.sin_addr.s_addr == a->sin_addr.s_addr && p->addr.sin_port == a->sin_port)
            return p->id;
    }
    return 0;
}

static int host_add_peer(NetState* n, const struct sockaddr_in* a, const char* name) {
    for (int id = 2; id <= NET_MAX_PLAYERS; id++) {
        NetPeer* p = &n->peers[id - 1];
        if (p->used) continue;
        memset(p, 0, sizeof(*p));
        p->id = id;
        p->used = true;
        p->addr = *a;
        snprintf(p->name, sizeof(p->name), "%s", name);
        p->lastRecv = n->netTime;
        p->health = (float)PLAYER_MAX_HEALTH;
        p->flags = NETF_ALIVE;
        p->haveSnap = false;
        p->lastSeq = 0;
        return id;
    }
    return 0;
}

static void host_send_welcome(NetState* n, const struct sockaddr_in* to, int id, int spawn) {
    PktWelcome pk;
    net_fill_hdr(n, &pk.h, P_WELCOME);
    pk.id = (uint8_t)id;
    pk.spawn = (uint8_t)spawn;
    int cnt = 1;
    for (int i = 0; i < NET_MAX_PLAYERS; i++)
        if (n->peers[i].used && n->peers[i].id != n->myId) cnt++;
    pk.count = (uint8_t)cnt;
    net_send_raw(n, to, &pk, (int)sizeof(pk));
    net_send_raw(n, to, &pk, (int)sizeof(pk)); // dup: idempotent, survives loss
}

static void host_send_snapshot(NetState* n, const NetLocalState* me,
                               const Enemy* enemies, int ec,
                               const Pickup* pickups, int pc,
                               const Barrel* barrels, int bc) {
    PktSnapshot pk;
    memset(&pk, 0, sizeof(pk));
    net_fill_hdr(n, &pk.h, P_SNAPSHOT);

    int idx = 0;
    NetPlayerSnap* s = &pk.players[idx++];
    s->id = (uint8_t)n->myId;
    s->health = (uint8_t)ClampF(me->health, 0, 255);
    s->weapon = (uint8_t)me->weapon;
    s->flags = (uint8_t)me->flags;
    s->kills = (uint8_t)(me->kills > 255 ? 255 : me->kills);
    s->deaths = (uint8_t)(me->deaths > 255 ? 255 : me->deaths);
    snprintf(s->name, sizeof(s->name), "%.11s", n->myName);
    s->x = me->pos.x; s->y = me->pos.y; s->z = me->pos.z;
    s->yaw = me->yaw; s->pitch = me->pitch;

    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        const NetPeer* p = &n->peers[i];
        if (!p->used || p->id == n->myId) continue;
        NetPlayerSnap* q = &pk.players[idx++];
        q->id = (uint8_t)p->id;
        q->health = (uint8_t)ClampF(p->health, 0, 255);
        q->weapon = (uint8_t)p->weapon;
        q->flags = (uint8_t)p->flags;
        q->kills = (uint8_t)(p->kills > 255 ? 255 : p->kills);
        q->deaths = (uint8_t)(p->deaths > 255 ? 255 : p->deaths);
        snprintf(q->name, sizeof(q->name), "%.11s", p->name);
        q->x = p->targetPos.x; q->y = p->targetPos.y; q->z = p->targetPos.z;
        q->yaw = p->targetYaw; q->pitch = p->pitch;
    }
    pk.pcount = (uint8_t)idx;

    int ecount = ec < MAX_ENEMIES ? ec : MAX_ENEMIES;
    pk.ecount = (uint8_t)ecount;
    for (int i = 0; i < ecount; i++) {
        const Enemy* e = &enemies[i];
        NetEnemySnap* es = &pk.enemies[i];
        es->x = e->position.x; es->y = e->position.y; es->z = e->position.z;
        es->facing = e->facing;
        es->health = e->health;
        es->maxHealth = e->maxHealth;
        es->walkPhase = e->walkPhase;
        es->deathTimer = e->deathTimer;
        es->aimBlend = (uint8_t)ClampF(e->aimBlend * 255.0f, 0, 255);
        es->strafeDir = (int8_t)(e->strafeDir < 0 ? -1 : 1);
        uint8_t fl = 0;
        if (e->alive) fl |= NETEF_ALIVE;
        if (e->elite) fl |= NETEF_ELITE;
        if (e->hasHelmet) fl |= NETEF_HELMET;
        if (e->hurtTimer > 0 || e->flinchTimer > 0) fl |= NETEF_HURT;
        if (e->aimTimer > 0 || e->burstLeft > 0) fl |= NETEF_AIMING;
        if (e->windupTimer > 0) fl |= NETEF_WINDING;
        if (e->state == E_COVER) fl |= NETEF_COVER;
        if (e->state == E_STRAFE) fl |= NETEF_STRAFE;
        es->flags = fl;
        es->firing = (e->muzzleFlash > 0) ? 1 : 0;
    }

    uint8_t bmask = 0, pmask = 0;
    for (int i = 0; i < bc && i < 8; i++) if (barrels[i].alive) bmask |= (uint8_t)(1u << i);
    for (int i = 0; i < pc && i < 8; i++) if (pickups[i].active) pmask |= (uint8_t)(1u << i);
    pk.barrelMask = bmask;
    pk.pickupMask = pmask;
    n->pendingShot = false;

    // Wire format must be contiguous, but the struct has fixed-size arrays
    // (ecount/enemies sit after all four player slots in memory), so
    // serialize explicitly: [hdr][pcount][players...][ecount][enemies...][bmask][pmask]
    uint8_t out[sizeof(PktSnapshot)];
    memcpy(out, &pk.h, sizeof(PktHdr));
    int o = (int)sizeof(PktHdr);
    out[o++] = pk.pcount;
    memcpy(out + o, pk.players, (size_t)idx * sizeof(NetPlayerSnap));
    o += idx * (int)sizeof(NetPlayerSnap);
    out[o++] = pk.ecount;
    memcpy(out + o, pk.enemies, (size_t)ecount * sizeof(NetEnemySnap));
    o += ecount * (int)sizeof(NetEnemySnap);
    out[o++] = pk.barrelMask;
    out[o++] = pk.pickupMask;
    net_send_all(n, 0, out, o);
}

static void client_send_hello(NetState* n) {
    PktHello pk;
    net_fill_hdr(n, &pk.h, P_HELLO);
    memset(pk.name, 0, sizeof(pk.name));
    snprintf(pk.name, sizeof(pk.name), "%s", n->myName);
    net_send_hostaddr(n, &pk, (int)sizeof(pk));
    n->packetsOut++;
}

static void client_send_state(NetState* n, const NetLocalState* me) {
    PktState pk;
    net_fill_hdr(n, &pk.h, P_STATE);
    pk.x = me->pos.x; pk.y = me->pos.y; pk.z = me->pos.z;
    pk.yaw = me->yaw; pk.pitch = me->pitch;
    pk.health = (uint8_t)ClampF(me->health, 0, 255);
    pk.weapon = (uint8_t)me->weapon;
    pk.flags = (uint8_t)me->flags;
    pk.kills = (uint8_t)(me->kills > 255 ? 255 : me->kills);
    pk.deaths = (uint8_t)(me->deaths > 255 ? 255 : me->deaths);
    n->pendingShot = false;
    net_send_hostaddr(n, &pk, (int)sizeof(pk));
}

// ---------------------------------------------------------------------------
// Packet handling
// ---------------------------------------------------------------------------

static void host_peer_drop(NetState* n, int id, const char* why) {
    if (id < 1 || id > NET_MAX_PLAYERS) return;
    NetPeer* p = &n->peers[id - 1];
    if (!p->used) return;
    char t[sizeof(n->feed[0].text)];
    snprintf(t, sizeof(t), "%s LEFT%s%s", p->name[0] ? p->name : "PLAYER", why[0] ? " (" : "", why);
    if (why[0]) {
        // append closing paren
        size_t l = strlen(t);
        if (l + 1 < sizeof(t)) { t[l] = ')'; t[l + 1] = 0; }
    }
    Net_Feed(n, t);
    p->used = false;
}

static void net_handle_packet(NetState* n, const struct sockaddr_in* from, const uint8_t* buf,
                              int len, Enemy* enemies, int ec, Pickup* pickups, int pc,
                              Barrel* barrels, int bc) {
    if (len < (int)sizeof(PktHdr)) return;
    (void)enemies;
    (void)ec;
    PktHdr h;
    memcpy(&h, buf, sizeof(h));
    if (h.magic != NET_MAGIC) return;

    if (n->mode == NET_CLIENT) n->lastHostRecv = n->netTime;
    if (n->mode == NET_HOST && h.from >= 1 && h.from <= NET_MAX_PLAYERS && h.from != n->myId)
        n->peers[h.from - 1].lastRecv = n->netTime;

    // duplicate / out-of-order suppression (unicast streams are ordered per
    // sender; hello has from=0 before ids are assigned and is idempotent)
    if (h.from != 0 && net_is_stale(n, h.from, h.seq)) return;

    switch (h.type) {
    case P_HELLO: {
        if (n->mode != NET_HOST || len < (int)sizeof(PktHello)) return;
        PktHello hp;
        memcpy(&hp, buf, sizeof(hp));
        char nm[16];
        memcpy(nm, hp.name, 15);
        nm[15] = 0;
        for (char* c = nm; *c; c++) if (*c < 32 || *c > 126) *c = '?';
        int id = host_find_addr(n, from);
        if (id == 0) {
            id = host_add_peer(n, from, nm);
            if (id == 0) { host_send_welcome(n, from, 0, 0); return; } // full
            char t[sizeof(n->feed[0].text)];
            snprintf(t, sizeof(t), "%s JOINED", nm);
            Net_Feed(n, t);
            TraceLog(LOG_INFO, "NET: %s joined as id %d", nm, id);
        }
        host_send_welcome(n, from, id, (id - 1) % NET_MAX_PLAYERS);
        break;
    }
    case P_WELCOME: {
        if (n->mode != NET_CLIENT || len < (int)sizeof(PktWelcome)) return;
        PktWelcome wp;
        memcpy(&wp, buf, sizeof(wp));
        if (wp.id == 0) {
            n->serverFull = true;
            TraceLog(LOG_WARNING, "NET: server full");
            return;
        }
        if (!n->connected) {
            n->myId = wp.id;
            n->connected = true;
            n->hostLost = false;
            n->pendingSpawn = wp.spawn;
            n->spawnApplied = false;
            TraceLog(LOG_INFO, "NET: connected as id %d (players: %d)", wp.id, wp.count);
            Net_Feed(n, "CONNECTED TO HOST");
        }
        break;
    }
    case P_STATE: {
        if (n->mode != NET_HOST || len < (int)sizeof(PktState)) return;
        if (h.from < 2 || h.from > NET_MAX_PLAYERS) return;
        NetPeer* p = &n->peers[h.from - 1];
        if (!p->used) return;
        if (p->addr.sin_addr.s_addr != from->sin_addr.s_addr ||
            p->addr.sin_port != from->sin_port) return;
        PktState st;
        memcpy(&st, buf, sizeof(st));
        Vector3 np = { st.x, st.y, st.z };
        if (!p->haveSnap) { p->prevPos = np; p->prevYaw = st.yaw; }
        else { p->prevPos = p->targetPos; p->prevYaw = p->targetYaw; }
        p->targetPos = np;
        p->targetYaw = st.yaw;
        p->pitch = st.pitch;
        p->snapTime = n->netTime;
        p->haveSnap = true;
        p->speed = Vector3Distance(p->targetPos, p->prevPos) / NET_SNAP_DT;
        p->health = (float)st.health;
        p->weapon = st.weapon;
        p->flags = st.flags;
        p->kills = st.kills;
        p->deaths = st.deaths;
        break;
    }
    case P_SNAPSHOT: {
        if (n->mode != NET_CLIENT) return;
        if (len < (int)(sizeof(PktHdr) + 1)) return;
        const uint8_t* q = buf + sizeof(PktHdr);
        int remain = len - (int)sizeof(PktHdr);
        uint8_t pcount = *q++;
        remain--;
        if (pcount > NET_MAX_PLAYERS) return;
        if (remain < (int)(pcount * sizeof(NetPlayerSnap) + 1)) return;
        for (int i = 0; i < pcount; i++) {
            NetPlayerSnap ps;
            memcpy(&ps, q, sizeof(ps));
            q += sizeof(ps);
            remain -= (int)sizeof(ps);
            if (ps.id == n->myId || ps.id < 1 || ps.id > NET_MAX_PLAYERS) continue;
            NetPeer* p = &n->peers[ps.id - 1];
            if (!p->used) {
                memset(p, 0, sizeof(*p));
                p->id = ps.id;
                p->used = true;
                p->health = (float)ps.health;
                p->haveSnap = false;
            }
            p->name[sizeof(ps.name)] = 0;
            memcpy(p->name, ps.name, sizeof(ps.name));
            p->name[sizeof(ps.name)] = 0;
            Vector3 np = { ps.x, ps.y, ps.z };
            if (!p->haveSnap) { p->prevPos = np; p->prevYaw = ps.yaw; }
            else { p->prevPos = p->targetPos; p->prevYaw = p->targetYaw; }
            p->targetPos = np;
            p->targetYaw = ps.yaw;
            p->pitch = ps.pitch;
            p->snapTime = n->netTime;
            p->haveSnap = true;
            p->speed = Vector3Distance(p->targetPos, p->prevPos) / NET_SNAP_DT;
            p->health = (float)ps.health;
            p->weapon = ps.weapon;
            p->flags = ps.flags;
            p->kills = ps.kills;
            p->deaths = ps.deaths;
            p->lastRecv = n->netTime;
        }
        uint8_t ecount = *q++;
        remain--;
        if (ecount > MAX_ENEMIES) return;
        if (remain < (int)(ecount * sizeof(NetEnemySnap) + 2)) return;
        for (int i = 0; i < ecount && i < ec; i++) {
            n->esnapPrev[i] = n->esnap[i];
            n->esnapAlivePrev[i] = n->esnapHave[i] && (n->esnap[i].flags & NETEF_ALIVE);
            memcpy(&n->esnap[i], q, sizeof(NetEnemySnap));
            q += sizeof(NetEnemySnap);
            remain -= (int)sizeof(NetEnemySnap);
            n->esnapHave[i] = true;
        }
        n->esnapTime = n->netTime;
        uint8_t bmask = *q++;
        uint8_t pmask = *q++;
        for (int i = 0; i < pc && i < 8; i++) {
            bool hostActive = ((pmask >> i) & 1) != 0;
            if (hostActive) {
                if (!pickups[i].active && (n->netTime - n->pickupTaken[i] > 0.6f))
                    pickups[i].active = true;
            } else {
                pickups[i].active = false;
            }
        }
        for (int i = 0; i < bc && i < 8; i++) {
            bool hostAlive = ((bmask >> i) & 1) != 0;
            if (barrels[i].alive != hostAlive) {
                barrels[i].alive = hostAlive;
                barrels[i].health = BARREL_HEALTH;
                barrels[i].respawnTimer = hostAlive ? 0.0f : 18.0f;
            }
        }
        break;
    }
    case P_DAMAGE: {
        if (len < (int)sizeof(PktDamage)) return;
        PktDamage dp;
        memcpy(&dp, buf, sizeof(dp));
        if (n->mode == NET_HOST) {
            if (dp.target == (uint8_t)n->myId) {
                Vector3 from3 = { dp.ax, dp.ay, dp.az };
                net_push_event(n, NEV_DAMAGE, from3, (Vector3){ 0 }, dp.head ? 1 : 0, dp.attacker, 0, dp.dmg);
            } else if (dp.target >= 2 && dp.target <= NET_MAX_PLAYERS) {
                // relay to the victim, untouched (same seq for dedup)
                net_send_peer(n, dp.target, buf, len);
                net_send_peer(n, dp.target, buf, len);
            }
        } else if (dp.target == (uint8_t)n->myId) {
            Vector3 from3 = { dp.ax, dp.ay, dp.az };
            net_push_event(n, NEV_DAMAGE, from3, (Vector3){ 0 }, dp.head ? 1 : 0, dp.attacker, 0, dp.dmg);
        }
        break;
    }
    case P_ENEMY_HIT: {
        if (n->mode != NET_HOST || len < (int)sizeof(PktEnemyHit)) return;
        PktEnemyHit ep;
        memcpy(&ep, buf, sizeof(ep));
        if (ep.idx >= MAX_ENEMIES) return;
        Vector3 hp = { ep.hx, ep.hy, ep.hz };
        net_push_event(n, NEV_ENEMY_HIT, hp, (Vector3){ 0 }, ep.idx, ep.zone, h.from, ep.dmg);
        break;
    }
    case P_KILL: {
        if (len < (int)sizeof(PktKill)) return;
        PktKill kp;
        memcpy(&kp, buf, sizeof(kp));
        net_feed_kill(n, kp.killer, kp.victim, kp.kind, kp.head != 0);
        bool myKill = kp.killer == (uint8_t)n->myId &&
                      ((kp.kind == 0 && kp.victim != (uint8_t)n->myId) || kp.kind == 1);
        if (myKill)
            net_push_event(n, NEV_MY_KILL, (Vector3){ 0 }, (Vector3){ 0 }, kp.head ? 1 : 0, 0, 0, 0);
        if (n->mode == NET_HOST) {
            // behaviour relay: forward player-death announcements to others
            if (kp.kind == 0) {
                for (int i = 0; i < NET_MAX_PLAYERS; i++) {
                    const NetPeer* p = &n->peers[i];
                    if (!p->used || p->id == n->myId) continue;
                    if (p->id == h.from) continue;   // not back to sender
                    if (p->id == kp.victim) continue; // victim already knows
                    net_send_peer(n, p->id, buf, len);
                    net_send_peer(n, p->id, buf, len);
                }
            }
        }
        break;
    }
    case P_BARREL: {
        if (n->mode != NET_CLIENT || len < (int)sizeof(PktBarrel)) return;
        PktBarrel bp;
        memcpy(&bp, buf, sizeof(bp));
        if (bp.idx >= MAX_BARRELS) return;
        Vector3 pos = { bp.x, 0.9f, bp.z };
        net_push_event(n, NEV_BARREL, pos, (Vector3){ 0 }, bp.idx, 0, 0, 0);
        break;
    }
    case P_BARREL_HIT: {
        if (n->mode != NET_HOST || len < (int)sizeof(PktBarrelHit)) return;
        PktBarrelHit bh;
        memcpy(&bh, buf, sizeof(bh));
        if (bh.idx >= MAX_BARRELS) return;
        net_push_event(n, NEV_BARREL_HIT, (Vector3){ 0 }, (Vector3){ 0 }, bh.idx, 0, h.from, bh.dmg);
        break;
    }
    case P_NOISE: {
        if (n->mode != NET_HOST || len < (int)sizeof(PktNoise)) return;
        PktNoise npk;
        memcpy(&npk, buf, sizeof(npk));
        Vector3 np = { npk.x, npk.y, npk.z };
        net_push_event(n, NEV_NOISE, np, (Vector3){ 0 }, 0, 0, 0, 34.0f);
        break;
    }
    case P_PICKUP: {
        if (n->mode != NET_HOST || len < (int)sizeof(PktPickup)) return;
        PktPickup pp;
        memcpy(&pp, buf, sizeof(pp));
        if (pp.idx < (uint8_t)pc && pickups[pp.idx].active) {
            pickups[pp.idx].active = false;
            pickups[pp.idx].respawnTimer = PICKUP_RESPAWN_TIME;
        }
        break;
    }
    case P_BYE: {
        if (n->mode == NET_HOST) {
            host_peer_drop(n, h.from, "");
        } else if (h.from >= 1 && h.from <= NET_MAX_PLAYERS && h.from != n->myId) {
            NetPeer* p = &n->peers[h.from - 1];
            if (p->used) {
                char t[sizeof(n->feed[0].text)];
                snprintf(t, sizeof(t), "%s LEFT", p->name[0] ? p->name : "PLAYER");
                Net_Feed(n, t);
                p->used = false;
            }
        }
        break;
    }
    default: break;
    }
}

// ---------------------------------------------------------------------------
// Per-frame interpolation (render state for remote players)
// ---------------------------------------------------------------------------

static float net_lerp_angle(float a, float b, float t) {
    float d = b - a;
    while (d > PI) d -= 2.0f * PI;
    while (d < -PI) d += 2.0f * PI;
    return a + d * t;
}

static void net_interp_peers(NetState* n) {
    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        NetPeer* p = &n->peers[i];
        if (!p->used || p->id == n->myId) continue;
        if (!p->haveSnap) continue;
        float a = ClampF((n->netTime - p->snapTime) / NET_SNAP_DT, 0.0f, 1.0f);
        p->pos = Vector3Lerp(p->prevPos, p->targetPos, a);
        p->yaw = net_lerp_angle(p->prevYaw, p->targetYaw, a);

        // firing edge: local muzzle flash + tracer + (game.c drains) sound
        bool firing = (p->flags & NETF_FIRING) != 0;
        if (firing && !p->firingLatch && (p->flags & NETF_ALIVE)) {
            Vector3 fwd = { cosf(p->pitch) * sinf(p->yaw), sinf(p->pitch), cosf(p->pitch) * cosf(p->yaw) };
            Vector3 muzzle = { p->pos.x + fwd.x * 0.75f, p->pos.y + 1.36f + fwd.y * 0.75f,
                               p->pos.z + fwd.z * 0.75f };
            Vector3 end = Vector3Add(muzzle, Vector3Scale(fwd, 55.0f));
            net_push_event(n, NEV_REMOTE_SHOT, muzzle, end, p->weapon, 0, 0, 0);
        }
        p->firingLatch = firing;
    }
}

// ---------------------------------------------------------------------------
// Client mirror of host enemies (called every frame for smooth 60fps motion)
// ---------------------------------------------------------------------------

static void net_apply_enemies(NetState* n, Enemy* enemies, int count, float dt) {
    if (n->mode != NET_CLIENT) return; // host/SP: the sim owns the enemies
    if (!n->connected || !n->esnapHave[0]) {
        // no snapshot yet: keep the local proxies hidden (world is host-side)
        for (int i = 0; i < count; i++) enemies[i].alive = false;
        return;
    }
    float a = ClampF((n->netTime - n->esnapTime) / NET_SNAP_DT, 0.0f, 1.0f);
    for (int i = 0; i < count && i < MAX_ENEMIES; i++) {
        if (!n->esnapHave[i]) continue;
        NetEnemySnap* c = &n->esnap[i];
        NetEnemySnap* pv = &n->esnapPrev[i];
        Enemy* e = &enemies[i];

        // clients skip Enemy_UpdateAll, so tick event timers here (the sim
        // normally decays them; snapshots only carry edge flags)
        if (e->hurtTimer > 0) e->hurtTimer -= dt;
        if (e->flinchTimer > 0) e->flinchTimer -= dt;
        if (e->muzzleFlash > 0) e->muzzleFlash -= dt;

        bool prevAlive = n->esnapAlivePrev[i];
        bool nowAlive = (c->flags & NETEF_ALIVE) != 0;
        if (prevAlive && !nowAlive)
            net_push_event(n, NEV_ENEMY_DIE, e->position, (Vector3){ 0 }, i, 0, 0, 0);

        Vector3 tp = { c->x, c->y, c->z };
        Vector3 pp = { pv->x, pv->y, pv->z };
        if (prevAlive && nowAlive && n->esnapHave[i]) e->position = Vector3Lerp(pp, tp, a);
        else e->position = tp; // spawn/respawn: snap, don't slide

        if (prevAlive && nowAlive) e->facing = net_lerp_angle(pv->facing, c->facing, a);
        else e->facing = c->facing;
        e->walkPhase = pv->walkPhase + (c->walkPhase - pv->walkPhase) * a;
        e->deathTimer = c->deathTimer;
        e->health = c->health;
        e->maxHealth = c->maxHealth > 1.0f ? c->maxHealth : 100.0f;
        e->alive = nowAlive;
        e->elite = (c->flags & NETEF_ELITE) != 0;
        e->hasHelmet = (c->flags & NETEF_HELMET) != 0;
        e->aimBlend = (float)c->aimBlend / 255.0f;
        e->state = (c->flags & NETEF_COVER) ? E_COVER
                 : (c->flags & NETEF_STRAFE) ? E_STRAFE : E_CHASE;
        e->strafeDir = c->strafeDir < 0 ? -1.0f : 1.0f;

        if (c->flags & NETEF_HURT) {
            if (e->hurtTimer <= 0) e->hurtTimer = 0.13f;
            if (e->flinchTimer <= 0) e->flinchTimer = 0.10f;
        }
        if (c->firing) {
            if (e->muzzleFlash <= 0) {
                if (n->netTime - n->enemyShotCd > 0.05f) {
                    net_push_event(n, NEV_ENEMY_SHOT, e->position, (Vector3){ 0 }, i, 0, 0, 0);
                    n->enemyShotCd = n->netTime;
                }
            }
            e->muzzleFlash = fmaxf(e->muzzleFlash, 0.055f);
        }
    }
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool Net_Init(NetState* n) {
    int mode = g_cfgMode;
    memset(n, 0, sizeof(*n));
    n->mode = mode;
    n->myId = (mode == NET_CLIENT) ? 0 : 1;
    n->pendingSpawn = -1;
    n->port = g_cfgPort;
    const char* defName = (mode == NET_HOST) ? "HOST" : "PLAYER";
    snprintf(n->myName, sizeof(n->myName), "%s", g_cfgName[0] ? g_cfgName : defName);
    for (int i = 0; i < NET_MAX_PLAYERS; i++) n->peers[i].id = i + 1;
    if (mode == NET_OFF) return false;

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        TraceLog(LOG_WARNING, "NET: socket() failed, staying single-player");
        n->mode = NET_OFF;
        return false;
    }
    net_set_nonblock(fd);

    if (mode == NET_HOST) {
        int yes = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        struct sockaddr_in a;
        memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET;
        a.sin_port = htons((uint16_t)g_cfgPort);
        a.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(fd, (struct sockaddr*)&a, sizeof(a)) != 0) {
            TraceLog(LOG_WARNING, "NET: bind UDP %d failed, staying single-player", g_cfgPort);
            close(fd);
            n->mode = NET_OFF;
            return false;
        }
        n->sock = fd;
        n->netTime = (float)GetTime();
        n->active = true;
        TraceLog(LOG_INFO, "NET: hosting on UDP port %d as \"%s\"", g_cfgPort, n->myName);
    } else {
        n->host.sin_family = AF_INET;
        n->host.sin_port = htons((uint16_t)g_cfgPort);
        if (inet_pton(AF_INET, g_cfgAddr, &n->host.sin_addr) != 1)
            n->host.sin_addr.s_addr = inet_addr(g_cfgAddr);
        n->sock = fd;
        n->netTime = (float)GetTime();
        n->lastHostRecv = GetTime();
        n->active = true;
        client_send_hello(n);
        n->connectTimer = 0.0f;
        TraceLog(LOG_INFO, "NET: connecting to %s:%d as \"%s\"", g_cfgAddr, g_cfgPort, n->myName);
    }
    return true;
}

void Net_Shutdown(NetState* n) {
    if (!n->active) return;
    PktBye pk;
    net_fill_hdr(n, &pk.h, P_BYE);
    if (n->mode == NET_HOST) {
        net_send_all(n, 0, &pk, (int)sizeof(pk));
    } else {
        net_send_hostaddr(n, &pk, (int)sizeof(pk));
        net_send_hostaddr(n, &pk, (int)sizeof(pk));
    }
    if (n->sock > 0) close(n->sock);
    n->sock = 0;
    n->active = false;
}

bool Net_HasPeers(const NetState* n) {
    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        const NetPeer* p = &n->peers[i];
        if (p->used && p->id != n->myId) return true;
    }
    return false;
}

int Net_AlivePeers(const NetState* n) {
    int c = 0;
    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        const NetPeer* p = &n->peers[i];
        if (p->used && p->id != n->myId && (p->flags & NETF_ALIVE)) c++;
    }
    return c;
}

// ---------------------------------------------------------------------------
// Main per-frame pump
// ---------------------------------------------------------------------------

void Net_Update(NetState* n, float dt, const NetLocalState* me,
                Enemy* enemies, int ec, Pickup* pickups, int pc,
                Barrel* barrels, int bc) {
    if (!n->active) return;
    n->netTime = (float)GetTime();
    n->eventCount = 0;

    static uint8_t rbuf[NET_RECV_BUF];
    for (int guard = 0; guard < 64; guard++) {
        struct sockaddr_in from;
        socklen_t sl = sizeof(from);
        ssize_t r = recvfrom(n->sock, rbuf, sizeof(rbuf), 0, (struct sockaddr*)&from, &sl);
        if (r <= 0) break;
        n->packetsIn++;
        net_handle_packet(n, &from, rbuf, (int)r, enemies, ec, pickups, pc, barrels, bc);
    }

    // liveness
    if (n->mode == NET_HOST) {
        for (int i = 0; i < NET_MAX_PLAYERS; i++) {
            NetPeer* p = &n->peers[i];
            if (!p->used || p->id == n->myId) continue;
            if (n->netTime - p->lastRecv > NET_TIMEOUT) host_peer_drop(n, p->id, "TIMEOUT");
        }
    } else if (n->connected && !n->hostLost) {
        if (GetTime() - n->lastHostRecv > NET_HOST_TIMEOUT) {
            n->hostLost = true;
            Net_Feed(n, "HOST CONNECTION LOST");
            TraceLog(LOG_WARNING, "NET: host silent, connection stalled");
        }
    }

    for (int i = 0; i < NET_FEED_LINES; i++)
        if (n->feed[i].timer > 0) n->feed[i].timer -= dt;

    if (n->lastAttackerTime > 0) n->lastAttackerTime -= dt;

    n->sendTimer += dt;
    if (n->mode == NET_HOST) {
        if (n->sendTimer >= NET_SNAP_DT) {
            n->sendTimer = 0;
            host_send_snapshot(n, me, enemies, ec, pickups, pc, barrels, bc);
        }
    } else {
        if (!n->connected && !n->serverFull) {
            n->connectTimer += dt;
            if (n->connectTimer >= 0.6f) { n->connectTimer = 0; client_send_hello(n); }
        }
        if (n->connected && !n->hostLost && n->sendTimer >= NET_STATE_DT) {
            n->sendTimer = 0;
            client_send_state(n, me);
        }
    }

    net_interp_peers(n);
    net_apply_enemies(n, enemies, ec, dt);
}

// ---------------------------------------------------------------------------
// Remote player rendering
// ---------------------------------------------------------------------------

static void net_draw_body_lit(const NetPeer* p, float time) {
    bool alive = (p->flags & NETF_ALIVE) != 0;
    Color suit = Net_PlayerColor(p->id);
    Color pant = { (unsigned char)(suit.r * 0.72f), (unsigned char)(suit.g * 0.72f),
                   (unsigned char)(suit.b * 0.72f), 255 };
    Color skin = { 224, 172, 105, 255 };
    Color gun  = { 55, 55, 62, 255 };

    rlPushMatrix();
    rlTranslatef(p->pos.x, p->pos.y, p->pos.z);
    rlRotatef(p->yaw * RAD2DEG, 0, 1, 0);

    if (alive) {
        float swing = 0;
        if (p->speed > 0.6f)
            swing = sinf(time * 9.5f + (float)p->id * 1.7f) *
                    fminf(p->speed / 8.0f, 1.0f) * 0.30f;
        // legs (swing fore/aft in local z)
        DrawCubeV((Vector3){ -0.15f, 0.42f, swing }, (Vector3){ 0.21f, 0.84f, 0.23f }, pant);
        DrawCubeV((Vector3){  0.15f, 0.42f, -swing }, (Vector3){ 0.21f, 0.84f, 0.23f }, pant);
        // torso
        DrawCubeV((Vector3){ 0, 1.24f, 0 }, (Vector3){ 0.55f, 0.74f, 0.34f }, suit);
        // head
        DrawCubeV((Vector3){ 0, 1.78f, 0 }, (Vector3){ 0.30f, 0.30f, 0.28f }, skin);
        // arms reaching forward onto the weapon
        DrawCubeV((Vector3){ -0.29f, 1.30f, 0.12f }, (Vector3){ 0.16f, 0.16f, 0.40f }, suit);
        DrawCubeV((Vector3){  0.29f, 1.30f, 0.12f }, (Vector3){ 0.16f, 0.16f, 0.40f }, suit);
        // weapon silhouette depends on what they are holding
        float glen = p->weapon == 2 ? 1.40f : (p->weapon == 1 ? 1.05f : 0.45f);
        float gz   = p->weapon == 0 ? 0.30f : 0.62f;
        DrawCubeV((Vector3){ -0.02f, 1.36f, gz }, (Vector3){ 0.13f, 0.13f, glen }, gun);
        if (p->flags & NETF_FIRING) {
            Vector3 tip = { -0.02f, 1.36f, gz + glen * 0.55f + 0.10f };
            DrawSphereEx(tip, 0.075f, 6, 5, (Color){ 255, 210, 100, 255 });
        }
        if (p->flags & NETF_ADS) {
            // little scope/carry pose marker: hands up a touch
            DrawCubeV((Vector3){ -0.02f, 1.50f, 0.30f }, (Vector3){ 0.10f, 0.06f, 0.20f }, gun);
        }
    } else {
        // corpse: flat pile on the ground
        DrawCubeV((Vector3){ 0, 0.18f, 0 }, (Vector3){ 0.62f, 0.34f, 0.92f }, suit);
        DrawCubeV((Vector3){ 0, 0.16f, 0.72f }, (Vector3){ 0.30f, 0.30f, 0.28f }, skin);
        DrawCubeV((Vector3){ -0.42f, 0.12f, -0.15f }, (Vector3){ 0.18f, 0.22f, 0.60f }, pant);
        DrawCubeV((Vector3){  0.42f, 0.12f, -0.15f }, (Vector3){ 0.18f, 0.22f, 0.60f }, pant);
    }
    rlPopMatrix();
}

void Net_DrawBodies(const NetState* n, float time, bool lit) {
    if (!n->active) return;
    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        const NetPeer* p = &n->peers[i];
        if (!p->used || p->id == n->myId) continue;
        if (!lit && !(p->flags & NETF_ALIVE)) continue; // casters: living only
        net_draw_body_lit(p, time);
    }
}

void Net_DrawCasterBoxes(const NetState* n) {
    if (!n->active) return;
    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        const NetPeer* p = &n->peers[i];
        if (!p->used || p->id == n->myId) continue;
        if (!(p->flags & NETF_ALIVE)) continue;
        DrawCubeV((Vector3){ p->pos.x, p->pos.y + 0.85f, p->pos.z },
                  (Vector3){ 0.68f, 1.70f, 0.55f }, WHITE);
    }
}
