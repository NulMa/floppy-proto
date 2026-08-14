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

typedef enum {
    T_EMPTY = 0,
    T_SOLID,          /* 벽 */
    T_ONEWAY,         /* 아래에서 뚫고 올라가고, 위에서는 밟힌다 */
    T_PAD,            /* 점프패드 — 밟으면 튕겨 올린다 */
    T_SPIKE,          /* 가시 — 닿으면 죽는다 */
    T_TERMINAL,       /* 로드아웃 교체 + 체크포인트 + 파동 정지 */
    T_DISK,           /* 회수 대상. 없는 스테이지도 있다 */
    T_EXIT            /* 드라이브 슬롯 — 여기 닿으면 클리어 */
} Tile;
typedef enum { ST_PLAY = 0, ST_WON } State;

/* ★ 순서 = 획득 순서 = 키 번호.
 *   1 boot / 2 dash / 3 glide / 4 grip / 5 jump2
 *   해금 순서와 키가 어긋나면 첫 능력이 5번 키가 되어버린다. */
typedef enum { AB_BOOT = 0, AB_DASH, AB_GLIDE, AB_GRIP, AB_JUMP2, AB_COUNT } Ability;
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
    float padVel;             /* 점프패드가 튕겨 올리는 속도 (음수) */
    float formatLead;         /* 터미널을 떠날 때 파동이 뒤에 잡히는 거리 */
    float formatScale;        /* 레벨별 파동 속도에 곱하는 전역 배수 (난이도 조절용) */
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
    int   hasDisk;         /* 이 스테이지의 디스크를 회수했는가 */

    /* 안내는 상태를 안 갖는다 — 힌트는 플레이어 위치의 함수라 렌더가 직접 계산한다 */

    int   evJump, evDoubleJump, evDash, evWallJump, evLand, evDeny, evEquip;
    int   evCheckpoint, evDeath, evPad, evDisk;
    float denyFlash, checkFlash, diskFlash;
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
