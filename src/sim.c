#include "sim.h"
#include <string.h>
#include <math.h>

const char *const ABILITY_FILE[AB_COUNT] = {
    "jump2.exe", "dash.exe", "glide.exe", "grip.exe", "boot.exe"
};
const int ABILITY_COST[AB_COUNT] = { 5, 4, 4, 4, 3 };

/* ── 레벨 ────────────────────────────────────────────────────────────
 *
 * 96x18. 96자짜리 문자열 18줄은 손으로 못 맞추므로 사각형 목록으로 짓는다.
 *
 * 구간마다 **서로 다른 능력으로 가는 두 갈래 길**이 있는 게 핵심이다.
 * 관문마다 정답을 여러 개 두는 방식으로는 안 된다 — jump2 가 높이도 거리도
 * 최상위라 항상 상위호환이 되기 때문이다. 길 자체를 갈라야 한다.
 *
 *   구간1 (x1~31)   세로 장벽 9칸   : grip 으로 직등 / boot·jump2 로 3칸 발판
 *   구간2 (x32~61)  구덩이 6칸 x2   : dash / jump2        (평지라 glide 는 무의미)
 *   구간3 (x62~94)  저천장 통로     : dash·glide (2칸씩 내려가는 4칸 구덩이)
 *                   또는 위층       : grip 으로 9칸 벽면 등반
 *
 * 결과: 단독으로 전 구간을 여는 능력이 없다. jump2 는 구간3 에서 막힌다. */
typedef struct { short x, y, w, h; unsigned char t; } Box;

static const Box BOXES[] = {
    /* 테두리 */
    {  0,  0, 96,  1, T_SOLID },      /* 천장 */
    {  0,  0,  1, 18, T_SOLID },      /* 왼쪽 벽 */
    { 95,  0,  1, 18, T_SOLID },      /* 오른쪽 벽 */

    /* ── 구간 1 ── */
    {  1, 15, 31,  3, T_SOLID },      /* 바닥 */
    { 13, 12,  2,  1, T_SOLID },      /* 발판 (바닥에서 3칸) */
    { 16,  9,  2,  1, T_SOLID },      /* 발판 (3칸) */
    { 19,  6,  3,  9, T_SOLID },      /* 장벽. 꼭대기는 바닥에서 9칸 */

    /* ── 구간 2 ── */
    { 32, 15,  7,  3, T_SOLID },
    { 45, 15,  6,  3, T_SOLID },      /* 구덩이 x39~44 (6칸) */
    { 57, 15,  5,  3, T_SOLID },      /* 구덩이 x51~56 (6칸) */

    /* ── 구간 3 ──
     * 위층 덩어리의 아랫면이 row12 까지 내려와야 한다.
     * 그래야 바닥에서 점프(2.4칸)했을 때 손이 벽면에 닿아 grip 등반을 시작할 수 있다.
     * row10 까지만 내려오면 4칸이라 안 닿고, 위층 루트가 통째로 막힌다. */
    { 62,  6, 29,  7, T_SOLID },      /* 위층 x62~90, row6~12. 왼쪽 면이 등반 대상 */
    { 62, 15,  6,  3, T_SOLID },      /* 통로 바닥 (표면 row15) — 머리 위 여유 2칸 */
    { 72, 17, 23,  1, T_SOLID },      /* 구덩이 x68~71 을 건넌 뒤의 바닥 (2칸 아래) */
    { 72, 14, 19,  1, T_SOLID },      /* 낮은 천장 → 여기도 여유 2칸 */

    /* 터미널 — 여기서만 로드아웃을 바꿀 수 있다 */
    {  2, 14,  1,  1, T_TERMINAL },
    { 29, 14,  1,  1, T_TERMINAL },
    { 58, 14,  1,  1, T_TERMINAL },

    { 93, 16,  1,  1, T_GOAL },
};

void tune_defaults(Tune *t)
{
    t->gravity        = 900.0f;
    t->jumpVel        = -262.0f;
    t->jumpCut        = 0.45f;
    t->maxFall        = 420.0f;
    t->accelGround    = 1500.0f;
    t->accelAir       = 900.0f;
    t->maxSpeed       = 128.0f;
    t->frictionGround = 1700.0f;
    t->frictionAir    = 320.0f;
    t->coyoteTime     = 0.09f;
    t->jumpBuffer     = 0.10f;
    t->jump2Mult      = 0.92f;
    t->dashSpeed      = 340.0f;
    t->dashTime       = 0.18f;
    t->dashCooldown   = 0.35f;
    t->glideFall      = 60.0f;
    t->gripSlide      = 60.0f;
    t->gripJumpX      = 190.0f;
    t->gripJumpY      = -250.0f;
    t->gripStick      = 0.12f;
    t->bootMult       = 1.20f;
    t->formatSpeed    = 34.0f;
    t->formatLead     = 200.0f;
}

int tune_set(Tune *t, const char *key, float v)
{
#define K(name) if (strcmp(key, #name) == 0) { t->name = v; return 1; }
    K(gravity) K(jumpVel) K(jumpCut) K(maxFall)
    K(accelGround) K(accelAir) K(maxSpeed) K(frictionGround) K(frictionAir)
    K(coyoteTime) K(jumpBuffer)
    K(jump2Mult) K(dashSpeed) K(dashTime) K(dashCooldown)
    K(glideFall) K(gripSlide) K(gripJumpX) K(gripJumpY) K(gripStick) K(bootMult)
    K(formatSpeed) K(formatLead)
#undef K
    return 0;
}

/* ── 맵 ─────────────────────────────────────────────────────────────── */

static unsigned char map_at(const World *w, int r, int c)
{
    if (r < 0 || r >= MAP_H || c < 0 || c >= MAP_W) return T_EMPTY;
    return w->map[r][c];
}

static int hits_solid(const World *w, float x, float y)
{
    int c0 = (int)floorf(x / TILE), c1 = (int)floorf((x + PLAYER_W - 0.01f) / TILE);
    int r0 = (int)floorf(y / TILE), r1 = (int)floorf((y + PLAYER_H - 0.01f) / TILE);
    for (int r = r0; r <= r1; r++)
        for (int c = c0; c <= c1; c++)
            if (map_at(w, r, c) == T_SOLID) return 1;
    return 0;
}

static int touches(const World *w, unsigned char kind, int *outR, int *outC)
{
    const Player *p = &w->p;
    int c0 = (int)floorf(p->x / TILE), c1 = (int)floorf((p->x + PLAYER_W - 0.01f) / TILE);
    int r0 = (int)floorf(p->y / TILE), r1 = (int)floorf((p->y + PLAYER_H - 0.01f) / TILE);
    for (int r = r0; r <= r1; r++)
        for (int c = c0; c <= c1; c++)
            if (map_at(w, r, c) == kind) {
                if (outR) *outR = r;
                if (outC) *outC = c;
                return 1;
            }
    return 0;
}

int sim_used_slots(const World *w)
{
    int s = 0;
    for (int i = 0; i < AB_COUNT; i++) if (w->p.has[i]) s += ABILITY_COST[i];
    return s;
}
int sim_free_slots(const World *w) { return CAP_SLOTS - sim_used_slots(w); }

/* ── 초기화 ─────────────────────────────────────────────────────────── */

static void build_map(World *w)
{
    memset(w->map, T_EMPTY, sizeof(w->map));
    for (unsigned i = 0; i < sizeof(BOXES) / sizeof(BOXES[0]); i++) {
        const Box *b = &BOXES[i];
        for (int r = b->y; r < b->y + b->h; r++)
            for (int c = b->x; c < b->x + b->w; c++)
                if (r >= 0 && r < MAP_H && c >= 0 && c < MAP_W) w->map[r][c] = b->t;
    }
}

void sim_init(World *w)
{
    int deaths = w->deaths;
    int has[AB_COUNT];
    memcpy(has, w->p.has, sizeof(has));

    memset(w, 0, sizeof(*w));
    w->deaths = deaths;
    memcpy(w->p.has, has, sizeof(has));

    build_map(w);
    w->spawnX = 2 * TILE + 2.0f;
    w->spawnY = 15 * TILE - PLAYER_H;
    w->p.x = w->spawnX;
    w->p.y = w->spawnY;
    w->p.facing = 1;
    w->formatX = w->spawnX - 400.0f;
    w->state = ST_PLAY;
}

/* 죽으면 마지막 터미널에서 다시 시작한다 (맵과 로드아웃은 유지) */
static void respawn(World *w, const Tune *t)
{
    Player *p = &w->p;
    int has[AB_COUNT];
    memcpy(has, p->has, sizeof(has));
    memset(p, 0, sizeof(*p));
    memcpy(p->has, has, sizeof(has));
    p->x = w->spawnX;
    p->y = w->spawnY;
    p->facing = 1;
    w->formatX = w->spawnX - t->formatLead;
    w->state = ST_PLAY;
}

/* ── 이동 ───────────────────────────────────────────────────────────── */

static void move_x(World *w, float dx)
{
    Player *p = &w->p;
    float step = (dx > 0) ? 1.0f : -1.0f, remain = fabsf(dx);
    while (remain > 0.0f) {
        float s = (remain > 1.0f) ? 1.0f : remain;
        if (hits_solid(w, p->x + step * s, p->y)) { p->vx = 0; return; }
        p->x += step * s;
        remain -= s;
    }
}

static void move_y(World *w, float dy)
{
    Player *p = &w->p;
    float step = (dy > 0) ? 1.0f : -1.0f, remain = fabsf(dy);
    while (remain > 0.0f) {
        float s = (remain > 1.0f) ? 1.0f : remain;
        if (hits_solid(w, p->x, p->y + step * s)) {
            if (dy > 0) { if (!p->onGround) w->evLand = 1; p->onGround = 1; }
            p->vy = 0;
            return;
        }
        p->y += step * s;
        remain -= s;
    }
}

/* ── 한 스텝 ────────────────────────────────────────────────────────── */

void sim_step(World *w, const Tune *t, const Input *in, float dt)
{
    Player *p = &w->p;

    w->evJump = w->evDoubleJump = w->evDash = w->evWallJump = 0;
    w->evLand = w->evDeny = w->evEquip = w->evCheckpoint = 0;
    if (w->denyFlash  > 0) w->denyFlash  -= dt;
    if (w->checkFlash > 0) w->checkFlash -= dt;

    /* 로드아웃 — 터미널 위에서만. 이 잠금이 전략을 만든다. */
    if (in->toggleAbility >= 0 && in->toggleAbility < AB_COUNT) {
        int ab = in->toggleAbility;
        if (!w->atTerminal && w->state == ST_PLAY) {
            w->evDeny = 1; w->denyFlash = 0.5f;          /* 여기선 못 바꾼다 */
        } else if (p->has[ab]) {
            p->has[ab] = 0; w->evEquip = 1;
        } else if (sim_free_slots(w) >= ABILITY_COST[ab]) {
            p->has[ab] = 1; w->evEquip = 1;
        } else {
            w->evDeny = 1; w->denyFlash = 0.5f;          /* 칸이 모자란다 */
        }
    }

    if (w->state != ST_PLAY) return;
    w->time += dt;

    if (p->dashCd > 0) p->dashCd -= dt;
    if (p->wallStick > 0) p->wallStick -= dt;

    int dir = (in->right ? 1 : 0) - (in->left ? 1 : 0);
    if (dir != 0) p->facing = dir;

    if (p->dashTimer > 0.0f) {
        p->dashTimer -= dt;
        p->vx = p->dashDir * t->dashSpeed;
        p->vy = 0.0f;
    } else {
        if (in->dashPressed && p->has[AB_DASH] && p->dashCd <= 0.0f && !p->dashUsed) {
            p->dashTimer = t->dashTime;
            p->dashDir   = (dir != 0) ? dir : p->facing;
            p->dashCd    = t->dashCooldown;
            p->dashUsed  = 1;
            p->vy = 0.0f;
            w->evDash = 1;
        }

        float accel = p->onGround ? t->accelGround : t->accelAir;
        float fric  = p->onGround ? t->frictionGround : t->frictionAir;
        if (dir != 0) {
            p->vx += dir * accel * dt;
            if (p->vx >  t->maxSpeed) p->vx =  t->maxSpeed;
            if (p->vx < -t->maxSpeed) p->vx = -t->maxSpeed;
        } else if (p->wallStick <= 0.0f) {
            if (p->vx > 0) { p->vx -= fric * dt; if (p->vx < 0) p->vx = 0; }
            else           { p->vx += fric * dt; if (p->vx > 0) p->vx = 0; }
        }

        p->wallDir = 0;
        if (p->has[AB_GRIP] && !p->onGround) {
            if (hits_solid(w, p->x - 1.0f, p->y) && dir < 0) p->wallDir = -1;
            if (hits_solid(w, p->x + 1.0f, p->y) && dir > 0) p->wallDir = +1;
        }
        if (p->wallDir != 0) {
            p->dashUsed = 0;
            if (p->has[AB_JUMP2]) p->airJumps = 1;
            if (p->vy > t->gripSlide) p->vy = t->gripSlide;
        }

        if (in->jumpPressed) p->buffer = t->jumpBuffer;
        if (p->buffer > 0) p->buffer -= dt;
        if (p->onGround) p->coyote = t->coyoteTime;
        else if (p->coyote > 0) p->coyote -= dt;

        float boost = p->has[AB_BOOT] ? t->bootMult : 1.0f;

        if (p->buffer > 0.0f) {
            if (p->wallDir != 0) {
                p->vy = t->gripJumpY * boost;
                p->vx = -p->wallDir * t->gripJumpX;
                p->wallStick = t->gripStick;
                p->buffer = 0; p->wallDir = 0;
                w->evWallJump = 1;
            } else if (p->coyote > 0.0f) {
                p->vy = t->jumpVel * boost;
                p->buffer = 0; p->coyote = 0; p->onGround = 0;
                w->evJump = 1;
            } else if (p->airJumps > 0) {
                p->vy = t->jumpVel * boost * t->jump2Mult;
                p->airJumps--;
                p->buffer = 0;
                w->evDoubleJump = 1;
            }
        }
        if (!in->jumpHeld && p->vy < 0) {
            float cap = t->jumpVel * boost * (1.0f - t->jumpCut);
            if (p->vy < cap) p->vy = cap;
        }

        float cap = t->maxFall;
        if (p->has[AB_GLIDE] && !p->onGround && p->vy > 0.0f && in->jumpHeld)
            cap = t->glideFall;
        p->vy += t->gravity * dt;
        if (p->vy > cap) p->vy = cap;
    }

    p->onGround = 0;
    move_x(w, p->vx * dt);
    move_y(w, p->vy * dt);
    if (!p->onGround && hits_solid(w, p->x, p->y + 1.0f)) p->onGround = 1;
    if (p->onGround) {
        p->dashUsed = 0;
        p->airJumps = p->has[AB_JUMP2] ? 1 : 0;
    }

    /* ── 터미널 ── 밟으면 안전 + 체크포인트 + 로드아웃 해금 ── */
    int tr, tc;
    w->atTerminal = touches(w, T_TERMINAL, &tr, &tc);
    if (w->atTerminal) {
        float tx = tc * (float)TILE + 2.0f;
        if (fabsf(tx - w->spawnX) > 1.0f) {
            w->spawnX = tx;
            w->spawnY = (float)(tr * TILE) + (TILE - PLAYER_H);
            w->terminalsSeen++;
            w->evCheckpoint = 1; w->checkFlash = 1.0f;
        }
        /* 터미널 위에서는 포맷이 멈추고, 뒤로 물러나 있는다 */
        float want = p->x - t->formatLead;
        if (w->formatX > want) w->formatX = want;
    } else {
        w->formatX += t->formatSpeed * dt;
    }

    /* ── 포맷 파동 / 낙사 ── */
    if (p->x < w->formatX || p->y > VIEW_H + TILE) {
        w->deaths++;
        respawn(w, t);
        return;
    }

    if (touches(w, T_GOAL, 0, 0)) w->state = ST_WON;
}
