/* test.c — 헤드리스 검증. raylib 없이 sim.c + levels.c 만 돌린다.
 *
 * 플랫포머 AI 를 만들지 않는다. 대신 **레벨을 물리 수치·해금 곡선과 대조**한다.
 * 스테이지가 20개로 늘어나면 사람이 눈으로 못 잡는다. */
#include "sim.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int failures = 0;
static void check(int cond, const char *what)
{
    printf("  %s  %s\n", cond ? "[ OK ]" : "[FAIL]", what);
    if (!cond) failures++;
}

/* ══════════════════════════════════════════════════════════════════════
 *  도달 범위 — **실제 sim.c 물리를 그대로 따라간다.**
 *
 *  ★ 예전 공식은 두 가지를 빼먹어서 dash 를 2칸 넘게 **과소평가**했다.
 *    사람이 "활공 구간을 점프+대시로 깼다"고 알려주고 나서야 잡혔다.
 *
 *    ① 코요테 타임 — 발판을 벗어난 뒤 0.09초 동안 점프할 수 있다.
 *       그 사이에도 128px/s 로 나아가므로 **모든 가로 도달거리에 11.5px 이 붙는다.**
 *
 *    ② 대시는 중력을 멈춘다 (sim.c: dashTimer 동안 vy=0, 중력 스킵).
 *       그래서 대시 시간은 낙하 시간에 안 들어간다 — 가로로 공짜 61px 이다.
 *       게다가 **낙하 도중에 쓰면 쌓인 낙하속도가 지워져** 체공이 오히려 늘어난다.
 *       낙하 거리의 절반에서 쓸 때가 최대다.
 *
 *  두 번째가 특히 위험하다. "대시 = 조금 더 멀리 가는 점프"가 아니라
 *  **낙차가 클수록 이득이 커지는 능력**이다. 활공의 자리를 침범한다.
 * ══════════════════════════════════════════════════════════════════════ */

/* 정지 상태에서 h px 낙하하는 데 걸리는 시간 (종단속도 반영) */
static float fall_time(const Tune *t, float h)
{
    float g = t->gravity;
    float tCap = t->maxFall / g, dCap = t->maxFall * t->maxFall / (2.0f * g);
    if (h <= dCap) return sqrtf(2.0f * h / g);
    return tCap + (h - dCap) / t->maxFall;
}

static float rise_h(const Tune *t, float v0) { return v0 * v0 / (2.0f * t->gravity); }

/* 발판을 벗어난 뒤에도 점프가 되는 만큼 가로로 더 간다 */
static float coyote_gain(const Tune *t) { return t->maxSpeed * t->coyoteTime; }

/* 초속 v0 로 뛰어올라 drop px 아래에 착지할 때까지의 체공 시간 */
static float air_time(const Tune *t, float v0, float drop)
{
    return v0 / t->gravity + fall_time(t, rise_h(t, v0) + drop);
}

/* 같은 조건에서 **대시를 가장 유리한 타이밍(낙하 절반)에 쓴** 체공 시간.
 * 대시 자체의 0.18초는 중력이 멈추므로 여기 안 들어간다 — 가로 거리에만 더한다. */
static float air_time_dashed(const Tune *t, float v0, float drop)
{
    float H = rise_h(t, v0) + drop;
    return v0 / t->gravity + 2.0f * fall_time(t, H * 0.5f);
}

/* 2단 점프 — 정점에서 한 번 더 */
static float air_time_jump2(const Tune *t, float v0, float drop)
{
    float v2 = v0 * t->jump2Mult;
    return (v0 + v2) / t->gravity + fall_time(t, rise_h(t, v0) + rise_h(t, v2) + drop);
}

/* 활공 — 하강 내내 낙하속도가 glideFall 로 묶인다 */
static float air_time_glide(const Tune *t, float v0, float drop)
{
    return v0 / t->gravity + (rise_h(t, v0) + drop) / t->glideFall;
}

static float reach(const Tune *t, float airTime, int dashing)
{
    return t->maxSpeed * airTime + (dashing ? t->dashSpeed * t->dashTime : 0.0f) + coyote_gain(t);
}

typedef struct {
    float baseH, bootH, jump2H, bootJump2H;
    float baseD, bootD, dashD, jump2D, lowCeilD;
    float padH, padD;          /* 점프패드 — 능력이 아니라 지형이 주는 값 */
} Reach;

static Reach reach_of(const Tune *t)
{
    Reach r;
    float v0 = fabsf(t->jumpVel), bv = v0 * t->bootMult;

    r.baseH  = rise_h(t, v0);
    r.bootH  = rise_h(t, bv);
    r.jump2H = rise_h(t, v0) + rise_h(t, v0 * t->jump2Mult);
    /* ★ boot 와 jump2 를 같이 들면 2단 점프도 boot 배율을 받는다 — 6칸이 넘는다.
     *   높이 관문을 설계할 때 이걸 빼먹으면 그냥 뚫린다 */
    r.bootJump2H = rise_h(t, bv) + rise_h(t, bv * t->jump2Mult);

    r.baseD  = reach(t, air_time(t, v0, 0.0f), 0);
    r.bootD  = reach(t, air_time(t, bv, 0.0f), 0);
    r.dashD  = reach(t, air_time_dashed(t, v0, 0.0f), 1);
    r.jump2D = reach(t, air_time_jump2(t, v0, 0.0f), 0);

    /* ★ 점프패드는 **세로 기믹이 아니다.** 높이 올라간 만큼 체공이 길어지고,
     *   체공이 길어진 만큼 가로로도 간다. "패드는 세로라서 가로 관문은 안 흐린다"는
     *   틀렸다 — 아래 표가 그걸 보여준다. */
    float pv = fabsf(t->padVelHold);
    r.padH = rise_h(t, pv);
    r.padD = reach(t, air_time(t, pv, 0.0f), 0);

    /* 저천장 2칸 통로 — 머리가 천장에 닿아 상승이 잘린다 */
    float head = 2.0f * TILE - PLAYER_H;
    float tUp  = (v0 - sqrtf(fmaxf(0.0f, v0 * v0 - 2.0f * t->gravity * head))) / t->gravity;
    r.lowCeilD = t->maxSpeed * (tUp + sqrtf(2.0f * head / t->gravity)) + coyote_gain(t);
    return r;
}

/* 낙차가 있는 구덩이를 **가장 좋은 단일 조합**으로 건널 때의 가로 도달거리.
 * 레벨 관문은 "막고 싶은 조합"의 값보다 넓고 "통과시킬 조합"의 값보다 좁아야 한다. */
static float reach_drop_dash(const Tune *t, float drop, int boot)
{
    float v0 = fabsf(t->jumpVel) * (boot ? t->bootMult : 1.0f);
    return reach(t, air_time_dashed(t, v0, drop), 1);
}
static float reach_drop_glide(const Tune *t, float drop, int boot)
{
    float v0 = fabsf(t->jumpVel) * (boot ? t->bootMult : 1.0f);
    return reach(t, air_time_glide(t, v0, drop), 0);
}
static float reach_drop_plain(const Tune *t, float drop, int boot)
{
    float v0 = fabsf(t->jumpVel) * (boot ? t->bootMult : 1.0f);
    return reach(t, air_time(t, v0, drop), 0);
}

static int surface_row(const World *w, int c)
{
    for (int r = 1; r < MAP_H; r++)
        if (w->map[r][c] == T_SOLID && w->map[r - 1][c] != T_SOLID) return r;
    return -1;
}

/* rowFrom..rowTo 사이에 발 디딜 곳이 아예 없는 가장 긴 구간.
 * 특정 행 하나만 보면 바닥 높이가 변하는 구간을 잘못 잰다. */
static int widest_gap(const World *w, int rowFrom, int rowTo, int x0, int x1)
{
    int best = 0, run = 0;
    for (int c = x0; c <= x1; c++) {
        int floorHere = 0;
        for (int r = rowFrom; r <= rowTo; r++) {
            unsigned char t = w->map[r][c];
            /* 원웨이·점프패드·무너짐 발판도 밟을 수 있으니 바닥으로 센다.
             * T_SOLID 만 보면 원웨이 섬을 구덩이로 잘못 잰다 */
            if (t == T_SOLID || t == T_ONEWAY || t == T_PAD || t == T_CRUMBLE) floorHere = 1;
        }
        if (!floorHere) { run++; if (run > best) best = run; }
        else run = 0;
    }
    return best;
}

/* 이 레벨이 avail 마스크로 터미널마다 도구를 제한하는가.
 * 그런 스테이지는 "needs ≤ 8칸" 규칙에서 빠진다 — 갈아끼우는 것이 주제이기 때문이다. */
static int uses_avail(const Level *L)
{
    for (int i = 0; i < L->termCount; i++) if (L->terms[i].avail) return 1;
    return 0;
}

/* 위험 칸이 하나라도 있는 열인가 (가시·교대 가시). 밀도와 빈 구간을 재는 기준이다. */
static int hazard_col(const World *w, int c)
{
    for (int r = 0; r < MAP_H; r++) {
        unsigned char t = w->map[r][c];
        if (t == T_SPIKE || t == T_SPIKE_A || t == T_SPIKE_B) return 1;
    }
    return 0;
}

static int count_tile(const World *w, const Level *L, unsigned char kind)
{
    int n = 0;
    for (int r = 0; r < MAP_H; r++)
        for (int c = 0; c < L->width; c++)
            if (w->map[r][c] == kind) n++;
    return n;
}

/* 진입 시점에 해금돼 있는 능력 (앞 스테이지들의 보상까지) */
static unsigned unlocked_at(int levelIndex)
{
    unsigned m = AB_STARTING;
    for (int i = 0; i < levelIndex; i++) m |= LEVELS[i].reward;
    return m;
}

static int mask_cost(unsigned m)
{
    int c = 0;
    for (int a = 0; a < AB_COUNT; a++) if (m & AB_BIT(a)) c += ABILITY_COST[a];
    return c;
}

/* needs 를 채우고 남는 칸으로 만들 수 있는 **서로 다른 로드아웃 수**.
 *
 * ★ 이 값이 1 이면 그 스테이지에서는 고를 것이 없다 — 이 게임의 코어가 꺼진다.
 *   너무 크면(9~11) 반대로 남는 칸에 뭘 넣든 상관없다는 뜻이라 그것도 선택이 아니다.
 *   2~8 을 목표로 둔다. */
static int loadout_options(int levelIndex)
{
    const Level *L = &LEVELS[levelIndex];
    unsigned unl = unlocked_at(levelIndex);
    int budget = CAP_SLOTS - mask_cost(L->needs);
    int n = 0;
    for (unsigned m = 0; m < (1u << AB_COUNT); m++) {
        if (m & ~(unl & ~L->needs)) continue;      /* 아직 없거나 이미 필수인 건 제외 */
        if (mask_cost(m) <= budget) n++;
    }
    return n;
}

static void ability_list(unsigned mask, char *out, int n)
{
    out[0] = '\0';
    for (int a = 0; a < AB_COUNT; a++)
        if (mask & AB_BIT(a)) {
            if (out[0]) strncat(out, " ", n - strlen(out) - 1);
            strncat(out, ABILITY_FILE[a], n - strlen(out) - 1);
        }
    if (!out[0]) strncpy(out, "-", n);
}

int main(void)
{
    Tune t;
    tune_defaults(&t);
    Reach R = reach_of(&t);
    World w;
    char label[160], names[160];

    /* ★ tune.txt 를 **안 읽는다.** 코드 기본값(tune_defaults)으로 잰다 —
     *   제출물은 tune.txt 없이 도는 단일 exe 라 그게 실제로 배포되는 값이다.
     *   예전엔 여기 "tune.txt 기준"이라고 찍었는데 거짓말이었다 (2026-08-19). */
    printf("\n== 능력별 도달 범위 (코드 기본값 tune_defaults 기준) ==\n");
    printf("  높이  맨몸 %.1f칸  boot %.1f칸  jump2 %.1f칸  boot+jump2 %.1f칸  grip 제한없음\n",
           R.baseH / TILE, R.bootH / TILE, R.jump2H / TILE, R.bootJump2H / TILE);
    printf("  거리  맨몸 %.1f칸  boot %.1f칸  dash %.1f칸  jump2 %.1f칸  저천장 %.1f칸\n",
           R.baseD / TILE, R.bootD / TILE, R.dashD / TILE, R.jump2D / TILE, R.lowCeilD / TILE);
    printf("        (코요테 %.0fpx 포함. 대시는 중력이 멈추는 것까지 반영한 값이다)\n",
           coyote_gain(&t));
    printf("  패드  높이 %.1f칸  거리 %.1f칸   ← 능력이 아니라 지형이 준다\n",
           R.padH / TILE, R.padD / TILE);
    printf("  낙차별 가로 도달거리 — 활공만 통과시키려면 이 사이를 노려야 한다\n");
    printf("    낙차   맨몸   boot  boot+dash   활공   boot+활공\n");
    for (int d = 3; d <= 7; d++) {
        float D = (float)(d * TILE);
        printf("    %d칸  %5.1f  %5.1f      %5.1f  %5.1f      %5.1f   (칸)\n", d,
               reach_drop_plain(&t, D, 0) / TILE, reach_drop_plain(&t, D, 1) / TILE,
               reach_drop_dash(&t, D, 1) / TILE,
               reach_drop_glide(&t, D, 0) / TILE, reach_drop_glide(&t, D, 1) / TILE);
    }

    /* ── 용량 ── */
    printf("\n== 레벨 데이터 용량 ==\n");
    printf("  실측(2 vs 30 레벨 빌드 비교): 레벨당 713 B\n");
    printf("  현재 %d개 = 약 %.1f KB = 예산의 %.2f%%\n",
           LEVEL_COUNT, 713.0 * LEVEL_COUNT / 1024.0, 713.0 * LEVEL_COUNT / TOTAL_BYTES * 100.0);
    printf("  20개면 %.1f KB (%.2f%%). 레벨 수는 용량 제약을 받지 않는다.\n",
           713.0 * 20 / 1024.0, 713.0 * 20 / TOTAL_BYTES * 100.0);

    /* ── 해금 곡선 · 필요 능력 ──
     * 보상형 진행에서 제일 위험한 건 "그 시점 능력으로 못 깨는 스테이지"다. */
    printf("\n== 해금 곡선 vs 필요 능력 ==\n");
    {
        unsigned unl = AB_STARTING;
        for (int i = 0; i < LEVEL_COUNT; i++) {
            const Level *L = &LEVELS[i];
            int cost = 0;
            for (int a = 0; a < AB_COUNT; a++) if (unl & AB_BIT(a)) cost += ABILITY_COST[a];

            ability_list(L->needs, names, sizeof(names));
            printf("  [%s] 보유 %2d칸  필요 %-28s %s\n", L->id, cost, names,
                   cost > CAP_SLOTS ? "  <- 12칸 초과, 골라야 한다" : "");

            snprintf(label, sizeof(label), "%s: 필요 능력을 진입 시점에 갖고 있다", L->id);
            check((unl & L->needs) == L->needs, label);

            unl |= L->reward;
        }
        check((AB_STARTING & AB_BIT(AB_BOOT)) != 0, "boot 는 처음부터 있다");
        check(AB_BOOT == 0, "키 1번이 첫 능력(boot) — 획득 순서 = 키 순서");
    }

    /* ── 레벨별 구조 ── */
    printf("\n== 레벨 구조 ==\n");
    for (int i = 0; i < LEVEL_COUNT; i++) {
        memset(&w, 0, sizeof(w));
        sim_load(&w, &t, i);
        const Level *L = sim_level(&w);

        int exits = 0, disks = 0, terms = 0, spikes = 0, pads = 0;
        for (int r = 0; r < MAP_H; r++)
            for (int c = 0; c < MAP_W; c++) {
                if (w.map[r][c] == T_EXIT)     exits++;
                if (w.map[r][c] == T_DISK)     disks++;
                if (w.map[r][c] == T_TERMINAL) terms++;
                if (w.map[r][c] == T_SPIKE)    spikes++;
                if (w.map[r][c] == T_PAD)      pads++;
            }
        printf("  [%s] %-13s w%2d  term %d  hint %2d  spike %2d  pad %d  %.0fpx/s\n",
               L->id, L->file, L->width, terms, L->hintCount, spikes, pads, L->formatSpeed);

        snprintf(label, sizeof(label), "%s: 출구 1개", L->id);
        check(exits == 1, label);
        snprintf(label, sizeof(label), "%s: 디스크 0 또는 1개", L->id);
        check(disks == (L->diskCol >= 0 ? 1 : 0), label);
        snprintf(label, sizeof(label), "%s: 보상이 있으면 디스크도 있다", L->id);
        check(L->reward == 0 || L->diskCol >= 0, label);
        snprintf(label, sizeof(label), "%s: 스폰 아래에 바닥", L->id);
        check(surface_row(&w, L->spawnCol) == L->spawnRow, label);

        /* 출구·디스크·터미널이 공중에 떠 있으면 못 닿는다 */
        snprintf(label, sizeof(label), "%s: 출구가 발판 위에 있다", L->id);
        check(w.map[L->exitRow + 1][L->exitCol] == T_SOLID, label);

        int termOK = 1;
        for (int k = 0; k < L->termCount; k++)
            if (w.map[L->terms[k].y + 1][L->terms[k].x] != T_SOLID) termOK = 0;
        snprintf(label, sizeof(label), "%s: 터미널이 전부 발판 위에 있다", L->id);
        check(termOK, label);

        /* ── 디스크 ── 2026-08-16 에 규칙을 **뒤집었다.**
         *
         * 예전 규칙은 "디스크가 출구 8칸 이내"였다. 그게 20 스테이지 중 18개에서
         * 디스크를 출구 옆에 붙여놨고, 결과적으로 **위험/보상 층을 금지하고 있었다** —
         * 회수가 선택이 아니라 통행세였다.
         *
         * 지금은 둘로 나눈다.
         *   보상 스테이지 : 디스크가 곧 진행이다. 출구가 잠기므로(sim_exit_locked)
         *                  어디 둬도 되지만, 놓치면 안 되니 경로 위에 둔다
         *   나머지        : **실제 우회 비용**이 있어야 한다.
         *                  가로로 10칸 넘게 떨어져 있거나, 4칸 넘게 위에 있거나 */
        if (L->diskCol >= 0 && L->diskCost == 0 && L->reward == 0 && i >= 5) {
            int dx = abs(L->diskCol - L->exitCol), dy = L->exitRow - L->diskRow;
            snprintf(label, sizeof(label), "%s: 디스크 회수에 우회 비용이 있다 (가로 %d칸 · 위로 %d칸)",
                     L->id, dx, dy);
            check(dx >= 10 || dy >= 4, label);
        }

        /* ── 로드아웃 자유도 ──
         *
         * ★ needs 합이 12칸이면 유효 로드아웃이 **하나**가 된다. 예전에 s09·s18·s19·s20
         *   넷이 그랬고, 그게 "무엇을 들고 갈까"라는 이 게임의 코어를 후반부에서
         *   통째로 껐다. 8칸으로 묶으면 최소 4칸이 항상 플레이어 몫으로 남는다.
         *   (avail 로 갈아끼우게 만드는 스테이지는 그게 주제이므로 면제) */
        {
            int nc = mask_cost(L->needs), opts = loadout_options(i);
            int pool = mask_cost(unlocked_at(i));
            snprintf(label, sizeof(label), "%s: needs 가 %d칸 (8칸 이하)", L->id, nc);
            check(nc <= 8 || uses_avail(L), label);

            /* 아직 해금된 걸 전부 들 수 있는 초반(합 ≤ 12칸)에는 고를 것이 없는 게 정상이다.
             * 12칸을 넘긴 뒤부터가 진짜다 — 거기서 선택지가 1이면 코어가 꺼진 것이다. */
            if (pool > CAP_SLOTS && !uses_avail(L)) {
                snprintf(label, sizeof(label),
                         "%s: 버릴 것을 고를 수 있다 (해금 %d칸 · 필수 %d칸 · %d가지)",
                         L->id, pool, nc, opts);
                check(opts >= 2, label);
            }
        }

        /* ── 새 타일의 성립 조건 (2026-08-16) ── */
        {
            int upd = count_tile(&w, L, T_UPDRAFT);
            int sa  = count_tile(&w, L, T_SPIKE_A), sb = count_tile(&w, L, T_SPIKE_B);
            /* 기류는 활공이 없으면 못 쓴다. 아직 glide 가 안 풀린 스테이지에 두면
             * 그냥 못 지나가는 벽이 된다 */
            snprintf(label, sizeof(label), "%s: 기류를 두려면 glide 가 해금돼 있어야 한다", L->id);
            check(upd == 0 || (unlocked_at(i) & AB_BIT(AB_GLIDE)), label);
            /* 교대 가시는 **짝이 있어야** 의미가 있다. 한쪽만 두면 그냥 깜빡이는 가시다 */
            snprintf(label, sizeof(label), "%s: 교대 가시는 A 와 B 가 짝을 이룬다", L->id);
            check((sa == 0 && sb == 0) || (sa > 0 && sb > 0), label);
        }

        if (L->diskCost > 0) {
            int left = CAP_SLOTS - L->diskCost;
            printf("      디스크 %d칸 → 회수 후 %d칸 남음\n", L->diskCost, left);
            snprintf(label, sizeof(label), "%s: 디스크를 담아도 boot 는 들 수 있다", L->id);
            check(left >= ABILITY_COST[AB_BOOT], label);
            snprintf(label, sizeof(label), "%s: 디스크가 실제로 용량을 조인다", L->id);
            check(left < CAP_SLOTS / 2, label);
        }

        int badHint = 0;
        for (int h = 0; h < L->hintCount; h++)
            if (L->hints[h].x < 0 || L->hints[h].x >= L->width ||
                L->hints[h].y < 1 || L->hints[h].y >= MAP_H || L->hints[h].l1 == 0)
                badHint = 1;
        snprintf(label, sizeof(label), "%s: 힌트 앵커가 레벨 안에 있다", L->id);
        check(!badHint, label);

        /* ★ 예전엔 상한이 maxSpeed*0.5 였다. 그게 **파동이 압박을 만들 수 있는 한계를
         *   설계 규칙으로 못박고 있었다** — 어떤 스테이지도 실제로 쫓기지 않았다.
         *   0.70 으로 올리고, 쉼 과금이 다 붙은 최고 속도도 0.90 아래로만 묶는다. */
        float topSpeed = L->formatSpeed * t.formatScale * (1.0f + t.formatTax * (L->termCount - 1));
        snprintf(label, sizeof(label), "%s: 기본 파동이 플레이어보다 느리다", L->id);
        check(L->formatSpeed * t.formatScale < t.maxSpeed * 0.70f, label);
        snprintf(label, sizeof(label), "%s: 쉼 과금이 다 붙어도 따라잡히지는 않는다 (%.0fpx/s)",
                 L->id, topSpeed);
        check(topSpeed < t.maxSpeed * 0.90f, label);
    }

    /* ── 밀도와 빈 구간 ──────────────────────────────────────────────
     *
     * ★ 이 표가 2026-08-16 진단의 핵심이었다. 예전 값은 장애물 간격 10~70칸,
     *   무위험 연속 구간 최대 42칸이었다. **화면이 32칸**이라 42칸이면
     *   한 화면 반 동안 아무 일도 안 일어난다는 뜻이다. */
    printf("\n== 밀도 ==\n");
    printf("  %-14s %5s %6s %7s %9s %8s\n", "레벨", "폭", "위험열", "장애물", "장애간격", "최장무위험");
    for (int i = 0; i < LEVEL_COUNT; i++) {
        memset(&w, 0, sizeof(w));
        sim_load(&w, &t, i);
        const Level *L = sim_level(&w);

        int haz = 0, obst = 0, inHaz = 0, run = 0, bestSafe = 0;
        for (int c = 1; c < L->width - 1; c++) {
            if (hazard_col(&w, c)) {
                haz++;
                if (!inHaz) obst++;
                inHaz = 1; run = 0;
            } else {
                inHaz = 0; run++;
                if (run > bestSafe) bestSafe = run;
            }
        }
        /* ★ 간격은 **폭이 아니라 안전한 땅**을 장애물 수로 나눈다.
         *   폭으로 나누면 s04(16칸 구덩이 세 개)처럼 장애물이 거대한 레벨이
         *   "비었다"로 잘못 잡힌다 — 그 레벨은 절반이 구덩이인데도. */
        int safeCols = (L->width - 2) - haz;
        float spacing = obst ? (float)safeCols / obst : 999.0f;
        printf("  %-14s %4d %5d%% %6d %8.1f %8d%s\n",
               L->file, L->width, 100 * haz / (L->width - 2), obst, spacing, bestSafe,
               bestSafe > 20 ? "  <- 비었다" : "");

        snprintf(label, sizeof(label), "%s: 장애물 사이 안전지대가 13칸 이하 (%.1f)", L->id, spacing);
        check(spacing <= 13.0f, label);
        snprintf(label, sizeof(label), "%s: 무위험 연속 구간이 20칸 이하 (%d)", L->id, bestSafe);
        check(bestSafe <= 20, label);
    }

    /* ── 밸런스: 파동 압박 ────────────────────────────────────────────
     *
     * ★★ 2026-08-16 에 모델을 통째로 바꿨다.
     *
     *   예전 모델은 "레벨 전체의 세로 스팬을 오르는 시간 vs formatLead/속도"였다.
     *   두 가지가 틀렸다.
     *     ① **가로 압박을 아예 안 쟀다.** 실제로 재보니 여유가 3.4~10.9배였다 —
     *        즉 20 스테이지 중 어디서도 파동이 위협이 된 적이 없다
     *     ② 하한(1.5배)만 있고 **상한이 없어서** 11배도 99배도 통과했다
     *
     *   새 모델은 **터미널 사이 한 구간을 실제로 뛰어본다.**
     *     생존조건 :  S + D  >  v × T
     *       S = 출발 시 파동과의 간격 (터미널이 formatHold 를 보장한다)
     *       D = 다음 터미널(또는 출구)까지의 가로 거리
     *       T = D/실주행속도 + 등반높이/58   (실주행 = maxSpeed 의 75%.
     *           점프·감속·착지 때문에 최고속도로는 못 달린다)
     *       v = 그 구간의 파동 속도 (쉼 과금이 누적된 값)
     *
     *   여유 = (S+D) / (v×T). **1.25 아래면 클리어 불가에 가깝고,
     *   2.6 위면 파동이 있으나 마나다.** 양쪽을 다 잡는다. */
    printf("\n== 파동 압박 (구간별 최악값) ==\n");
    printf("  %-14s %-6s %-7s %-7s %-8s %s\n", "레벨", "파동", "최악구간", "등반", "여유", "");
    {
        const float runSpeed = 0.75f;      /* 실주행 속도 = maxSpeed × 이것 */
        for (int i = 0; i < LEVEL_COUNT; i++) {
            memset(&w, 0, sizeof(w));
            sim_load(&w, &t, i);
            const Level *L = sim_level(&w);

            float worst = 99.0f;
            int   worstAt = 0, worstRise = 0;

            for (int k = 0; k < L->termCount; k++) {
                int x0 = L->terms[k].x;
                /* 이 터미널 다음으로 오른쪽에 있는 터미널 (없으면 출구) */
                int x1 = L->exitCol;
                for (int m = 0; m < L->termCount; m++)
                    if (L->terms[m].x > x0 && L->terms[m].x < x1) x1 = L->terms[m].x;
                if (x1 <= x0) continue;

                /* ── 이 구간에서 **실제로 올라가야 하는** 높이 ──
                 *
                 * ★ 단순히 "구간 안 가장 높은 표면"으로 재면 s15 처럼 **선택 회수용
                 *   굴뚝**이 있는 레벨이 잘못 잡힌다 — 안 올라가도 되는 곳인데
                 *   올라가는 것으로 친다. 두 가지를 나눠 본다.
                 *     ① 다음 체크포인트까지의 **순 상승** — 이건 언제나 해야 한다
                 *     ② 바닥 높이로 쭉 갈 수 있으면(지상 루트가 있으면) 중간 봉우리는 무시,
                 *        중간에 허공이 있으면 그 봉우리를 넘는 수밖에 없으므로 포함 */
                int base = L->terms[k].y + 1;
                int endRow = (x1 == L->exitCol) ? L->exitRow + 1 : -1;
                if (endRow < 0)
                    for (int m = 0; m < L->termCount; m++)
                        if (L->terms[m].x == x1) endRow = L->terms[m].y + 1;
                if (endRow < 0) endRow = base;

                int rise = base - endRow; if (rise < 0) rise = 0;
                int peak = 0, ground = 1;
                for (int c = x0; c <= x1 && c < L->width; c++) {
                    int s = surface_row(&w, c);
                    if (s >= 0 && base - s > peak) peak = base - s;
                    /* ★ 지상 루트 판정은 **바닥 근처에 디딜 것이 있는가**로 본다.
                     *   surface_row(가장 위 표면)로 보면 안 된다 — s15 의 굴뚝처럼
                     *   머리 위로 벽이 솟아 있어도 그 아래로 그냥 걸어가는 경우를
                     *   "올라가야 한다"고 잘못 읽는다. */
                    int floorNear = 0;
                    for (int r = base - 1; r <= base + 1 && r < MAP_H; r++) {
                        if (r < 0) continue;
                        unsigned char tt = w.map[r][c];
                        if (tt == T_SOLID || tt == T_ONEWAY || tt == T_PAD || tt == T_CRUMBLE)
                            floorNear = 1;
                    }
                    if (!floorNear) ground = 0;
                }
                if (!ground && peak > rise) rise = peak;

                /* ── 오르는 수단에 따라 등반 속도가 다르다 ──
                 *   벽타기 58px/s (벽점프 1회 약 35px, 왕복 0.6초)
                 *   상승 기류는 tune 값 그대로, 점프패드는 사실상 쏘아 올린다 */
                float climbSpeed = 58.0f;
                for (int c = x0; c <= x1 && c < L->width; c++)
                    for (int r = 0; r < MAP_H; r++) {
                        if (w.map[r][c] == T_UPDRAFT && climbSpeed < t.updraftRise)
                            climbSpeed = t.updraftRise;
                        if (w.map[r][c] == T_PAD) climbSpeed = 220.0f;
                    }

                float D = (float)(x1 - x0) * TILE;
                float T = D / (t.maxSpeed * runSpeed) + (float)(rise * TILE) / climbSpeed;
                float v = L->formatSpeed * t.formatScale * (1.0f + t.formatTax * k);
                float margin = (T > 0.05f) ? (t.formatHold + D) / (v * T) : 99.0f;

                if (margin < worst) { worst = margin; worstAt = x0; worstRise = rise; }
            }

            printf("  %-14s %4.0f  x%-6d %4d칸 %6.2fx  %s\n",
                   L->file, L->formatSpeed, worstAt, worstRise, worst,
                   worst < 1.25f ? "<- 빠듯하다" : (worst > 2.6f ? "<- 헐겁다" : ""));

            snprintf(label, sizeof(label), "%s: 어느 구간도 파동에 못 잡힌다 (%.2fx)", L->id, worst);
            check(worst >= 1.25f, label);
            snprintf(label, sizeof(label), "%s: 파동이 실제로 압박이 된다 (%.2fx ≤ 2.6)", L->id, worst);
            check(worst <= 2.6f, label);
        }
    }

    /* ── 밸런스: 티어 간 난이도 곡선 ──
     * 티어가 올라가면 평균 파동 속도도 올라가야 한다. 한 스테이지가 튀면
     * 그 뒤가 오히려 쉬워 보인다 (s10 이 48 이라 어려운 티어보다 빨랐다). */
    printf("\n== 티어별 난이도 ==\n");
    {
        float sum[3] = { 0, 0, 0 }, mx[3] = { 0, 0, 0 };
        int   n[3] = { 0, 0, 0 };
        for (int i = 0; i < LEVEL_COUNT; i++) {
            int tier = (i < 5) ? 0 : (i < 15 ? 1 : 2);
            sum[tier] += LEVELS[i].formatSpeed;
            if (LEVELS[i].formatSpeed > mx[tier]) mx[tier] = LEVELS[i].formatSpeed;
            n[tier]++;
        }
        const char *tn[3] = { "EASY  ", "MEDIUM", "HARD  " };
        for (int k = 0; k < 3; k++)
            if (n[k]) printf("  %s  %d개  평균 %.0fpx/s  최대 %.0f\n", tn[k], n[k], sum[k] / n[k], mx[k]);
        if (n[0] && n[1] && n[2]) {
            check(sum[0] / n[0] < sum[1] / n[1], "쉬움 < 중간 (평균 파동 속도)");
            check(sum[1] / n[1] < sum[2] / n[2], "중간 < 어려움 (평균 파동 속도)");
        }

        /* ── 톱니 구조 ──
         * 계단처럼 단조 상승하면 안 된다. 새 능력을 얻은 **직후 스테이지는
         * 낮은 위험에서 그걸 소개**해야 하므로 압박을 떨어뜨린다.
         * (Nintendo 4단 구성 / Celeste 챕터 구조의 공통 원칙) */
        printf("\n  능력 도입 스테이지의 압박 하강\n");
        for (int i = 1; i < LEVEL_COUNT; i++) {
            if (LEVELS[i - 1].reward == 0) continue;
            printf("    %s (%.0f) → %s (%.0f)  %s\n",
                   LEVELS[i - 1].id, LEVELS[i - 1].formatSpeed,
                   LEVELS[i].id, LEVELS[i].formatSpeed,
                   LEVELS[i].formatSpeed <= LEVELS[i - 1].formatSpeed ? "내려감" : "<- 올라갔다");
            snprintf(label, sizeof(label), "%s: 새 능력 도입 스테이지는 압박을 낮춘다", LEVELS[i].id);
            check(LEVELS[i].formatSpeed <= LEVELS[i - 1].formatSpeed, label);
        }

        /* 티어 안에서도 최소 한 번은 내려가야 쉬는 구간이 생긴다 */
        for (int k = 0; k < 3; k++) {
            int lo = (k == 0) ? 0 : (k == 1 ? 5 : 15);
            int hi = (k == 0) ? 5 : (k == 1 ? 15 : 20);
            if (hi > LEVEL_COUNT) hi = LEVEL_COUNT;
            int drops = 0;
            for (int i = lo + 1; i < hi; i++)
                if (LEVELS[i].formatSpeed < LEVELS[i - 1].formatSpeed) drops++;
            if (hi - lo >= 3) {
                snprintf(label, sizeof(label), "%s 티어 안에 쉬는 구간이 있다 (하강 %d회)", tn[k], drops);
                check(drops >= 1, label);
            }
        }
    }

    /* ── 01 튜토리얼: 관문이 실제로 그 능력을 요구하는가 ── */
    printf("\n== 01 튜토리얼 ==\n");
    memset(&w, 0, sizeof(w));
    sim_load(&w, &t, 0);
    {
        const Level *L = sim_level(&w);
        float step  = (float)(surface_row(&w, 2) - surface_row(&w, 10)) * TILE;
        float ledge = (float)(surface_row(&w, 2) - surface_row(&w, 22)) * TILE;
        float gap   = (float)widest_gap(&w, 15, 17, 1, L->width - 2) * TILE;
        printf("  계단 %.0fpx / 구덩이 %.0fpx / 턱 %.0fpx\n", step, gap, ledge);
        check(step  <= R.baseH, "계단은 맨몸으로 넘어진다");
        check(gap   <= R.baseD, "구덩이는 맨몸으로 건너진다");
        check(ledge >  R.baseH, "턱은 맨몸으로 못 넘는다");
        check(ledge <= R.bootH, "턱은 boot 로 넘어진다");
        int nearTerm = 0;
        for (int k = 0; k < L->termCount; k++)
            if (L->terms[k].x >= 15 && L->terms[k].x <= 19) nearTerm = 1;
        check(nearTerm, "턱 바로 앞에 터미널이 있다");
    }

    /* ── 02 · 03: 관문이 dash 를 강제하는가 ── */
    printf("\n== 02 CONFIG.SYS / 03 LETTER.TXT ==\n");
    memset(&w, 0, sizeof(w));
    sim_load(&w, &t, 1);
    {
        float gap = (float)widest_gap(&w, 15, 17, 1, LEVELS[1].width - 2) * TILE;
        printf("  02 최대 구덩이 %.0fpx  (boot %.0f · jump2 %.0f · dash %.0f)\n",
               gap, R.bootD, R.jump2D, R.dashD);
        check(gap >  R.bootD,  "02: boot 로는 못 건넌다");
        /* ★ 이 줄이 2026-08-16 에 추가됐다. 예전엔 8칸(128px)이었는데 jump2(134px)로도
         *   건너졌다 — 즉 "dash 관문"이 아니었다. dash 만 여는 폭은 9~10칸뿐이다. */
        check(gap >  R.jump2D, "02: jump2 로도 못 건넌다 (진짜 dash 관문)");
        check(gap <= R.dashD,  "02: dash 로 건너진다");
    }
    memset(&w, 0, sizeof(w));
    sim_load(&w, &t, 2);
    {
        float gap = (float)widest_gap(&w, 15, 17, 14, LEVELS[2].width - 2) * TILE;
        /* 통로 천장이 실제로 2칸인지 */
        int ceilBottom = -1;
        for (int r = MAP_H - 1; r >= 0; r--) if (w.map[r][20] == T_SOLID && r < 14) { ceilBottom = r; break; }
        int head = surface_row(&w, 20) - ceilBottom - 1;
        printf("  03 통로 구덩이 %.0fpx / 머리 위 여유 %d칸\n", gap, head);
        check(head == 2, "03: 통로 높이가 정확히 2칸");
        check(gap >  R.lowCeilD, "03: 저천장에서는 점프로 못 건넌다");
        check(gap <= R.dashD,    "03: dash 로 건너진다");
        check(gap >  R.jump2D * 0.0f + R.lowCeilD, "03: jump2 를 얻어도 여기는 안 통한다");

        /* ★ 교대 가시는 **저천장 아래**에 있어야 성립한다.
         *   개방 지형이면 그냥 뛰어넘어서 타이밍이 사라진다. */
        int wide = 0, bad = 0;
        for (int c = 1; c < LEVELS[2].width - 1; c++)
            for (int r = 1; r < MAP_H; r++) {
                if (w.map[r][c] != T_SPIKE_A && w.map[r][c] != T_SPIKE_B) continue;
                wide++;
                int ceil = -1;
                for (int rr = r - 1; rr >= 0; rr--) if (w.map[rr][c] == T_SOLID) { ceil = rr; break; }
                if (ceil < 0 || (r - ceil - 1) > 2) bad++;
            }
        printf("  03 교대 가시 %d칸 / 저천장 밖 %d칸\n", wide, bad);
        check(wide > 0, "03: 교대 가시가 있다");
        check(bad == 0, "03: 교대 가시가 전부 저천장 아래에 있다 (뛰어넘을 수 없다)");
    }

    /* ── 04 · 05: 활공이 필요한 하강인가 ── */
    printf("\n== 04 SONG.MOD / 05 PHOTO.PCX ==\n");
    for (int idx = 3; idx <= 4; idx++) {
        memset(&w, 0, sizeof(w));
        sim_load(&w, &t, idx);
        const Level *L = sim_level(&w);
        float gap = (float)widest_gap(&w, 1, 17, 1, L->width - 2) * TILE;
        /* 이 레벨의 가장 큰 하강폭 — 표면이 있는 열들의 최대 낙차 */
        int prev = -1, drop = 0;
        for (int c = 1; c < L->width - 1; c++) {
            int s = surface_row(&w, c);
            if (s < 0) continue;
            if (prev >= 0 && s - prev > drop) drop = s - prev;
            prev = s;
        }
        /* ★ 막을 것은 dash 가 아니라 **boot+dash** 다. 그 조합이 11칸이라 항상 들 수 있고,
         *   낙차가 클수록 대시의 이득이 커진다 (중력 정지 + 낙하속도 초기화). */
        float D = (float)(drop * TILE);
        float dashReach  = reach_drop_dash(&t, D, 1);
        float glideReach = reach_drop_glide(&t, D, 0);
        printf("  %s 최대 구덩이 %.0fpx / 최대 낙차 %d칸 / boot+dash %.0fpx / 활공 %.0fpx\n",
               L->id, gap, drop, dashReach, glideReach);
        snprintf(label, sizeof(label), "%s: boot+dash 로는 못 건넌다 (활공 구간)", L->id);
        check(gap > dashReach, label);
        snprintf(label, sizeof(label), "%s: 활공이면 건너진다", L->id);
        check(gap <= glideReach, label);
    }

    /* ── 07 MAZE.DAT: 단독으로 전 구간을 여는 능력이 없어야 한다 ──
     * 관문마다 답을 여러 개 두는 방식으로는 안 된다 — jump2 는 높이도 거리도 1위라
     * 항상 상위호환이 된다. 길 자체를 갈라야 하고, 저천장 통로가 유일한 차단 수단이다. */
    if (LEVEL_COUNT > 6) {
        printf("\n== 07 MAZE.DAT 불변식 ==\n");
        memset(&w, 0, sizeof(w));
        sim_load(&w, &t, 6);
        int floorR = surface_row(&w, 2);
        float s1step = (float)(floorR - surface_row(&w, 13)) * TILE;   /* 첫 디딤 발판 */
        float s1wall = (float)(floorR - surface_row(&w, 20)) * TILE;   /* 장벽 꼭대기 */
        float s2gap  = (float)widest_gap(&w, 15, 17, 32, 75) * TILE;
        float s3gap  = (float)widest_gap(&w, 15, 17, 76, 114) * TILE;
        float s3top  = (float)(floorR - surface_row(&w, 80)) * TILE;   /* 위층 */
        int blockBottom = 0;
        for (int r = MAP_H - 1; r >= 0; r--) if (w.map[r][76] == T_SOLID && r < 14) { blockBottom = r; break; }
        float faceReach = ((float)floorR * TILE - PLAYER_H) - (float)(blockBottom + 1) * TILE;

        printf("  발판 %.1f칸 / 장벽 %.1f칸 / 구덩이 %.1f칸 / 통로 %.1f칸 / 위층 %.1f칸 / 벽면까지 %.0fpx\n",
               s1step / TILE, s1wall / TILE, s2gap / TILE, s3gap / TILE, s3top / TILE, faceReach);
        check(s1step > R.baseH,     "구간1: 맨몸 불가");
        check(s1wall > R.jump2H,    "구간1: 장벽은 점프로 못 넘는다 (발판을 밟거나 벽을 타야 한다)");
        check(s2gap  > R.baseD,     "구간2: 맨몸 불가");
        /* ★ 2026-08-16 추가. 예전 구간2 는 6칸이라 jump2(8.4칸)로 그냥 건너졌다 —
         *   "구간마다 다른 능력"이라는 이 스테이지의 전제가 거기서 깨져 있었다. */
        check(s2gap  > R.jump2D,    "구간2: jump2 로도 못 건넌다 (dash 전용)");
        check(s2gap <= R.dashD,     "구간2: dash 로는 건너진다");
        check(s3top  > R.jump2H,    "구간3: 위층은 점프로 못 오른다");
        check(faceReach <= R.baseH, "구간3: 맨몸 점프로 벽면에 손이 닿는다 (grip 루트 성립)");
        check(s3gap  > R.lowCeilD,  "구간3: 저천장 통로는 점프로 못 건넌다");
        check(s3gap <= R.dashD,     "구간3: 통로는 dash 로 건너진다");

        int masterKey = 0;
        printf("\n  단독 통과 판정\n");
        for (int a = 0; a < AB_COUNT; a++) {
            float hgt = R.baseH, dst = R.baseD;
            int wall = 0, glides = 0;
            if (a == AB_JUMP2) { hgt = R.jump2H; dst = R.jump2D; }
            if (a == AB_BOOT)  { hgt = R.bootH;  dst = R.bootD;  }
            if (a == AB_DASH)  { dst = R.dashD; }
            if (a == AB_GRIP)  { wall = 1; }
            if (a == AB_GLIDE) { glides = 1; }
            int s1 = (hgt >= s1step) || wall;
            int s2 = (dst >= s2gap);
            float corr = (a == AB_DASH) ? R.dashD : (glides ? R.lowCeilD * 1.6f : R.lowCeilD);
            int s3 = (corr >= s3gap) || wall;
            printf("    [%d] %-10s  1:%s 2:%s 3:%s%s\n", a + 1, ABILITY_FILE[a],
                   s1 ? "O" : "X", s2 ? "O" : "X", s3 ? "O" : "X",
                   (s1 && s2 && s3) ? "   <- 마스터키" : "");
            if (s1 && s2 && s3) masterKey = 1;
        }
        check(!masterKey, "단독으로 전 구간을 여는 능력이 없다");
    }

    /* ── 튜토리얼은 능력당 한 번 ────────────────────────────────────
     *
     * ★ 예전엔 관문마다 답이 벽에 붙어 있었다. 그러면 플레이어가 전략을 짜는 게 아니라
     *   지시를 수행한다 — "무엇을 들고 갈까"가 전부인 게임에서 고를 것이 없어진다.
     *   teaches 가 붙은 힌트는 그 능력을 막 얻은 스테이지에서만 뜬다. */
    printf("\n== 튜토리얼 노출 ==\n");
    {
        int taughtIn[AB_COUNT];
        int shown = 0, hidden = 0;
        for (int a = 0; a < AB_COUNT; a++) taughtIn[a] = 0;

        for (int i = 0; i < LEVEL_COUNT; i++) {
            const Level *L = &LEVELS[i];
            for (int h = 0; h < L->hintCount; h++) {
                unsigned tc = L->hints[h].teaches;
                if (!tc) continue;
                int bits = 0, which = -1;
                for (int a = 0; a < AB_COUNT; a++) if (tc & AB_BIT(a)) { bits++; which = a; }
                snprintf(label, sizeof(label), "%s: 튜토리얼 힌트는 능력 하나만 가리킨다", L->id);
                check(bits == 1, label);
                if (which < 0) continue;
                if (hint_school_level(AB_BIT(which)) == i) { taughtIn[which]++; shown++; }
                else hidden++;
            }
        }
        printf("  능력 힌트 %d개 중 교실에서 %d개만 뜬다 (%d개는 숨는다)\n",
               shown + hidden, shown, hidden);
        for (int a = 0; a < AB_COUNT; a++) {
            int sc = hint_school_level(AB_BIT(a));
            printf("    %-10s 교실 %s · 힌트 %d개\n", ABILITY_FILE[a],
                   (sc >= 0) ? LEVELS[sc].id : "-", taughtIn[a]);
            snprintf(label, sizeof(label), "%s 는 해금 직후에 정확히 한 번 설명된다", ABILITY_FILE[a]);
            check(taughtIn[a] == 1, label);
        }
    }

    /* ── 09 · 20: jump2 관문이 grip 으로 안 뚫리는가 ──────────────────
     *
     * ★ 벽 모양으로는 grip 을 막을 수 없다. 잡기 판정이 "머리가 벽면에 닿으면"이라
     *   맨몸 점프로 닿는 52px 위까지는 어떤 형태로 파도 잡힌다.
     *   ㄱ자로 처마를 내면 그 처마의 옆면이 또 벽이 된다.
     *
     *   **원웨이 발판만이 T_SOLID 가 아니라 grip 이 안 붙는다.** 그래서 높이 관문은
     *   턱이 아니라 "구덩이 위에 뜬 원웨이 선반"이어야 한다. 이 절이 그걸 지킨다. */
    printf("\n== jump2 관문 (원웨이 선반) ==\n");
    {
        const int gate[] = { 8 };       /* s09.
                                         * ★ 예전엔 s20 도 여기 있었다. 그런데 그러면
                                         *   s20 의 needs 가 boot+dash+jump2 = 정확히 12칸이 되어
                                         *   로드아웃 선택지가 0 이 됐다. 클라이맥스에서
                                         *   코어 메커닉이 꺼지는 셈이라 s20 의 관문을
                                         *   9·10칸 구덩이로 바꿨다 (아래 s20 절에서 따로 검사한다). */
        float flatDash = reach_drop_dash(&t, 0.0f, 1);   /* boot+dash 로 닿는 최대 가로 */
        for (int k = 0; k < (int)(sizeof(gate) / sizeof(gate[0])); k++) {
            memset(&w, 0, sizeof(w));
            sim_load(&w, &t, gate[k]);
            const Level *L = sim_level(&w);

            int sr = -1, sc = -1;
            for (int c = 1; c < L->width - 1 && sr < 0; c++)
                for (int r = 1; r < MAP_H; r++)
                    if (w.map[r][c] == T_ONEWAY) { sr = r; sc = c; break; }

            snprintf(label, sizeof(label), "%s: 높이 관문이 원웨이 선반이다 (턱이 아니다)", L->id);
            check(sr >= 0, label);
            if (sr < 0) continue;

            /* 선반 왼쪽에서 발판이 끊기는 지점 = 뛰어오르는 자리 */
            int edge = sc;
            while (edge > 1 && surface_row(&w, edge) < 0) edge--;
            int floorR = surface_row(&w, edge);
            float rise = (float)(floorR - sr) * TILE;

            /* 그 발판 끝에서 다음 **솔리드** 벽까지 — 여기 닿으면 grip 으로 기어오른다 */
            int nextSolid = L->width - 1;
            for (int c = edge + 1; c < L->width - 1; c++) {
                int solid = 0;
                for (int r = 1; r < MAP_H; r++) if (w.map[r][c] == T_SOLID) solid = 1;
                if (solid) { nextSolid = c; break; }
            }
            float wallGap = (float)(nextSolid - edge - 1) * TILE;

            printf("  %s 선반 높이 %.0fpx / 건너편 벽까지 %.0fpx  (boot %.0f · jump2 %.0f · boot+jump2 %.0f · boot+dash %.0f)\n",
                   L->id, rise, wallGap, R.bootH, R.jump2H, R.bootJump2H, flatDash);

            snprintf(label, sizeof(label), "%s: boot 단독으로는 선반에 못 오른다", L->id);
            check(rise > R.bootH, label);
            snprintf(label, sizeof(label), "%s: jump2 단독으로도 못 오른다 (boot 를 같이 들어야 한다)", L->id);
            check(rise > R.jump2H, label);
            snprintf(label, sizeof(label), "%s: boot+jump2 면 오른다", L->id);
            check(rise <= R.bootJump2H, label);
            snprintf(label, sizeof(label), "%s: 건너편 벽이 멀어서 grip 으로 못 돌아간다", L->id);
            check(wallGap > flatDash, label);
        }
    }

    /* ── 20 ME.IMG: 디스크를 담은 뒤의 길 ──────────────────────────────
     *
     * ★ 이 스테이지만 목표가 용량을 먹는다 (9칸). 담으면 3칸 = boot.exe 하나만 남는다.
     *   그러면 **디스크 지점부터 출구까지가 맨몸+boot 로 통과 가능해야 한다.**
     *   아니면 담는 순간 게임이 끝난다 — 되돌아갈 수도 없다(파동이 오른쪽에서 온다).
     *
     *   예전엔 이 검사가 없었다. `D-36` 이 "성립 조건: 돌아오는 경로가 남는 칸의
     *   능력만으로 통과 가능해야 한다. test.bat 에 검사 추가 필요"라고 적어둔 그것이다. */
    printf("\n== 20 ME.IMG: 디스크를 담은 뒤 ==\n");
    {
        memset(&w, 0, sizeof(w));
        sim_load(&w, &t, LEVEL_COUNT - 1);
        const Level *L = sim_level(&w);
        int left = CAP_SLOTS - L->diskCost;

        int gap = widest_gap(&w, 1, 17, L->diskCol, L->width - 2);
        /* 디스크 이후 구간의 최대 오르막 */
        int prev = -1, rise = 0;
        for (int c = L->diskCol; c < L->width - 1; c++) {
            int s = surface_row(&w, c);
            if (s < 0) continue;
            if (prev >= 0 && prev - s > rise) rise = prev - s;
            prev = s;
        }
        printf("  디스크 %d칸 → 남는 %d칸 (boot=%d) · 이후 최대 구덩이 %d칸 · 최대 턱 %d칸\n",
               L->diskCost, left, ABILITY_COST[AB_BOOT], gap, rise);
        printf("  boot 거리 %.1f칸 / boot 높이 %.1f칸\n", R.bootD / TILE, R.bootH / TILE);

        check(left >= ABILITY_COST[AB_BOOT], "s20: 디스크를 담아도 boot 는 들 수 있다");
        check(left <  ABILITY_COST[AB_BOOT] + ABILITY_COST[AB_DASH],
              "s20: boot 말고는 아무것도 못 든다 (그래야 결말이 성립한다)");
        check((float)gap  * TILE <= R.bootD, "s20: 디스크 이후 구덩이를 boot 로 건널 수 있다");
        check((float)rise * TILE <= R.bootH, "s20: 디스크 이후 턱을 boot 로 오를 수 있다");
    }

    /* ── 이벤트 ────────────────────────────────────────────────────
     *
     * ★ 이 절이 왜 있는가: `evLand` 가 **가만히 서 있어도 매 프레임** 켜졌다.
     *   onGround 를 매 프레임 0 으로 리셋한 뒤 충돌 안에서 "!onGround 면 착지"로
     *   판정했기 때문이다. 소리를 붙이기 전에는 아무도 몰랐다 — 화면에는
     *   아무 표시도 없었으니까. 효과음이 초당 60번 나고 나서야 드러났다.
     *
     *   교훈: **이벤트는 아무 데도 안 보여도 틀릴 수 있다.** 세어봐야 한다. */
    printf("\n== 이벤트 ==\n");
    {
        sim_load(&w, &t, 0);
        Input idle = { 0 };
        const float dt = 1.0f / 60.0f;

        /* 스폰 직후 낙하·착지가 끝나도록 먼저 돌린다 */
        for (int f = 0; f < 60; f++) sim_step(&w, &t, &idle, dt);

        int lands = 0, jumps = 0;
        for (int f = 0; f < 180; f++) {   /* 3초간 가만히 */
            sim_step(&w, &t, &idle, dt);
            if (w.evLand) lands++;
            if (w.evJump) jumps++;
        }
        printf("  가만히 3초: 착지 %d회 · 점프 %d회 (기대 0 / 0)\n", lands, jumps);
        check(lands == 0, "서 있는 동안 착지 이벤트가 안 난다");
        check(jumps == 0, "서 있는 동안 점프 이벤트가 안 난다");
        check(w.p.onGround, "서 있는 동안 접지 상태가 유지된다");

        /* 점프 한 번 → 착지 정확히 1회 */
        Input jump = { 0 };
        jump.jumpPressed = 1; jump.jumpHeld = 1;
        sim_step(&w, &t, &jump, dt);
        check(w.evJump, "점프하면 점프 이벤트가 난다");

        int landsAfter = 0, airborne = 0;
        for (int f = 0; f < 180; f++) {
            sim_step(&w, &t, &idle, dt);
            if (!w.p.onGround) airborne++;
            if (w.evLand) landsAfter++;
        }
        printf("  점프 후 3초: 체공 %d프레임 · 착지 %d회 (기대 >0 / 1)\n", airborne, landsAfter);
        check(airborne > 0,      "점프하면 실제로 뜬다");
        check(landsAfter == 1,   "점프 후 착지 이벤트가 정확히 1회");
    }

    /* ── 점프패드 ──────────────────────────────────────────────────
     *
     * ★ 밟는 것만으로는 안 튄다. **그 위에서 점프해야** 튄다 (2026-08-15 변경).
     *   지나가다 튀면 조작이 아니라 사고다. 그리고 패드로 뜬 것은 점프가 아니라서
     *   일반 점프컷에 걸리면 안 된다 — 손을 떼도 보통 점프보다는 높아야 한다. */
    printf("\n== 점프패드 ==\n");
    {
        const float dt = 1.0f / 60.0f;
        Input idle = { 0 }, hold = { 0 }, press = { 0 };
        hold.jumpHeld = 1;
        press.jumpPressed = 1; press.jumpHeld = 1;

        /* 좌표를 박아두면 레벨을 옮길 때 조용히 깨진다. 맵에서 찾는다. */
        memset(&w, 0, sizeof(w));
        sim_load(&w, &t, 0);
        int pr = -1, pc = -1;
        for (int r = 0; r < MAP_H && pr < 0; r++)
            for (int c = 0; c < MAP_W; c++)
                if (w.map[r][c] == T_PAD) { pr = r; pc = c; break; }
        check(pr >= 0, "s01 에 점프패드가 있다");

        /* 기준선 — 평지에서의 맨몸 점프 */
        float hNormal = 0.0f;
        {
            for (int f = 0; f < 60; f++) sim_step(&w, &t, &idle, dt);
            float y0 = w.p.y, top = w.p.y;
            sim_step(&w, &t, &press, dt);
            for (int f = 0; f < 120; f++) { sim_step(&w, &t, &hold, dt); if (w.p.y < top) top = w.p.y; }
            hNormal = y0 - top;
        }

        if (pr >= 0) {
            float hHold = 0.0f, hTap = 0.0f;
            int bounced = 0;

            /* 1) 그냥 서 있기 — 안 튀어야 한다 */
            w.p.x = (float)(pc * TILE); w.p.y = (float)(pr * TILE - PLAYER_H);
            w.p.vx = w.p.vy = 0.0f;
            for (int f = 0; f < 120; f++) { sim_step(&w, &t, &idle, dt); if (w.evPad) bounced = 1; }
            check(!bounced,      "패드를 밟고 서 있기만 하면 안 튄다");
            check(w.p.onGround,  "패드 위에 그냥 서 있을 수 있다");

            /* 2) 누른 채 점프 */
            w.p.x = (float)(pc * TILE); w.p.y = (float)(pr * TILE - PLAYER_H);
            w.p.vx = w.p.vy = 0.0f;
            sim_step(&w, &t, &idle, dt);
            {
                float y0 = w.p.y, top = w.p.y;
                sim_step(&w, &t, &press, dt);
                check(w.evPad, "패드 위에서 점프하면 패드가 발동한다");
                for (int f = 0; f < 150; f++) { sim_step(&w, &t, &hold, dt); if (w.p.y < top) top = w.p.y; }
                hHold = y0 - top;
            }

            /* 3) 누르고 바로 떼기 */
            w.p.x = (float)(pc * TILE); w.p.y = (float)(pr * TILE - PLAYER_H);
            w.p.vx = w.p.vy = 0.0f;
            sim_step(&w, &t, &idle, dt);
            {
                float y0 = w.p.y, top = w.p.y;
                sim_step(&w, &t, &press, dt);
                for (int f = 0; f < 150; f++) { sim_step(&w, &t, &idle, dt); if (w.p.y < top) top = w.p.y; }
                hTap = y0 - top;
            }

            printf("  맨몸 점프 %.0fpx / 패드(누른 채) %.0fpx / 패드(떼기) %.0fpx\n",
                   hNormal, hHold, hTap);
            check(hHold > hTap,    "패드는 누르고 있으면 더 높이 튄다");
            check(hTap  > hNormal, "패드는 손을 떼도 맨몸 점프보다 높다");
        }
    }

    /* ── 점프패드와 관문의 거리 ────────────────────────────────────
     *
     * ★ 패드는 **세로 기믹이 아니다.** 위 표대로 가로 10.1칸을 준다 — dash(10.2칸)와
     *   사실상 같고 jump2(8.4칸)·boot(6.3칸)보다 멀리 간다. 높이도 9.4칸으로
     *   boot+jump2(6.3칸)를 넘는다. **패드 하나가 마스터키다.**
     *
     *   그래서 "패드는 세로라 가로 관문은 안 흐린다"는 성립하지 않는다.
     *   대신 **거리**로 관리한다: 패드 사거리 안에 맨몸으로 못 건너는 구덩이가 없으면,
     *   그 패드는 어떤 관문도 대신 열어주지 못한다. */
    printf("\n== 점프패드와 관문의 거리 ==\n");
    {
        int padReach = (int)(R.padD / TILE) + 1;    /* 11칸 */
        for (int i = 0; i < LEVEL_COUNT; i++) {
            memset(&w, 0, sizeof(w));
            sim_load(&w, &t, i);
            const Level *L = sim_level(&w);
            for (int r = 0; r < MAP_H; r++)
                for (int c = 0; c < MAP_W; c++) {
                    if (w.map[r][c] != T_PAD) continue;
                    int x0 = c - padReach, x1 = c + padReach;
                    if (x0 < 1) x0 = 1;
                    if (x1 > L->width - 2) x1 = L->width - 2;
                    int gap = widest_gap(&w, 1, 17, x0, x1);

                    /* s10 만 예외다. 마지막 7칸 구덩이는 **패드로 건너라고** 만든 구간이고
                     * (`레벨구성.md` §10 "네가 아니라 레벨이 속도를 준다"), 그래서
                     * s10 의 `needs dash` 는 그 구덩이에 관해서는 강제가 아니다.
                     * 예외를 지우고 싶으면 패드를 옮기는 게 아니라 **구덩이를 넓혀야** 한다. */
                    int intended = (strcmp(L->id, "s10") == 0);

                    printf("  [%s] 패드 x%-2d  사거리 %d칸 안의 최대 구덩이 %d칸 (맨몸 %.1f칸)%s\n",
                           L->id, c, padReach, gap, R.baseD / TILE,
                           (gap * TILE > R.baseD)
                               ? (intended ? "   <- 의도된 패드 건너기 (s10)" : "   <- 관문을 대신 열 수 있다")
                               : "");
                    if (intended) continue;
                    snprintf(label, sizeof(label),
                             "%s: 패드(x%d) 사거리 안에 맨몸으로 못 건너는 구덩이가 없다", L->id, c);
                    check(gap * TILE <= R.baseD, label);
                }
        }
    }

    /* ── 새 지형 기믹이 실제로 동작하는가 (2026-08-16) ──────────────────
     *
     * ★ 위쪽 절들은 전부 **지형의 모양**을 본다. 모양이 맞아도 규칙이 안 돌면
     *   레벨은 통과하고 게임은 망가진다. 여기서는 sim 을 실제로 돌린다.
     *   (`evLand` 가 초당 60번 켜지던 버그를 잡아낸 것도 이런 절이었다) */
    printf("\n== 교대 가시 ==\n");
    {
        const float dt = 1.0f / 60.0f;
        memset(&w, 0, sizeof(w));
        w.unlocked = AB_ALL;
        sim_load(&w, &t, 2);                       /* s03 — 교대 가시의 교실 */

        w.time = 0.0f;
        check(sim_spike_phase(&w, &t) == T_SPIKE_A, "t=0 에는 A 가 켜져 있다");
        w.time = t.spikeCycle * 1.5f;
        check(sim_spike_phase(&w, &t) == T_SPIKE_B, "한 주기 뒤에는 B 가 켜져 있다");
        w.time = t.spikeCycle * 2.5f;
        check(sim_spike_phase(&w, &t) == T_SPIKE_A, "두 주기 뒤에는 다시 A");

        /* A 칸 위에 세워놓고 위상만 바꿔본다 */
        int ar = -1, ac = -1;
        for (int r = 1; r < MAP_H && ar < 0; r++)
            for (int c = 1; c < LEVELS[2].width - 1; c++)
                if (w.map[r][c] == T_SPIKE_A) { ar = r; ac = c; break; }
        check(ar >= 0, "s03 에 A 가시가 있다");
        if (ar >= 0) {
            Input idle = { 0 };
            /* 위상 A — 밟으면 죽는다 */
            w.p.x = (float)(ac * TILE + 2); w.p.y = (float)(ar * TILE);
            w.p.vx = w.p.vy = 0.0f;
            w.formatX = -9999.0f; w.deathCause = DEATH_NONE; w.time = 0.0f;
            sim_step(&w, &t, &idle, dt);
            check(w.deathCause == DEATH_SPIKE, "켜져 있는 쪽에 닿으면 죽는다");

            /* 위상 B — 같은 칸이 그냥 빈 칸이 된다 */
            w.p.x = (float)(ac * TILE + 2); w.p.y = (float)(ar * TILE);
            w.p.vx = w.p.vy = 0.0f;
            w.formatX = -9999.0f; w.deathCause = DEATH_NONE;
            w.time = t.spikeCycle * 1.2f;
            sim_step(&w, &t, &idle, dt);
            check(w.deathCause == DEATH_NONE, "꺼져 있는 쪽은 그냥 지나간다");
        }
    }

    printf("\n== 상승 기류 ==\n");
    {
        const float dt = 1.0f / 60.0f;
        Input hold = { 0 }; hold.jumpHeld = 1;
        int ur = -1, uc = -1;

        memset(&w, 0, sizeof(w));
        w.unlocked = AB_ALL;
        sim_load(&w, &t, 3);                       /* s04 — 기류의 교실 */
        for (int r = 1; r < MAP_H && ur < 0; r++)
            for (int c = 1; c < LEVELS[3].width - 1; c++)
                if (w.map[r][c] == T_UPDRAFT) { ur = r; uc = c; break; }
        check(ur >= 0, "s04 에 기류가 있다");

        if (ur >= 0) {
            float withGlide, without;
            /* 활공을 들고 있으면 떠오른다 */
            w.p.has[AB_GLIDE] = 1;
            w.p.x = (float)(uc * TILE + 2); w.p.y = (float)((ur + 3) * TILE);
            w.p.vx = w.p.vy = 0.0f; w.p.onGround = 0;
            w.formatX = -9999.0f;
            {
                float y0 = w.p.y;
                for (int f = 0; f < 40 && w.deathCause == DEATH_NONE; f++)
                    sim_step(&w, &t, &hold, dt);
                withGlide = y0 - w.p.y;
            }
            /* 없으면 그냥 떨어진다 — 공짜 엘리베이터가 되면 세로 관문이 다 열린다 */
            sim_load(&w, &t, 3);
            w.p.has[AB_GLIDE] = 0;
            w.p.x = (float)(uc * TILE + 2); w.p.y = (float)((ur + 3) * TILE);
            w.p.vx = w.p.vy = 0.0f; w.p.onGround = 0;
            w.formatX = -9999.0f; w.deathCause = DEATH_NONE;
            {
                float y0 = w.p.y;
                for (int f = 0; f < 40 && w.deathCause == DEATH_NONE; f++)
                    sim_step(&w, &t, &hold, dt);
                without = y0 - w.p.y;
            }
            printf("  40프레임 동안 활공 있음 %+.0fpx / 없음 %+.0fpx (양수 = 상승)\n",
                   withGlide, without);
            check(withGlide > 30.0f, "기류: 활공 중이면 떠오른다");
            check(without   < 0.0f,  "기류: 활공이 없으면 그냥 떨어진다");
        }
    }

    printf("\n== 무너지는 발판 ==\n");
    {
        const float dt = 1.0f / 60.0f;
        Input idle = { 0 };
        int cr = -1, cc = -1;

        memset(&w, 0, sizeof(w));
        w.unlocked = AB_ALL;
        sim_load(&w, &t, 6);                       /* s07 — 무너짐의 교실 */
        for (int r = 1; r < MAP_H && cr < 0; r++)
            for (int c = 1; c < LEVELS[6].width - 1; c++)
                if (w.map[r][c] == T_CRUMBLE) { cr = r; cc = c; break; }
        check(cr >= 0, "s07 에 무너지는 발판이 있다");

        if (cr >= 0) {
            w.p.x = (float)(cc * TILE + 2);
            w.p.y = (float)(cr * TILE - PLAYER_H);
            w.p.vx = w.p.vy = 0.0f;
            w.formatX = -9999.0f;

            sim_step(&w, &t, &idle, dt);
            check(w.p.onGround,          "무너지기 전에는 밟고 설 수 있다");
            check(w.evCrumble,           "밟은 **그 순간** 이벤트가 난다 (소리가 인과를 만든다)");
            check(w.map[cr][cc] == T_CRUMBLE, "밟자마자 사라지지는 않는다");

            int frames = (int)((t.crumbleDelay + 0.2f) / dt);
            for (int f = 0; f < frames; f++) sim_step(&w, &t, &idle, dt);
            check(w.map[cr][cc] == T_EMPTY, "crumbleDelay 뒤에 사라진다");

            /* ★ 되살아날 때 **원래대로 돌려놓아야 한다.** 안 그러면 두 번째 시도부터는
             *   발판이 없는 레벨이 된다 — 시작하자마자 통과 불가다. */
            w.p.y = (float)(MAP_H * TILE + 100);   /* 낙사시킨다 */
            sim_step(&w, &t, &idle, dt);
            check(w.deathCause == DEATH_FALL, "떨어지면 죽는다");
            Input any = { 0 }; any.anyKey = 1;
            for (int f = 0; f < (int)(t.deathFreeze / dt) + 4; f++) sim_step(&w, &t, &any, dt);
            check(w.deathCause == DEATH_NONE,  "되살아난다");
            check(w.map[cr][cc] == T_CRUMBLE,  "되살아나면 무너진 발판이 원래대로 돌아온다");
            check(w.crumbleCount == 0,         "무너짐 목록도 비워진다");
        }
    }

    printf("\n== 파동 규칙 (쉼 과금 · 사망 페널티) ==\n");
    {
        const float dt = 1.0f / 60.0f;
        Input idle = { 0 };

        memset(&w, 0, sizeof(w));
        w.unlocked = AB_ALL;
        sim_load(&w, &t, 9);                       /* s10 — 터미널이 둘뿐인 속도 스테이지 */
        const Level *L = sim_level(&w);

        /* 쉼 과금 — 터미널에 **새로 올라설 때마다** 는다.
         * ★ 스폰 지점은 터미널 **옆**이다 (스폰 x2, 터미널 x3). 걸어 올라가야 한다. */
        w.p.x = (float)(L->terms[0].x * TILE + 2);
        w.p.y = (float)(L->terms[0].y * TILE);
        for (int f = 0; f < 30; f++) sim_step(&w, &t, &idle, dt);
        printf("  터미널 위 정지 횟수 %d\n", w.formatStops);
        check(w.atTerminal,        "터미널 위에 서 있다");
        check(w.formatStops == 1,  "터미널에 올라서면 정지 1회로 센다");

        /* 떠났다가 다시 올라오면 또 센다 — 그게 과금의 기준이다 */
        w.p.x = (float)((L->terms[0].x + 4) * TILE);
        for (int f = 0; f < 10; f++) sim_step(&w, &t, &idle, dt);
        w.p.x = (float)(L->terms[0].x * TILE + 2);
        for (int f = 0; f < 10; f++) sim_step(&w, &t, &idle, dt);
        check(w.formatStops == 2, "떠났다가 돌아오면 한 번 더 센다");

        float v1 = sim_format_speed(&w, &t, 1);
        float v4 = sim_format_speed(&w, &t, 4);
        printf("  파동 속도  1회 정지 %.1f px/s → 4회 정지 %.1f px/s\n", v1, v4);
        check(v1 == L->formatSpeed * t.formatScale, "첫 정지는 공짜다 (스폰이 터미널 위라 억울하다)");
        check(v4 > v1, "쉬면 쉴수록 파동이 빨라진다");

        /* 터미널은 멈추되 최소 간격을 보장한다 */
        w.formatX = w.p.x - 5.0f;                  /* 파동이 바짝 붙은 상태 */
        sim_step(&w, &t, &idle, dt);
        printf("  붙은 파동이 터미널에서 %.0fpx 뒤로 물러남 (보장 %.0f)\n",
               w.p.x - w.formatX, t.formatHold);
        check(w.p.x - w.formatX >= t.formatHold - (float)TILE,
              "터미널은 최소 간격을 보장한다 (뒤이은 구간이 불가능해지지 않게)");
        check(t.formatHold < VIEW_W / 2, "그 간격은 화면 안에 남는다 (얼마나 가까운지 계속 보여야 한다)");

        /* ── 사망 페널티 ──
         * ★ 예전엔 리스폰이 파동을 formatLead 만큼 **밀어줬다.** 그래서 파동이 붙으면
         *   일부러 죽는 게 최적 플레이였다. 이제 체크포인트 당시까지만 되돌린다. */
        float atCp = w.formatAtCheckpoint;
        w.p.y = (float)(MAP_H * TILE + 100);
        sim_step(&w, &t, &idle, dt);
        check(w.deathCause == DEATH_FALL, "떨어지면 죽는다");
        Input any = { 0 }; any.anyKey = 1;
        for (int f = 0; f < (int)(t.deathFreeze / dt) + 4; f++) sim_step(&w, &t, &any, dt);
        printf("  체크포인트 당시 %.0f → 되살아난 뒤 %.0f (유예 %.2fs)\n",
               atCp, w.formatX, w.formatGrace);
        check(w.formatX <= atCp + 1.0f, "죽어도 파동이 체크포인트보다 더 물러나지는 않는다");
        check(w.formatGrace > 0.0f,     "대신 되살아난 직후에는 잠깐 멈춰 있다 (무한 사망 방지)");
    }

    printf("\n== 보상 스테이지는 디스크가 있어야 나갈 수 있다 ==\n");
    {
        const float dt = 1.0f / 60.0f;
        Input idle = { 0 };
        for (int i = 0; i < LEVEL_COUNT; i++) {
            if (LEVELS[i].reward == 0) continue;
            memset(&w, 0, sizeof(w));
            w.unlocked = AB_ALL;
            sim_load(&w, &t, i);
            const Level *L = sim_level(&w);

            w.formatX = -9999.0f;
            w.p.x = (float)(L->exitCol * TILE + 2);
            w.p.y = (float)(L->exitRow * TILE);
            w.p.vx = w.p.vy = 0.0f;

            w.hasDisk = 0;
            sim_step(&w, &t, &idle, dt);
            snprintf(label, sizeof(label), "%s: 디스크 없이 출구에 닿아도 안 열린다", L->id);
            check(w.state == ST_PLAY && sim_exit_locked(&w), label);

            w.hasDisk = 1;
            sim_step(&w, &t, &idle, dt);
            snprintf(label, sizeof(label), "%s: 디스크를 챙기면 열린다", L->id);
            check(w.state == ST_WON, label);
        }
        /* 보상이 없는 스테이지는 잠기면 안 된다 — 디스크는 순수 선택이어야 한다 */
        memset(&w, 0, sizeof(w));
        w.unlocked = AB_ALL;
        sim_load(&w, &t, 14);                      /* s15 — 굴뚝 위 선택 회수물 */
        check(!sim_exit_locked(&w), "s15: 선택 회수물은 출구를 안 잠근다");
    }

    /* ── 앞 스테이지로 되돌아갔을 때 ────────────────────────────────
     *
     * ★ 15까지 깨고 01 로 돌아가면 dash 는 **아직 주운 적이 없는 파일**이다.
     *   전역 해금만 보고 목록에 띄우면 튜토리얼이 무너진다 (2026-08-15 변경). */
    printf("\n== 되돌아간 스테이지의 목록 ==\n");
    {
        memset(&w, 0, sizeof(w));
        sim_load(&w, &t, 0);
        w.unlocked = AB_ALL;              /* 끝까지 깬 세이브 */

        sim_load(&w, &t, 0);              /* 01 로 되돌아간다 */
        int n = 0;
        for (int a = 0; a < AB_COUNT; a++) if (sim_ability_unlocked(&w, a)) n++;
        printf("  끝까지 깬 세이브로 01 에 들어갔을 때 목록 %d개 (기대 1)\n", n);
        check(n == 1,                            "01 에서는 목록이 boot 하나뿐이다");
        check(sim_ability_unlocked(&w, AB_BOOT), "01 에서 boot 는 뜬다");
        check(!sim_ability_unlocked(&w, AB_DASH),"01 에서 dash 는 안 뜬다");

        w.p.has[AB_DASH] = 1;             /* 들고 온 로드아웃도 남으면 안 된다 */
        sim_load(&w, &t, 0);
        check(!w.p.has[AB_DASH], "되돌아가면 나중 유틸리티는 장착이 풀린다");

        sim_load(&w, &t, 5);              /* 06 — dash·glide·grip 은 이미 나온 뒤 */
        n = 0;
        for (int a = 0; a < AB_COUNT; a++) if (sim_ability_unlocked(&w, a)) n++;
        printf("  같은 세이브로 06 에 들어갔을 때 목록 %d개 (기대 4: boot dash glide grip)\n", n);
        check(sim_ability_unlocked(&w, AB_DASH),  "06 에서는 dash 가 뜬다");
        check(sim_ability_unlocked(&w, AB_GRIP),  "06 에서는 grip 이 뜬다");
        check(!sim_ability_unlocked(&w, AB_JUMP2),"06 에서 jump2 는 아직 안 뜬다");

        /* 해금 자체는 안 깎인다 — 세이브가 줄어들면 진행이 사라진다 */
        check(w.unlocked == AB_ALL, "되돌아가도 영구 해금은 그대로다");
    }

    printf("\n%s  (%d 실패)\n\n", failures ? "== 실패 ==" : "== 전부 통과 ==", failures);
    return failures ? 1 : 0;
}
