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

    t->spikeCycle     = 1.30f;
    t->updraftRise    = 90.0f;     /* grip 등반(약 58px/s)보다 빠르다 — 활공 루트의 값 */
    t->crumbleDelay   = 0.35f;
    t->crumbleRespawn = 2.50f;

    /* 스테이지를 시작할 때의 머리 시작 거리 */
    t->formatLead     = 400.0f;
    t->formatScale    = 1.00f;
    /* 터미널이 보장하는 최소 간격. 화면 뒤쪽으로 256px 이 보이므로 240 은 계속 보인다 */
    t->formatHold     = 240.0f;
    t->formatGrace    = 2.00f;
    t->formatTax      = 0.08f;     /* 터미널 한 번 쉴 때마다 +8% */
}

int tune_set(Tune *t, const char *key, float v)
{
#define K(name) if (strcmp(key, #name) == 0) { t->name = v; return 1; }
    K(gravity) K(jumpVel) K(jumpCut) K(maxFall)
    K(accelGround) K(accelAir) K(maxSpeed) K(frictionGround) K(frictionAir)
    K(coyoteTime) K(jumpBuffer)
    K(jump2Mult) K(dashSpeed) K(dashTime) K(dashCooldown)
    K(glideFall) K(gripSlide) K(gripJumpX) K(gripJumpY) K(gripStick) K(bootMult)
    K(padVel) K(padVelHold) K(deathFreeze)
    K(spikeCycle) K(updraftRise) K(crumbleDelay) K(crumbleRespawn)
    K(formatLead) K(formatScale) K(formatHold) K(formatGrace) K(formatTax)
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

/* 항상 막는 타일. 점프패드도 밟혀야 하므로 여기 들어간다.
 * 무너지는 발판은 **살아 있는 동안은 벽이다** — 사라질 때 T_EMPTY 로 바뀐다. */
static int tile_blocks(unsigned char t)
{
    return t == T_SOLID || t == T_PAD || t == T_CRUMBLE;
}

unsigned char sim_spike_phase(const World *w, const Tune *t)
{
    float cyc = (t->spikeCycle > 0.05f) ? t->spikeCycle : 0.05f;
    return (((int)(w->time / cyc)) & 1) ? T_SPIKE_B : T_SPIKE_A;
}

float sim_format_speed(const World *w, const Tune *t, int stops)
{
    /* 첫 터미널은 공짜다 — 스폰 지점이 대개 터미널 위라 앉자마자 과금되면 억울하다 */
    int billed = (stops > 1) ? stops - 1 : 0;
    return LEVELS[w->levelIndex].formatSpeed * t->formatScale
         * (1.0f + t->formatTax * (float)billed);
}

int sim_exit_locked(const World *w)
{
    return LEVELS[w->levelIndex].reward != 0 && !w->hasDisk;
}

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

/* ── 무너지는 발판 ─────────────────────────────────────────────────────
 *
 * ★ 사라진 발판은 **되살릴 때 플레이어와 겹치지 않는지 봐야 한다.**
 *   안 보면 발판이 몸 안에서 되살아나 벽에 끼인다 — 그 자리에서 영구 정지다.
 *   (같은 종류의 버그를 이 프로젝트에서 이미 한 번 냈다. 게임스펙.md §1 참고) */
static int overlaps_tile(const World *w, int r, int c)
{
    const Player *p = &w->p;
    float tx = c * (float)TILE, ty = r * (float)TILE;
    return p->x < tx + TILE && p->x + PLAYER_W > tx &&
           p->y < ty + TILE && p->y + PLAYER_H > ty;
}

/* 밟고 있는 무너짐 발판을 등록한다 (이미 등록된 건 건너뛴다) */
static void crumble_touch(World *w, const Tune *t)
{
    const Player *p = &w->p;
    if (!p->onGround) return;
    int r  = (int)floorf((p->y + PLAYER_H + 1.0f) / TILE);
    int c0 = (int)floorf(p->x / TILE), c1 = (int)floorf((p->x + PLAYER_W - 0.01f) / TILE);
    for (int c = c0; c <= c1; c++) {
        if (map_at(w, r, c) != T_CRUMBLE) continue;
        int seen = 0;
        for (int k = 0; k < w->crumbleCount; k++)
            if (w->crumble[k].r == r && w->crumble[k].c == c) { seen = 1; break; }
        if (seen || w->crumbleCount >= MAX_CRUMBLE) continue;
        CrumbleState *s = &w->crumble[w->crumbleCount++];
        s->r = (short)r; s->c = (short)c;
        s->timer = t->crumbleDelay; s->state = 1;
        w->evCrumble = 1;
    }
}

static void crumble_step(World *w, const Tune *t, float dt)
{
    for (int k = 0; k < w->crumbleCount; ) {
        CrumbleState *s = &w->crumble[k];
        s->timer -= dt;
        if (s->timer > 0.0f) { k++; continue; }

        if (s->state == 1) {                       /* 무너진다 */
            w->map[s->r][s->c] = T_EMPTY;
            s->state = 2;
            s->timer = t->crumbleRespawn;
            k++;
        } else if (overlaps_tile(w, s->r, s->c)) {  /* 몸에 겹치면 조금 더 기다린다 */
            s->timer = 0.15f;
            k++;
        } else {                                    /* 되돌아온다 */
            w->map[s->r][s->c] = T_CRUMBLE;
            w->crumble[k] = w->crumble[--w->crumbleCount];
        }
    }
}

/* 죽거나 스테이지를 다시 로드하면 전부 원래대로 */
static void crumble_reset(World *w)
{
    for (int k = 0; k < w->crumbleCount; k++)
        w->map[w->crumble[k].r][w->crumble[k].c] = T_CRUMBLE;
    w->crumbleCount = 0;
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

/* 이 스테이지 **시점에** 존재하는 유틸리티. 앞 스테이지들의 보상까지만이다.
 *
 * ★ 15까지 깨고 01 로 돌아가면 dash·glide 는 아직 **주운 적이 없는 파일**이다.
 *   전역 해금만 보고 목록에 띄우면 튜토리얼이 무너지고, 무엇보다 거짓말이다 —
 *   그 디스크는 아직 이 섹터 어딘가에 있다. */
static unsigned tools_by_level(int levelIndex)
{
    unsigned m = AB_STARTING;
    for (int i = 0; i < levelIndex && i < LEVEL_COUNT; i++) m |= LEVELS[i].reward;
    return m;
}

/* 아직 손에 넣은 적 없는 유틸리티는 목록에 아예 안 뜬다 */
int sim_ability_unlocked(const World *w, int ab)
{
    unsigned m = w->unlocked & tools_by_level(w->levelIndex);
    return (m & AB_BIT(ab)) != 0;
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

    /* 목록에 안 뜨는 능력이 장착된 채로 남으면 안 된다 —
     * 새 게임(해금 없음)도, 앞 스테이지로 되돌아간 경우(그때는 아직 없던 파일)도. */
    for (int a = 0; a < AB_COUNT; a++)
        if (w->p.has[a] && !sim_ability_unlocked(w, a)) w->p.has[a] = 0;

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
    w->formatX = w->spawnX - t->formatLead;
    w->formatAtCheckpoint = w->formatX;
    w->formatStops = 0;
    w->formatGrace = 0.0f;
    w->crumbleCount = 0;
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
    crumble_reset(w);

    /* ★ 2026-08-16 변경 — **죽는 것이 이득이던 것을 끊는다.**
     *
     *   예전엔 여기서 파동을 `spawnX - formatLead` 까지 **뒤로 밀어줬다.**
     *   그래서 파동이 붙으면 일부러 죽는 게 최적 플레이가 됐다 — 실패에 비용이 0이었다.
     *
     *   이제 되돌리는 건 **체크포인트 당시 위치까지뿐이다.** 그 이상은 안 준다.
     *   무한 사망은 formatGrace(리스폰 직후 파동 정지)로 막는다. 터미널 위에서는
     *   파동이 아예 안 오므로, 유예가 끝나도 플레이어는 언제든 다시 준비할 수 있다. */
    w->formatX = w->formatAtCheckpoint;
    w->formatGrace = t->formatGrace;
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
    w->evPad = w->evDisk = w->evCrumble = 0;
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
            w->evDeny = 1; w->denyFlash = 0.5f; w->denyCause = DENY_NOT_TERMINAL;
        } else if (p->has[ab]) {
            p->has[ab] = 0; w->evEquip = 1;              /* 빼는 건 언제나 가능 */
        } else if (!sim_ability_available(w, ab)) {
            w->evDeny = 1; w->denyFlash = 0.5f; w->denyCause = DENY_ABSENT;
        } else if (sim_free_slots(w) >= ABILITY_COST[ab]) {
            p->has[ab] = 1; w->evEquip = 1;
        } else {
            w->evDeny = 1; w->denyFlash = 0.5f; w->denyCause = DENY_NO_SLOTS;
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
                p->padBoost = 0;
                w->evWallJump = 1;
            } else if (p->onGround && tile_under(w, T_PAD)) {
                /* 점프패드 — 능력 없이도 높이 올려준다. 레벨 문법이지 능력이 아니다.
                 *
                 * ★ **밟는 것만으로는 안 튄다. 그 위에서 점프해야 튄다.**
                 *   지나가다 튀면 조작이 아니라 사고다. 밟고 서서 타이밍을 고를 수 있어야
                 *   패드가 "쓰는 것"이 된다. (버퍼로 들어오므로 착지 직전에 눌러도 먹는다) */
                p->vy = t->padVelHold;
                p->padBoost = 1;
                p->buffer = 0; p->coyote = 0; p->onGround = 0;
                p->dashUsed = 0;
                p->airJumps = p->has[AB_JUMP2] ? 1 : 0;
                w->evPad = 1;
                w->padFlash = 0.25f;
            } else if (p->coyote > 0.0f) {
                p->vy = t->jumpVel * boost;
                p->buffer = 0; p->coyote = 0; p->onGround = 0;
                p->padBoost = 0;
                w->evJump = 1;
            } else if (p->airJumps > 0) {
                p->vy = t->jumpVel * boost * t->jump2Mult;
                p->airJumps--;
                p->buffer = 0;
                p->padBoost = 0;
                w->evDoubleJump = 1;
            }
        }
        if (!in->jumpHeld && p->vy < 0) {
            /* 패드로 뜬 것은 점프가 아니다. 일반 점프컷(jumpVel 기준)에 걸리면
             * 손을 떼는 순간 패드가 보통 점프보다 낮아진다 — 패드가 죽는다.
             * 그래서 패드는 **자기 바닥값**으로 잘린다: 누르고 있으면 9.4칸, 떼면 5.3칸. */
            float cap = p->padBoost ? t->padVel : t->jumpVel * boost * (1.0f - t->jumpCut);
            if (p->vy < cap) p->vy = cap;
        }

        /* ── 활공과 상승 기류 ──
         *
         * 둘 다 **낙하속도 상한**을 바꾸는 것으로 끝난다. 기류는 그 상한이
         * 음수일 뿐이다 — 중력이 매 프레임 vy 를 밀어올려도 곧바로 되잘려서
         * 결과적으로 일정 속도로 떠오른다. 별도의 상태를 안 만드는 게 핵심이다.
         *
         * 기류가 활공을 요구하는 이유: 능력 없이 뜨면 그냥 공짜 엘리베이터라
         * 모든 세로 관문을 열어버린다. 점프패드가 마스터키였던 것과 같은 실수다. */
        float cap = t->maxFall;
        if (p->has[AB_GLIDE] && !p->onGround && in->jumpHeld) {
            if (touches(w, T_UPDRAFT, 0, 0)) cap = -t->updraftRise;
            else if (p->vy > 0.0f)           cap = t->glideFall;
        }
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
        p->padBoost = 0;        /* 발이 닿으면 패드 상승은 끝난 것이다 */
    }

    crumble_touch(w, t);
    crumble_step(w, t, dt);

    /* ── 터미널 ── */
    int tr, tc;
    int wasAtTerminal = w->atTerminal;
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

        /* ★ 쉼 과금 — 터미널에 **새로 올라설 때마다** 이 스테이지의 파동이 빨라진다.
         *   터미널이 순수 이득이면 "밟을까 말까"가 질문이 될 수 없다. */
        if (!wasAtTerminal) w->formatStops++;

        /* ★ 터미널 위에서는 **멈춘다.** 그리고 최소 formatHold 는 띄운다.
         *
         *   멈추기만 하던 시절엔 파동이 바짝 붙은 채로 도착할 수 있었고,
         *   그러면 뒤이은 등반 구간이 통째로 불가능해졌다.
         *   뒤로 확 밀면 화면 밖으로 나가 안 보이므로(예전에 그래서 버렸다)
         *   **화면 안에 남는 만큼만** 보장한다 — 얼마나 가까운지는 계속 보인다. */
        float minX = tc * (float)TILE - t->formatHold;
        if (w->formatX > minX) w->formatX = minX;

        w->formatAtCheckpoint = w->formatX;   /* 죽으면 이 상황으로 되돌린다 */
    } else if (w->formatGrace > 0.0f) {
        w->formatGrace -= dt;                 /* 되살아난 직후의 유예 */
    } else {
        w->formatX += sim_format_speed(w, t, w->formatStops) * dt;
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
                w->diskDenyHeld = 0;
            } else if (!w->diskDenyHeld) {
                /* 칸을 비우고 다시 오라는 뜻이다. 닿아 있는 동안은 한 번만 알린다 */
                w->diskDenyHeld = 1;
                w->evDeny = 1; w->denyFlash = 1.4f; w->denyCause = DENY_DISK_SLOTS;
            }
        } else {
            w->diskDenyHeld = 0;   /* 떨어졌다가 다시 오면 또 알려준다 */
        }
    }

    /* ── 사망 ── 원인을 남긴다. 연출이 달라야 무슨 일인지 읽힌다 */
    {
        int cause = DEATH_NONE;
        if (touches(w, T_SPIKE, 0, 0))                     cause = DEATH_SPIKE;
        else if (touches(w, sim_spike_phase(w, t), 0, 0))  cause = DEATH_SPIKE;
        else if (p->x < w->formatX)                        cause = DEATH_FORMAT;
        else if (p->y > VIEW_H + TILE)                     cause = DEATH_FALL;

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

    /* ── 클리어는 출구로 한다 ──
     * 단 보상이 걸린 스테이지는 디스크를 회수해야 열린다 (sim_exit_locked 주석 참고). */
    if (touches(w, T_EXIT, 0, 0)) {
        if (!sim_exit_locked(w)) w->state = ST_WON;
        else if (!w->exitDenyHeld) {          /* 디스크 때와 같은 이유로 한 번만 */
            w->exitDenyHeld = 1;
            w->evDeny = 1; w->denyFlash = 1.4f; w->denyCause = DENY_EXIT_LOCKED;
        }
    } else {
        w->exitDenyHeld = 0;
    }
}
