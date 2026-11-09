/* sim.h — 순수 게임 로직. raylib 을 절대 include 하지 않는다.
 *
 * 코어:
 *   용량 12칸(=1.44MB). 능력은 파일이고 칸을 먹는다. 전부는 못 들고 간다.
 *   로드아웃은 **터미널에서만** 바꾼다. 커밋이 생기고, 커밋이 전략이 된다.
 *   터미널을 떠나면 포맷 파동이 왼쪽에서 쫓아온다. "갈 수 있나"가 "제때 갈 수 있나"가 된다. */
#ifndef SIM_H
#define SIM_H

/* ★ 96 → 128 (2026-08-16). 스테이지당 볼륨을 늘리기 위해서다.
 *   맵은 Box 목록으로 짓기 때문에 폭을 늘려도 **레벨 데이터는 안 커진다.**
 *   커지는 건 World.map 뿐이고 18×128 = 2,304 B 라 예산에 영향이 없다. */
#define MAP_W  128                /* 레벨이 쓸 수 있는 최대 폭 */
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
    T_PAD,            /* 점프패드 — 그 위에서 점프하면 높이 튄다 (밟기만 해서는 안 튄다) */
    T_SPIKE,          /* 가시 — 닿으면 죽는다 */
    T_TERMINAL,       /* 로드아웃 교체 + 체크포인트 + 파동 정지 */
    T_DISK,           /* 회수 대상. 없는 스테이지도 있다 */
    T_EXIT,           /* 드라이브 슬롯 — 여기 닿으면 클리어 */

    /* ── 아래 넷은 2026-08-16 추가. 20 스테이지에 **움직이는 위험이 하나도 없다**는
     *    진단에서 나왔다. 적을 만들지 않고 지형만으로 시간 축을 만든다.
     *    ★ 새 타일은 반드시 **뒤에** 붙인다. 중간에 끼우면 기존 레벨의 t 값이 밀린다. */

    /* 교대 가시 — A 와 B 가 spikeCycle 마다 번갈아 켜진다. 정확히 한쪽만 켜져 있다.
     * 꺼져 있는 동안은 그냥 빈 칸이라 걸어서 지나간다.
     *
     * ★ 이것이 이 게임의 **유일한 타이밍 축**이다. 적 하나 없이 리듬을 만든다.
     *   폭을 3칸으로 두고 사이에 3칸 안전지대를 끼우면, 대시로 뚫으려 해도
     *   켜져 있는 쪽에 닿아 죽는다 — 기다리는 것 말고 답이 없다. */
    T_SPIKE_A,
    T_SPIKE_B,

    /* 상승 기류 — **활공 중일 때만** 위로 밀어올린다. 통과 가능한 칸이다.
     *
     * ★ D-33("glide 의 존재 이유")이 여기서 닫힌다. 활공은 지금까지
     *   "긴 하강"에서만 값을 했고 그 지형은 몇 개 스테이지에만 있었다.
     *   기류가 생기면 활공에 **세로 상승** 역할이 붙어서, 굴뚝을
     *   grip 으로 오를지 glide 로 오를지 고르는 두 갈래 길이 데이터만으로 만들어진다. */
    T_UPDRAFT,

    /* 무너지는 발판 — 밟고 crumbleDelay 뒤에 사라지고 crumbleRespawn 뒤에 돌아온다.
     * 파동은 전역 압력이라 국소 시간 압박을 못 만든다. 이 타일이 그걸 만든다. */
    T_CRUMBLE
} Tile;
typedef enum { ST_PLAY = 0, ST_WON } State;

/* 왜 죽었는지 — 연출이 달라야 무슨 일이 일어났는지 읽힌다 */
enum { DEATH_NONE = 0, DEATH_SPIKE, DEATH_FORMAT, DEATH_FALL };

/* 거절 사유. 하나로 뭉뚱그리면 "터미널에서만"이라는 엉뚱한 안내가 뜬다 —
 * 디스크를 못 담는 것과 터미널 밖인 것은 플레이어가 할 일이 전혀 다르다. */
enum {
    DENY_NONE = 0,
    DENY_NOT_TERMINAL,   /* 터미널 밖에서 능력을 갈아끼우려 함 */
    DENY_ABSENT,         /* 이 섹터 터미널에는 그 유틸리티가 없다 */
    DENY_NO_SLOTS,       /* 장착할 칸이 모자란다 */
    DENY_DISK_SLOTS,     /* 디스크를 담을 칸이 모자란다 */
    DENY_EXIT_LOCKED     /* 디스크를 아직 안 주웠다 */
};

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
    /* 점프패드 — 그냥 정해진 속도로 쏘면 조작이 아니라 사고처럼 느껴진다.
     * 점프키를 누르고 있으면 더 높이 튄다. 플레이어가 개입할 여지를 남긴다. */
    float padVel;             /* 패드 점프에서 손을 뗐을 때 잘리는 바닥값 (음수) */
    float padVelHold;         /* 패드 점프의 최대 — 누르고 있으면 여기까지 */
    /* 죽으면 이 시간 동안은 입력을 무시하고 연출만 보여준다.
     * 그 뒤에는 **아무 키나 누를 때까지** 기다린다 — 되살아나는 타이밍은 사람이 정한다. */
    float deathFreeze;

    /* ── 지형 기믹 (2026-08-16) ── */
    float spikeCycle;         /* 교대 가시가 뒤집히는 주기(초) */
    float updraftRise;        /* 상승 기류의 상승 속도 (활공 중일 때만 먹는다) */
    float crumbleDelay;       /* 밟고 무너지기까지 */
    float crumbleRespawn;     /* 무너진 뒤 되돌아오기까지 */

    /* ── 포맷 파동 ── */
    float formatLead;         /* 스테이지를 시작할 때 파동이 뒤에 잡히는 거리 */
    float formatScale;        /* 레벨별 파동 속도에 곱하는 전역 배수 (난이도 조절용) */

    /* 터미널이 **보장하는** 최소 간격. 예전엔 터미널이 파동을 멈추기만 해서
     * 파동이 바짝 붙은 채로 도착하면 다음 구간이 통째로 불가능해졌다.
     * 뒤로 확 밀면 화면 밖으로 나가 안 보이므로(그래서 예전에 그 방식을 버렸다)
     * **화면 안에 남는 거리**만 보장한다. 240px = 15칸 < 화면 뒤쪽 256px. */
    float formatHold;

    /* 죽고 되살아난 직후 파동이 멈춰 있는 시간.
     * 예전엔 리스폰 때 파동을 formatLead 만큼 **뒤로 밀어줬다.** 그래서
     * 파동이 붙으면 일부러 죽는 게 최적 플레이였다 — 실패에 비용이 없었다.
     * 이제 되돌리는 건 체크포인트 당시 위치까지뿐이고(그 이상은 안 준다),
     * 무한 사망만 이 유예로 막는다. */
    float formatGrace;

    /* 터미널에 멈출 때마다 파동 속도에 누적으로 붙는 비율.
     *
     * ★ 터미널은 지금까지 순수 이득이었다 — 체크포인트 + 파동 정지 + 로드아웃 교체.
     *   그래서 "밟을까 말까"가 질문이 된 적이 없다. 쉬는 데 값을 붙이면
     *   이 게임의 코어인 터미널에 처음으로 긴장이 생긴다.
     *   서사와도 맞는다: 포맷은 멈춰준 만큼 나중에 더 빨리 온다. */
    float formatTax;
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
    int   padBoost;           /* 지금 상승이 점프패드에서 나온 것인가 — 점프컷 기준이 달라진다 */
    int   has[AB_COUNT];
} Player;

#define AB_BIT(a)   (1u << (a))
#define AB_ALL      ((1u << AB_COUNT) - 1u)
#define AB_STARTING AB_BIT(AB_BOOT)   /* boot.exe 는 처음부터 있다 — 부팅에 쓰던 것 */

/* 무너지는 발판의 진행 상태. **맵을 직접 바꾸되 원래 자리를 여기 들고 있는다** —
 * 리스폰할 때 전부 되돌려야 하기 때문이다. 되돌리는 걸 빠뜨리면
 * 두 번째 시도에서 발판이 없는 레벨이 된다. */
#define MAX_CRUMBLE 32
typedef struct {
    short r, c;
    float timer;
    unsigned char state;   /* 1 = 무너지는 중 · 2 = 사라져 있음 */
} CrumbleState;

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
    /* 마지막 터미널을 밟았을 때의 파동 위치. 죽으면 여기로 되돌린다.
     * 파동이 터미널에서 "멈추기만" 하게 되면서, 죽으면 파동이 리셋되는 게
     * 이득이 되어버렸다 — 체크포인트 당시 상황을 그대로 복원해서 막는다. */
    float formatAtCheckpoint;
    float formatGrace;     /* 남은 유예 시간. 0 보다 크면 파동이 안 온다 */
    int   formatStops;     /* 터미널에 멈춘 횟수 — 쉼 과금의 기준 */
    int   hasDisk;         /* 이 스테이지의 디스크를 회수했는가 */

    CrumbleState crumble[MAX_CRUMBLE];
    int   crumbleCount;

    /* 안내는 상태를 안 갖는다 — 힌트는 플레이어 위치의 함수라 렌더가 직접 계산한다 */

    /* 죽으면 바로 되살리지 않는다. 무슨 일이 일어났는지 보여줄 시간이 필요하다. */
    float deathTimer, deathTimerMax;   /* 연출 진행도 계산에 둘 다 필요하다 */
    int   deathCause;                  /* DEATH_* — NONE 이 아니면 죽어 있는 상태 */

    int   evJump, evDoubleJump, evDash, evWallJump, evLand, evDeny, evEquip;
    int   evCheckpoint, evDeath, evPad, evDisk, evCrumble;
    float denyFlash, checkFlash, diskFlash, padFlash;

    int   denyCause;       /* DENY_* — 왜 거절됐는지. 메시지가 갈려야 한다 */
    /* ★ 디스크·출구 거절은 **닿아 있는 내내** 조건이 참이다. 매 프레임 울리면
     *   효과음이 무한 반복된다. 닿은 순간 한 번만 울리도록 걸어 잠근다. */
    int   diskDenyHeld, exitDenyHeld;
} World;

typedef struct {
    int left, right, jumpHeld, jumpPressed, dashPressed;
    int toggleAbility;   /* -1 = 없음 */
    int anyKey;          /* 아무 키나 눌렸는가 — 사망 후 리스폰에 쓴다 */
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

/* 지금 이 순간 켜져 있는 교대 가시. T_SPIKE_A 또는 T_SPIKE_B 를 돌려준다.
 * 렌더도 사망 판정도 같은 함수를 본다 — 두 벌로 나누면 반드시 어긋난다. */
unsigned char sim_spike_phase(const World *w, const Tune *t);

/* 지금 파동이 실제로 나아가는 속도. 레벨값 × 전역배수 × 쉼 과금.
 * test.bat 이 압박을 계산할 때도 이걸 쓴다. */
float sim_format_speed(const World *w, const Tune *t, int stops);

/* 보상이 걸린 스테이지는 **디스크를 회수해야 출구가 열린다.**
 *
 * ★ 안 그러면 진행이 통째로 막힌다: 보상 능력은 디스크 안에 들어 있어서
 *   (sim_grant_reward) 디스크를 안 줍고 클리어하면 그 능력을 영영 못 얻고,
 *   그걸 needs 로 요구하는 다음 스테이지에서 갇힌다. */
int  sim_exit_locked(const World *w);

#endif
