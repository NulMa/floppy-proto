/* sim.h — 순수 게임 로직. raylib 을 절대 include 하지 않는다.
 *
 * 코어:
 *   용량 12칸(=1.44MB). 능력은 파일이고 칸을 먹는다. 전부는 못 들고 간다.
 *   로드아웃은 **터미널에서만** 바꾼다. 커밋이 생기고, 커밋이 전략이 된다.
 *   터미널을 떠나면 포맷 파동이 왼쪽에서 쫓아온다. "갈 수 있나"가 "제때 갈 수 있나"가 된다. */
#ifndef SIM_H
#define SIM_H

#define MAP_W  96                 /* 레벨이 쓸 수 있는 최대 폭 */
#define MAP_H  18
#define TILE   16
#define VIEW_W (32 * TILE)        /* 512 */
#define VIEW_H (MAP_H * TILE)     /* 288 */

#define PLAYER_W 12
#define PLAYER_H 14

#define CAP_SLOTS   12
#define TOTAL_BYTES 1474560
#define BYTES_PER_SLOT (TOTAL_BYTES / CAP_SLOTS)

typedef enum { T_EMPTY = 0, T_SOLID, T_GOAL, T_TERMINAL } Tile;
typedef enum { ST_PLAY = 0, ST_WON } State;

typedef enum { AB_JUMP2 = 0, AB_DASH, AB_GLIDE, AB_GRIP, AB_BOOT, AB_COUNT } Ability;
extern const char *const ABILITY_FILE[AB_COUNT];
extern const int         ABILITY_COST[AB_COUNT];

#include "levels.h"

typedef struct {
    float gravity, jumpVel, jumpCut, maxFall;
    float accelGround, accelAir, maxSpeed, frictionGround, frictionAir;
    float coyoteTime, jumpBuffer;
    float jump2Mult;
    float dashSpeed, dashTime, dashCooldown;
    float glideFall;
    float gripSlide, gripJumpX, gripJumpY, gripStick;
    float bootMult;
    float formatLead;         /* 터미널을 떠날 때 파동이 뒤에 잡히는 거리 */
    float formatScale;        /* 레벨별 파동 속도에 곱하는 전역 배수 (난이도 조절용) */
    float msgTime;            /* 안내 문구가 떠 있는 시간 */
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

#define AB_BIT(a)   (1u << (a))
#define AB_ALL      ((1u << AB_COUNT) - 1u)
#define AB_STARTING AB_BIT(AB_BOOT)   /* boot.exe 는 처음부터 있다 — 부팅에 쓰던 것 */

typedef struct {
    int    levelIndex;
    unsigned unlocked;     /* 영구 해금된 능력. 스테이지 클리어로 늘어난다 */
    unsigned char map[MAP_H][MAP_W];
    Player p;
    State  state;
    float  time;
    int    deaths;

    float formatX;
    int   atTerminal;
    unsigned avail;        /* 지금 터미널에서 장착 가능한 능력. 0 = 전부 */
    float spawnX, spawnY;

    unsigned msgShown;     /* 이미 띄운 안내 (비트마스크) */
    float    msgTimer;
    const char *msgL1, *msgL2;

    int   evJump, evDoubleJump, evDash, evWallJump, evLand, evDeny, evEquip, evCheckpoint, evDeath;
    float denyFlash, checkFlash;
} World;

typedef struct {
    int left, right, jumpHeld, jumpPressed, dashPressed;
    int toggleAbility;   /* -1 = 없음 */
} Input;

void sim_new_game(World *w, const Tune *t);            /* 해금 상태까지 초기화 */
void sim_load(World *w, const Tune *t, int levelIndex); /* 해금 상태는 유지 */
void sim_step(World *w, const Tune *t, const Input *in, float dt);
unsigned sim_grant_reward(World *w);                    /* 클리어 보상 지급. 새로 얻은 것 반환 */
int  sim_used_slots(const World *w);
int  sim_free_slots(const World *w);
int  sim_ability_unlocked(const World *w, int ab);      /* 아예 안 뜨게 할지의 기준 */
int  sim_ability_available(const World *w, int ab);     /* 지금 이 터미널에서 꺼낼 수 있는가 */
const Level *sim_level(const World *w);

#endif
