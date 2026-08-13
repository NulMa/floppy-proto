/* sim.h — 순수 게임 로직. raylib 을 절대 include 하지 않는다.
 *
 * 코어:
 *   용량 12칸(=1.44MB). 능력은 파일이고 칸을 먹는다. 전부는 못 들고 간다.
 *   로드아웃은 **터미널에서만** 바꿀 수 있다. 그래서 커밋이 생기고, 커밋이 전략이 된다.
 *   터미널을 떠나면 포맷 파동이 왼쪽에서 쫓아온다. "갈 수 있나"가 "제때 갈 수 있나"가 된다. */
#ifndef SIM_H
#define SIM_H

#define MAP_W  96
#define MAP_H  18
#define TILE   16
#define VIEW_W (32 * TILE)        /* 화면에 보이는 폭 512 */
#define VIEW_H (MAP_H * TILE)     /* 288 */

#define PLAYER_W 12
#define PLAYER_H 14

#define CAP_SLOTS   12
#define TOTAL_BYTES 1474560
#define BYTES_PER_SLOT (TOTAL_BYTES / CAP_SLOTS)

typedef enum { T_EMPTY = 0, T_SOLID, T_GOAL, T_TERMINAL } Tile;
typedef enum { ST_PLAY = 0, ST_DEAD, ST_WON } State;

typedef enum { AB_JUMP2 = 0, AB_DASH, AB_GLIDE, AB_GRIP, AB_BOOT, AB_COUNT } Ability;
extern const char *const ABILITY_FILE[AB_COUNT];
extern const int         ABILITY_COST[AB_COUNT];

typedef struct {
    float gravity, jumpVel, jumpCut, maxFall;
    float accelGround, accelAir, maxSpeed, frictionGround, frictionAir;
    float coyoteTime, jumpBuffer;
    float jump2Mult;
    float dashSpeed, dashTime, dashCooldown;
    float glideFall;
    float gripSlide, gripJumpX, gripJumpY, gripStick;
    float bootMult;
    float formatSpeed;      /* 포맷 파동 진행 속도 px/s */
    float formatLead;       /* 터미널을 떠날 때 파동이 뒤에 잡히는 거리 */
} Tune;

void tune_defaults(Tune *t);
int  tune_set(Tune *t, const char *key, float v);

typedef struct {
    float x, y, vx, vy;
    int   onGround, facing;
    float coyote, buffer;
    int   airJumps;
    float dashTimer, dashCd;
    int   dashDir, dashUsed;
    int   wallDir;
    float wallStick;
    int   has[AB_COUNT];
} Player;

typedef struct {
    unsigned char map[MAP_H][MAP_W];
    Player p;
    State  state;
    float  time;
    int    deaths;

    float formatX;        /* 이 x 왼쪽은 지워졌다. 닿으면 죽는다 */
    int   atTerminal;     /* 지금 터미널 위에 서 있는가 = 안전 + 로드아웃 변경 가능 */
    float spawnX, spawnY; /* 마지막으로 밟은 터미널 = 체크포인트 */
    int   terminalsSeen;

    int   evJump, evDoubleJump, evDash, evWallJump, evLand, evDeny, evEquip, evCheckpoint;
    float denyFlash, checkFlash;
} World;

typedef struct {
    int left, right, jumpHeld, jumpPressed, dashPressed;
    int toggleAbility;   /* -1 = 없음 */
} Input;

void sim_init(World *w);
void sim_step(World *w, const Tune *t, const Input *in, float dt);
int  sim_used_slots(const World *w);
int  sim_free_slots(const World *w);

#endif
