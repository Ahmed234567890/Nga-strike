#ifndef NET_H
#define NET_H

// LAN multiplayer: host-authoritative star topology over a single UDP socket.
//   host   = the machine that runs the game world simulation (enemies, pickups,
//            barrels) and relays events between clients
//   client = sends its own player state to the host and renders everyone else
// All packets are small fixed-size little-endian structs (< MTU, no fragment).

#include "common.h"
#include <stdint.h>
#include <netinet/in.h>

#define NET_MAX_PLAYERS  4
#define NET_PORT_DEFAULT 7777
#define NET_FEED_LINES   5
#define NET_SNAP_DT      (1.0f / 20.0f)   // host world snapshot rate
#define NET_STATE_DT     (1.0f / 30.0f)   // client owner-state rate
#define NET_TIMEOUT      10.0f            // peer silence before drop (host)
#define NET_HOST_TIMEOUT 8.0f             // host silence before warning (client)

enum { NET_OFF = 0, NET_HOST = 1, NET_CLIENT = 2 };

// player snapshot flags
#define NETF_ALIVE   1
#define NETF_FIRING  2
#define NETF_SPRINT  4
#define NETF_ADS     8
#define NETF_MOVING  16

// enemy snapshot flags
#define NETEF_ALIVE   1
#define NETEF_ELITE   2
#define NETEF_HELMET  4
#define NETEF_HURT    8
#define NETEF_AIMING  16
#define NETEF_WINDING 32
#define NETEF_COVER   64
#define NETEF_STRAFE  128

// packet types
enum {
    P_HELLO = 1, P_WELCOME, P_STATE, P_SNAPSHOT, P_DAMAGE,
    P_ENEMY_HIT, P_KILL, P_BARREL, P_BARREL_HIT, P_NOISE, P_PICKUP, P_BYE,
};

// events drained by game.c after Net_Update (side effects live in game code)
enum {
    NEV_NONE = 0,
    NEV_DAMAGE,       // a = attacker pos, f0 = dmg, i0 = head, i1 = attacker id
    NEV_ENEMY_HIT,    // i0 = enemy idx, i1 = hit zone, i2 = killer id, f0 = dmg, a = hit pos
    NEV_BARREL_HIT,   // i0 = barrel idx, f0 = dmg (host applies barrel health)
    NEV_BARREL,       // i0 = barrel idx, a = blast pos (replicate explosion)
    NEV_REMOTE_SHOT,  // i0 = weapon idx, a = muzzle pos, b = tracer end
    NEV_ENEMY_SHOT,   // i0 = enemy idx (client: firing feedback sound)
    NEV_ENEMY_DIE,    // i0 = enemy idx, a = death position (client feedback)
    NEV_NOISE,        // a = position (host: alert enemies)
    NEV_MY_KILL,      // i0 = 1 headshot (confirmed kill by a remote, score it)
};

typedef struct __attribute__((packed)) {
    uint8_t id, health, weapon, flags, kills, deaths;
    char    name[12];
    float   x, y, z, yaw, pitch;
} NetPlayerSnap;

typedef struct __attribute__((packed)) {
    float   x, y, z, facing, health, maxHealth, walkPhase, deathTimer;
    uint8_t aimBlend, flags;
    int8_t  strafeDir;
    uint8_t firing;
} NetEnemySnap;

typedef struct {
    int    id;
    bool   used;
    char   name[16];
    // interpolated render state
    Vector3 pos, prevPos, targetPos;
    float   yaw, prevYaw, targetYaw, pitch;
    float   health;
    int     weapon;
    unsigned flags;
    float   speed;        // derived horizontal speed (walk animation)
    int     kills, deaths;
    // net bookkeeping
    struct sockaddr_in addr;   // host side: where to reach this client
    double  lastRecv;
    uint16_t lastSeq;
    bool    haveSnap;
    float   snapTime;
    bool    firingLatch;
} NetPeer;

typedef struct {
    char  text[52];
    float timer;
} NetFeedLine;

typedef struct {
    int type;
    Vector3 a, b;
    int i0, i1, i2;
    float f0;
} NetEvent;

typedef struct {
    Vector3 pos;
    float   yaw, pitch;
    float   health;
    int     weapon;
    unsigned flags;
    int     kills, deaths;
} NetLocalState;

typedef struct {
    int    mode;              // NET_OFF / NET_HOST / NET_CLIENT
    int    sock;
    int    port;              // configured UDP port (HUD display)
    bool   active;
    bool   connected;         // client: welcome received
    bool   serverFull;
    int    myId;
    char   myName[16];
    struct sockaddr_in host;  // client: host address
    double lastHostRecv;
    bool   hostLost;
    float  connectTimer;      // client: hello retry
    float  sendTimer;
    uint16_t seq;
    NetPeer peers[NET_MAX_PLAYERS];   // index = id-1 (peers[0] = host slot)
    // client mirror of host world state
    NetEnemySnap esnap[MAX_ENEMIES], esnapPrev[MAX_ENEMIES];
    bool   esnapHave[MAX_ENEMIES];
    bool   esnapAlivePrev[MAX_ENEMIES];
    float  esnapTime;
    float  enemyShotCd;
    float  pickupTaken[MAX_PICKUPS];  // client: local pickup prediction times
    // local death flow (driven by game.c)
    bool   localDead;
    float  respawnTimer;
    int    lastAttackerId;
    float  lastAttackerTime;
    int    selfDeaths;
    // spawn handshake
    bool   spawnApplied;
    int    pendingSpawn;
    // one-shot "fired since last state" bit for remote muzzle events
    bool   pendingShot;
    // events + feed
    NetEvent events[24];
    int    eventCount;
    NetFeedLine feed[NET_FEED_LINES];
    float  netTime;
    // stats
    int    packetsIn, packetsOut;
} NetState;

void Net_Configure(int mode, const char* addr, int port, const char* name);
bool Net_Init(NetState* n);
void Net_Shutdown(NetState* n);
void Net_Update(NetState* n, float dt, const NetLocalState* me,
                Enemy* enemies, int enemyCount,
                Pickup* pickups, int pickupCount,
                Barrel* barrels, int barrelCount);

// helpers shared with game/ui code
bool  Net_HasPeers(const NetState* n);
int   Net_AlivePeers(const NetState* n);
Vector3 Net_SpawnPos(int spawnIdx);
float Net_SpawnYaw(int spawnIdx);
Color Net_PlayerColor(int id);
void  Net_Feed(NetState* n, const char* text);
const char* Net_PeerName(const NetState* n, int id);

// event senders (routing through the host star is handled internally)
void Net_SendEnemyHit(NetState* n, int enemyIdx, int zone, float dmg, Vector3 hitPos);
void Net_SendDamageTo(NetState* n, int targetId, float dmg, bool head, Vector3 attackerPos, int attackerId);
void Net_SendBarrelHit(NetState* n, int barrelIdx, float dmg);
void Net_NotifyBarrel(NetState* n, int barrelIdx, Vector3 pos);
void Net_NotifyEnemyKilled(NetState* n, int killerId, bool head);
void Net_SendNoise(NetState* n, Vector3 pos);
void Net_RequestPickup(NetState* n, int idx);
void Net_NoteLocalPickup(NetState* n, int idx);
void Net_SendKill(NetState* n, int killerId, int kind, bool head);

// 3D rendering of remote players (call inside the 3D pass / caster passes)
void Net_DrawBodies(const NetState* n, float time, bool lit);
// caster-pass silhouettes for the dynamic lamp atlas (boxes only, cheap)
void Net_DrawCasterBoxes(const NetState* n);

#endif
