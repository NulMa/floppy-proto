#include "sim.h"
#include <string.h>
#include <math.h>

/* 순서 = 획득 순서 = 키 번호 (1~5) */
const char *const ABILITY_FILE[AB_COUNT] = {
    "boot.exe", "dash.exe", "glide.exe", "grip.exe", "jump2.exe"
};
const int ABILITY_COST[AB_COUNT] = { 3, 4, 4, 4, 5 };

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
    t->padVel         = -390.0f;   /* 정점 약 85px = 5.3칸 */
    t->padVelHold     = -520.0f;   /* 점프키를 누른 채 밟으면 150px = 9.4칸 */
    t->deathFreeze    = 0.45f;
    /* 세로 구간에서는 가로 진행이 0인데 파동은 계속 온다.
     * 200 이면 12칸 굴뚝을 오르는 동안(약 3.3초) 여유가 1.3배밖에 안 됐다.
     * 파동 속도를 낮추면 어려운 티어가 중간보다 느려져 난이도 곡선이 무너지므로
     * **출발 간격**을 올려서 푼다. 280 이면 전 스테이지가 1.8배 이상 확보된다. */
    t->formatLead     = 280.0f;
    t->formatScale    = 1.00f;
}

int tune_set(Tune *t, const char *key, float v)
{
#define K(name) if (strcmp(key, #name) == 0) { t->name = v; return 1; }
    K(gravity) K(jumpVel) K(jumpCut) K(maxFall)
    K(accelGround) K(accelAir) K(maxSpeed) K(frictionGround) K(frictionAir)
    K(coyoteTime) K(jumpBuffer)
    K(jump2Mult) K(dashSpeed) K(dashTime) K(dashCooldown)
    K(glideFall) K(gripSlide) K(gripJumpX) K(gripJumpY) K(gripStick) K(bootMult)
    K(padVel) K(padVelHold) K(deathFreeze) K(formatLead) K(formatScale)
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

/* 항상 막는 타일. 점프패드도 밟혀야 하므로 여기 들어간다. */
static int tile_blocks(unsigned char t) { return t == T_SOLID || t == T_PAD; }

static int hits_solid(const World *w, float x, float y)
{
    int c0 = (int)floorf(x / TILE), c1 = (int)floorf((x + PLAYER_W - 0.01f) / TILE);
    int r0 = (int)floorf(y / TILE), r1 = (int)floorf((y + PLAYER_H - 0.01f) / TILE);
    for (int r = r0; r <= r1; r++)
        for (int c = c0; c <= c1; c++)
            if (tile_blocks(map_at(w, r, c))) return 1;
    return 0;
}

/* 원웨이 발판 — 위에서 내려올 때만 막는다.
 * 1px 씩 움직이므로 "직전 발끝이 윗면 위에 있었나"로 판정하면 정확히 착지한다. */
static int oneway_blocks(const World *w, float x, float prevY, float newY)
{
    if (newY <= prevY) return 0;                   /* 올라가는 중이면 통과 */
    float prevBottom = prevY + PLAYER_H;
    float newBottom  = newY + PLAYER_H;
    int c0 = (int)floorf(x / TILE), c1 = (int)floorf((x + PLAYER_W - 0.01f) / TILE);
    int r  = (int)floorf((newBottom - 0.01f) / TILE);
    for (int c = c0; c <= c1; c++)
        if (map_at(w, r, c) == T_ONEWAY) {
            float top = r * (float)TILE;
            if (prevBottom <= top + 0.5f && newBottom > top) return 1;
        }
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
    /* 들고 있는 디스크도 용량을 먹는다 (마지막 스테이지) */
    if (w->hasDisk) s += LEVELS[w->levelIndex].diskCost;
    return s;
}
int sim_free_slots(const World *w) { return CAP_SLOTS - sim_used_slots(w); }

/* 아직 손에 넣은 적 없는 유틸리티는 목록에 아예 안 뜬다 */
int sim_ability_unlocked(const World *w, int ab)
{
    return (w->unlocked & AB_BIT(ab)) != 0;
}

/* 지금 이 터미널에서 꺼낼 수 있는가.
 * 해금됐고(unlocked) + 이 섹터에 남아 있어야(avail) 한다. avail 0 이면 섹터 제한 없음. */
int sim_ability_available(const World *w, int ab)
{
    if (!w->atTerminal) return 0;
    if (!sim_ability_unlocked(w, ab)) return 0;
    if (w->avail == 0) return 1;
    return (w->avail & AB_BIT(ab)) != 0;
}

/* 클리어 보상. **디스크를 회수했을 때만** 나온다 — 유틸리티는 그 디스크 안에 들어 있었다.
 * 이미 갖고 있던 건 빼고 새로 얻은 것만 반환한다 (연출용). */
unsigned sim_grant_reward(World *w)
{
    if (!w->hasDisk) return 0;
    unsigned r = sim_level(w)->reward & ~w->unlocked;
    w->unlocked |= r;
    return r;
}

/* ── 로드 ───────────────────────────────────────────────────────────── */

void sim_load(World *w, const Tune *t, int levelIndex)
{
    int deaths = w->deaths;
    unsigned unlocked = w->unlocked;
    int has[AB_COUNT];
    memcpy(has, w->p.has, sizeof(has));   /* 로드아웃·해금은 스테이지를 넘어 유지된다 */

    memset(w, 0, sizeof(*w));
    w->deaths = deaths;
    w->unlocked = unlocked;
    memcpy(w->p.has, has, sizeof(has));
    w->levelIndex = levelIndex;

    /* 해금이 풀린 능력이 장착돼 있으면 안 된다 (새 게임 등) */
    for (int a = 0; a < AB_COUNT; a++)
        if (w->p.has[a] && !(w->unlocked & AB_BIT(a))) w->p.has[a] = 0;

    const Level *L = &LEVELS[levelIndex];
    for (int i = 0; i < L->boxCount; i++) {
        const Box *b = &L->boxes[i];
        for (int r = b->y; r < b->y + b->h; r++)
            for (int c = b->x; c < b->x + b->w; c++)
                if (r >= 0 && r < MAP_H && c >= 0 && c < MAP_W) w->map[r][c] = b->t;
    }
    for (int i = 0; i < L->termCount; i++)
        w->map[L->terms[i].y][L->terms[i].x] = T_TERMINAL;
    w->map[L->exitRow][L->exitCol] = T_EXIT;
    if (L->diskCol >= 0) w->map[L->diskRow][L->diskCol] = T_DISK;

    w->spawnX = L->spawnCol * (float)TILE + 2.0f;
    w->spawnY = L->spawnRow * (float)TILE - PLAYER_H;
    w->p.x = w->spawnX;
    w->p.y = w->spawnY;
    w->p.facing = 1;
    w->formatX = w->spawnX - t->formatLead * 2.0f;
    w->formatAtCheckpoint = w->formatX;
    w->state = ST_PLAY;
}

void sim_new_game(World *w, const Tune *t)
{
    memset(w, 0, sizeof(*w));
    w->unlocked = AB_STARTING;
    sim_load(w, t, 0);
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
    /* 파동은 체크포인트 당시 위치로 되돌리되, **최소 formatLead 는 띄운다.**
     *
     * 터미널에서 파동이 멈추기만 하게 되면서, 파동이 바짝 붙은 채 터미널에
     * 도착하는 상황이 생긴다. 그 상태를 그대로 복원하면 등반 구간에서
     * 되살아나자마자 또 잡히는 **무한 사망**이 된다.
     * 일부러 죽어서 파동을 물리는 게 가능해지지만, 그 대가로 구간을 다시 뛰어야 하고
     * 무한 사망보다는 훨씬 낫다. */
    float floorX = w->spawnX - t->formatLead;
    w->formatX = (w->formatAtCheckpoint < floorX) ? w->formatAtCheckpoint : floorX;
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
        float ny = p->y + step * s;
        if (hits_solid(w, p->x, ny) || oneway_blocks(w, p->x, p->y, ny)) {
            if (dy > 0) p->onGround = 1;
            p->vy = 0;
            return;
        }
        p->y = ny;
        remain -= s;
    }
}

/* 발밑에 무엇이 있는가 (착지 판정 직후에 쓴다) */
static int tile_under(const World *w, unsigned char kind)
{
    const Player *p = &w->p;
    int r = (int)floorf((p->y + PLAYER_H + 1.0f) / TILE);
    int c0 = (int)floorf(p->x / TILE), c1 = (int)floorf((p->x + PLAYER_W - 0.01f) / TILE);
    for (int c = c0; c <= c1; c++)
        if (map_at(w, r, c) == kind) return 1;
    return 0;
}

/* ── 한 스텝 ────────────────────────────────────────────────────────── */

void sim_step(World *w, const Tune *t, const Input *in, float dt)
{
    Player *p = &w->p;
    const Level *L = sim_level(w);

    w->evJump = w->evDoubleJump = w->evDash = w->evWallJump = 0;
    w->evLand = w->evDeny = w->evEquip = w->evCheckpoint = w->evDeath = 0;
    w->evPad = w->evDisk = 0;
    if (w->denyFlash  > 0) w->denyFlash  -= dt;
    if (w->checkFlash > 0) w->checkFlash -= dt;
    if (w->diskFlash  > 0) w->diskFlash  -= dt;
    if (w->padFlash   > 0) w->padFlash   -= dt;

    /* ── 죽은 뒤 ──
     * 곧바로 되살리면 무슨 일이 일어났는지 못 본다. 가시를 밟았는지
     * 파동에 잡혔는지도 구분이 안 간다.
     *
     * deathFreeze 동안은 연출만 보여주고, **그 뒤엔 아무 키나 누를 때까지 기다린다.**
     * 자동으로 되살리면 마음의 준비가 안 된 채로 다시 시작된다. */
    if (w->deathCause != DEATH_NONE) {
        if (w->deathTimer > 0.0f) {
            w->deathTimer -= dt;
            if (w->deathTimer < 0.0f) w->deathTimer = 0.0f;
        } else if (in->anyKey) {
            w->deathCause = DEATH_NONE;
            respawn(w, t);
        }
        return;
    }

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

    /* ★ 착지는 **상태 변화**로 판정한다.
     *
     * onGround 를 매 프레임 0 으로 리셋한 뒤 충돌 안에서 "!onGround 면 착지"로
     * 판정하면, 가만히 서 있어도 매 프레임 착지가 된다 (초당 60번).
     * 소리를 붙이고 나서야 드러났다 — 서 있으면 착지음이 계속 나고
     * 점프하면(공중이라) 뚝 끊겼다. */
    int wasOnGround = p->onGround;

    p->onGround = 0;
    move_x(w, p->vx * dt);
    move_y(w, p->vy * dt);
    if (!p->onGround && hits_solid(w, p->x, p->y + 1.0f)) p->onGround = 1;
    if (p->onGround && !wasOnGround) w->evLand = 1;
    if (p->onGround) {
        p->dashUsed = 0;
        p->airJumps = p->has[AB_JUMP2] ? 1 : 0;

        /* 점프패드 — 능력 없이도 높이 올려준다. 레벨 문법이지 능력이 아니다.
         *
         * ★ 정해진 속도로 그냥 쏘면 조작이 아니라 사고처럼 느껴진다.
         *   **점프키를 누른 채 밟으면 더 높이** 튄다 (5.3칸 → 9.4칸).
         *   플레이어가 개입할 여지가 있어야 "조작"이 된다. */
        if (tile_under(w, T_PAD)) {
            p->vy = in->jumpHeld ? t->padVelHold : t->padVel;
            p->onGround = 0;
            p->coyote = 0;
            p->buffer = 0;      /* 패드 직후 점프버퍼가 남아 2단점프를 낭비하지 않게 */
            p->dashUsed = 0;
            p->airJumps = p->has[AB_JUMP2] ? 1 : 0;
            w->evPad = 1;
            w->padFlash = 0.25f;
        }
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
        w->formatAtCheckpoint = w->formatX;   /* 죽으면 이 상황으로 되돌린다 */

        /* ★ 터미널 위에서는 **멈춘다.** 뒤로 물리지 않는다.
         *
         * 예전엔 formatLead 만큼 뒤로 밀었는데, 그러면 화면 밖으로 나가서
         * 아예 사라진 것처럼 보였다. 멈춰 있으면 얼마나 가까운지가 계속 보이고,
         * 그게 "지금 출발해도 되나"를 판단하는 정보가 된다.
         *
         * 뒤로 물리는 건 죽어서 되살아날 때뿐이다 (respawn). */
    } else {
        w->formatX += L->formatSpeed * t->formatScale * dt;
    }

    /* ── 디스크 회수 — 선택이다. 없는 스테이지도 있다.
     * 용량을 먹는 디스크(마지막 스테이지)는 빈 칸이 모자라면 안 담긴다.
     * 능력을 버리러 터미널로 돌아가라는 뜻이다. */
    {
        int dr, dc;
        if (touches(w, T_DISK, &dr, &dc)) {
            if (L->diskCost <= 0 || sim_free_slots(w) >= L->diskCost) {
                w->map[dr][dc] = T_EMPTY;
                w->hasDisk = 1;
                w->evDisk = 1; w->diskFlash = 1.2f;
            } else {
                w->evDeny = 1; w->denyFlash = 0.5f;
            }
        }
    }

    /* ── 사망 ── 원인을 남긴다. 연출이 달라야 무슨 일인지 읽힌다 */
    {
        int cause = DEATH_NONE;
        if (touches(w, T_SPIKE, 0, 0))      cause = DEATH_SPIKE;
        else if (p->x < w->formatX)         cause = DEATH_FORMAT;
        else if (p->y > VIEW_H + TILE)      cause = DEATH_FALL;

        if (cause != DEATH_NONE) {
            w->deaths++;
            w->evDeath = 1;
            w->deathCause = cause;
            w->deathTimer = w->deathTimerMax = (t->deathFreeze > 0.05f) ? t->deathFreeze : 0.05f;
            p->vx = p->vy = 0.0f;
            p->dashTimer = 0.0f;
            return;
        }
    }

    /* ── 클리어는 출구로 한다 ── */
    if (touches(w, T_EXIT, 0, 0)) w->state = ST_WON;
}
