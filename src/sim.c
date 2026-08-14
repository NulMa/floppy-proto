#include "sim.h"
#include <string.h>
#include <math.h>

const char *const ABILITY_FILE[AB_COUNT] = {
    "jump2.exe", "dash.exe", "glide.exe", "grip.exe", "boot.exe"
};
const int ABILITY_COST[AB_COUNT] = { 5, 4, 4, 4, 3 };

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
    t->formatLead     = 200.0f;
    t->formatScale    = 1.00f;
    t->msgTime        = 4.00f;
}

int tune_set(Tune *t, const char *key, float v)
{
#define K(name) if (strcmp(key, #name) == 0) { t->name = v; return 1; }
    K(gravity) K(jumpVel) K(jumpCut) K(maxFall)
    K(accelGround) K(accelAir) K(maxSpeed) K(frictionGround) K(frictionAir)
    K(coyoteTime) K(jumpBuffer)
    K(jump2Mult) K(dashSpeed) K(dashTime) K(dashCooldown)
    K(glideFall) K(gripSlide) K(gripJumpX) K(gripJumpY) K(gripStick) K(bootMult)
    K(formatLead) K(formatScale) K(msgTime)
#undef K
    return 0;
}

const Level *sim_level(const World *w) { return &LEVELS[w->levelIndex]; }

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

/* 이 터미널에 그 유틸리티가 남아 있는가. avail 0 이면 전부 있다. */
int sim_ability_available(const World *w, int ab)
{
    if (!w->atTerminal) return 0;
    if (w->avail == 0) return 1;
    return (w->avail & (1u << ab)) != 0;
}

/* ── 로드 ───────────────────────────────────────────────────────────── */

void sim_load(World *w, const Tune *t, int levelIndex)
{
    int deaths = w->deaths;
    int has[AB_COUNT];
    memcpy(has, w->p.has, sizeof(has));   /* 로드아웃은 스테이지를 넘어 유지된다 */

    memset(w, 0, sizeof(*w));
    w->deaths = deaths;
    memcpy(w->p.has, has, sizeof(has));
    w->levelIndex = levelIndex;

    const Level *L = &LEVELS[levelIndex];
    for (int i = 0; i < L->boxCount; i++) {
        const Box *b = &L->boxes[i];
        for (int r = b->y; r < b->y + b->h; r++)
            for (int c = b->x; c < b->x + b->w; c++)
                if (r >= 0 && r < MAP_H && c >= 0 && c < MAP_W) w->map[r][c] = b->t;
    }
    for (int i = 0; i < L->termCount; i++)
        w->map[L->terms[i].y][L->terms[i].x] = T_TERMINAL;
    w->map[L->goalRow][L->goalCol] = T_GOAL;

    w->spawnX = L->spawnCol * (float)TILE + 2.0f;
    w->spawnY = L->spawnRow * (float)TILE - PLAYER_H;
    w->p.x = w->spawnX;
    w->p.y = w->spawnY;
    w->p.facing = 1;
    w->formatX = w->spawnX - t->formatLead * 2.0f;
    w->state = ST_PLAY;
}

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
    const Level *L = sim_level(w);

    w->evJump = w->evDoubleJump = w->evDash = w->evWallJump = 0;
    w->evLand = w->evDeny = w->evEquip = w->evCheckpoint = w->evDeath = 0;
    if (w->denyFlash  > 0) w->denyFlash  -= dt;
    if (w->checkFlash > 0) w->checkFlash -= dt;
    if (w->msgTimer   > 0) w->msgTimer   -= dt;

    /* 로드아웃 — 터미널 위에서, 그 터미널에 남아 있는 유틸리티만 */
    if (in->toggleAbility >= 0 && in->toggleAbility < AB_COUNT) {
        int ab = in->toggleAbility;
        if (!w->atTerminal) {
            w->evDeny = 1; w->denyFlash = 0.5f;
        } else if (p->has[ab]) {
            p->has[ab] = 0; w->evEquip = 1;              /* 빼는 건 언제나 가능 */
        } else if (!sim_ability_available(w, ab)) {
            w->evDeny = 1; w->denyFlash = 0.5f;          /* 이 섹터엔 없다 */
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

    /* ── 터미널 ── */
    int tr, tc;
    w->atTerminal = touches(w, T_TERMINAL, &tr, &tc);
    if (w->atTerminal) {
        w->avail = 0;
        for (int i = 0; i < L->termCount; i++)
            if (L->terms[i].x == tc && L->terms[i].y == tr) w->avail = L->terms[i].avail;

        float tx = tc * (float)TILE + 2.0f;
        if (fabsf(tx - w->spawnX) > 1.0f) {
            w->spawnX = tx;
            w->spawnY = (float)(tr * TILE) + (TILE - PLAYER_H);
            w->evCheckpoint = 1; w->checkFlash = 1.2f;
        }
        float want = p->x - t->formatLead;
        if (w->formatX > want) w->formatX = want;
    } else {
        w->formatX += L->formatSpeed * t->formatScale * dt;
    }

    /* ── 안내 문구 — 구간에 처음 들어올 때 한 번 ── */
    {
        int col = (int)((p->x + PLAYER_W * 0.5f) / TILE);
        for (int i = 0; i < L->msgCount && i < 32; i++) {
            if (w->msgShown & (1u << i)) continue;
            if (col >= L->msgs[i].x0 && col <= L->msgs[i].x1) {
                w->msgShown |= (1u << i);
                w->msgL1 = L->msgs[i].l1;
                w->msgL2 = L->msgs[i].l2;
                w->msgTimer = t->msgTime;
                break;
            }
        }
    }

    /* ── 사망 / 클리어 ── */
    if (p->x < w->formatX || p->y > VIEW_H + TILE) {
        w->deaths++;
        w->evDeath = 1;
        respawn(w, t);
        return;
    }
    if (touches(w, T_GOAL, 0, 0)) w->state = ST_WON;
}
