/* test.c — 헤드리스 검증. raylib 없이 sim.c + levels.c 만 돌린다.
 *
 * 플랫포머 AI 를 만들지 않는다. 대신 **레벨의 관문 크기를 실제 물리 수치와 대조**한다.
 * tune.txt 를 건드려 레벨이 클리어 불가가 되면 여기서 잡힌다.
 *
 * 이 바이너리는 제출물이 아니라서 1.44MB 예산에 안 들어간다. */
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

/* 능력별 도달 범위 — tune.txt 에서 파생된다 */
typedef struct {
    float baseH, bootH, jump2H;
    float baseD, bootD, dashD, jump2D, lowCeilD;
} Reach;

static Reach reach_of(const Tune *t)
{
    Reach r;
    float g = t->gravity, v = t->maxSpeed;
    r.baseH  = (t->jumpVel * t->jumpVel) / (2.0f * g);
    float bootV = t->jumpVel * t->bootMult;
    r.bootH  = (bootV * bootV) / (2.0f * g);
    float j2V = t->jumpVel * t->jump2Mult;
    r.jump2H = r.baseH + (j2V * j2V) / (2.0f * g);

    float airBase = 2.0f * fabsf(t->jumpVel) / g;
    r.baseD  = v * airBase;
    r.bootD  = v * (2.0f * fabsf(bootV) / g);
    r.dashD  = r.baseD + (t->dashSpeed - v) * t->dashTime;
    r.jump2D = v * (airBase + 2.0f * fabsf(j2V) / g);

    /* 2칸 통로에서는 머리가 천장에 닿아 상승이 잘린다 */
    float head = 2.0f * TILE - PLAYER_H;
    float tUp  = (fabsf(t->jumpVel) - sqrtf(fmaxf(0.0f, t->jumpVel * t->jumpVel - 2.0f * g * head))) / g;
    r.lowCeilD = v * (tUp + sqrtf(2.0f * head / g));
    return r;
}

static int surface_row(const World *w, int c)
{
    for (int r = 1; r < MAP_H; r++)
        if (w->map[r][c] == T_SOLID && w->map[r - 1][c] != T_SOLID) return r;
    return -1;
}

/* x0..x1 에서 rowFrom..rowTo 사이에 발 디딜 곳이 아예 없는 가장 긴 구간.
 * 특정 행 하나만 보면 바닥 높이가 변하는 구간을 잘못 잰다. */
static int widest_gap(const World *w, int rowFrom, int rowTo, int x0, int x1)
{
    int best = 0, run = 0;
    for (int c = x0; c <= x1; c++) {
        int floorHere = 0;
        for (int r = rowFrom; r <= rowTo; r++) if (w->map[r][c] == T_SOLID) floorHere = 1;
        if (!floorHere) { run++; if (run > best) best = run; }
        else run = 0;
    }
    return best;
}

int main(void)
{
    Tune t;
    tune_defaults(&t);
    Reach R = reach_of(&t);

    printf("\n== 능력별 도달 범위 (tune.txt 기준) ==\n");
    printf("  높이  맨몸 %.1f칸  boot %.1f칸  jump2 %.1f칸  grip 제한없음\n",
           R.baseH / TILE, R.bootH / TILE, R.jump2H / TILE);
    printf("  거리  맨몸 %.1f칸  boot %.1f칸  dash %.1f칸  jump2 %.1f칸  저천장 %.1f칸\n",
           R.baseD / TILE, R.bootD / TILE, R.dashD / TILE, R.jump2D / TILE, R.lowCeilD / TILE);

    /* ── 모든 레벨 공통 구조 검사 ── */
    printf("\n== 레벨 구조 (전 %d개) ==\n", LEVEL_COUNT);
    World w;
    for (int i = 0; i < LEVEL_COUNT; i++) {
        memset(&w, 0, sizeof(w));
        sim_load(&w, &t, i);
        const Level *L = sim_level(&w);
        char label[128];

        int goals = 0, terms = 0;
        for (int r = 0; r < MAP_H; r++)
            for (int c = 0; c < MAP_W; c++) {
                if (w.map[r][c] == T_GOAL)     goals++;
                if (w.map[r][c] == T_TERMINAL) terms++;
            }
        printf("  [%s] %-10s %-14s width %2d  terms %d  format %.0fpx/s\n",
               L->id, L->sector, L->file, L->width, terms, L->formatSpeed);

        snprintf(label, sizeof(label), "%s: 목표 1개", L->id);
        check(goals == 1, label);
        snprintf(label, sizeof(label), "%s: 터미널이 1개 이상", L->id);
        check(terms >= 1, label);
        snprintf(label, sizeof(label), "%s: 스폰 아래에 바닥이 있다", L->id);
        check(surface_row(&w, L->spawnCol) == L->spawnRow, label);
        snprintf(label, sizeof(label), "%s: 목표가 레벨 폭 안에 있다", L->id);
        check(L->goalCol < L->width, label);
        snprintf(label, sizeof(label), "%s: 시작 로드아웃 없이도 파동보다 빠르다", L->id);
        check(L->formatSpeed * t.formatScale < t.maxSpeed * 0.5f, label);
    }

    /* ── SECTOR 00 (튜토리얼) — 가르치려는 게 실제로 그 능력이어야 한다 ── */
    printf("\n== SECTOR 00 튜토리얼 ==\n");
    memset(&w, 0, sizeof(w));
    sim_load(&w, &t, 0);
    {
        float step  = (float)(surface_row(&w, 2) - surface_row(&w, 10)) * TILE;  /* 1칸 계단 */
        float ledge = (float)(surface_row(&w, 2) - surface_row(&w, 17)) * TILE;  /* 3칸 턱 */
        float gap   = (float)widest_gap(&w, 15, 17, 1, 50) * TILE;
        printf("  계단 %.0fpx / 턱 %.0fpx / 구덩이 %.0fpx\n", step, ledge, gap);

        check(step  <= R.baseH, "계단은 맨몸으로 넘어진다 (이동·점프 학습용)");
        check(ledge >  R.baseH, "턱은 맨몸으로 못 넘는다 (능력 필요를 가르침)");
        check(ledge <= R.bootH, "턱은 boot 로 넘어진다 (첫 터미널이 주는 것)");
        check(gap   >  R.bootD, "구덩이는 boot 로 못 건넌다 (두 번째 능력 필요)");
        check(gap   <= R.dashD, "구덩이는 dash 로 건너진다 (두 번째 터미널이 주는 것)");

        /* 터미널이 실제로 그 능력을 갖고 있는가 */
        const Level *L = sim_level(&w);
        int t0HasBoot = (L->terms[0].avail == 0) || (L->terms[0].avail & (1u << AB_BOOT));
        int t1HasDash = (L->terms[1].avail == 0) || (L->terms[1].avail & (1u << AB_DASH));
        check(t0HasBoot, "첫 터미널에 boot 가 있다");
        check(t1HasDash, "두 번째 터미널에 dash 가 있다");
        check(L->terms[0].x < 16, "첫 터미널이 턱보다 앞에 있다");
        check(L->terms[1].x < 28, "두 번째 터미널이 구덩이보다 앞에 있다");
    }

    /* ── SECTOR 01 — 마스터키 부재 (이 게임의 핵심 불변식) ── */
    printf("\n== SECTOR 01 불변식 ==\n");
    memset(&w, 0, sizeof(w));
    sim_load(&w, &t, 1);
    {
        int floorR = surface_row(&w, 2);
        float s1step = (float)(floorR - surface_row(&w, 13)) * TILE;
        float s2gap  = (float)widest_gap(&w, 15, 17, 32, 61) * TILE;
        float s3gap  = (float)widest_gap(&w, 15, 17, 62, 94) * TILE;
        float s3top  = (float)(floorR - surface_row(&w, 70)) * TILE;

        int blockBottom = 0;
        for (int r = MAP_H - 1; r >= 0; r--) if (w.map[r][62] == T_SOLID && r < 14) { blockBottom = r; break; }
        float faceReach = ((float)floorR * TILE - PLAYER_H) - (float)(blockBottom + 1) * TILE;

        printf("  구간1 발판 %.1f칸 / 구간2 구덩이 %.1f칸 / 구간3 통로 %.1f칸 · 위층 %.1f칸\n",
               s1step / TILE, s2gap / TILE, s3gap / TILE, s3top / TILE);
        printf("  위층 벽면을 잡으려면 %.0fpx 상승 필요 (맨몸 %.0fpx)\n", faceReach, R.baseH);

        check(s1step > R.baseH,   "구간1: 맨몸 불가");
        check(s2gap  > R.baseD,   "구간2: 맨몸 불가");
        check(s3top  > R.jump2H,  "구간3: 위층은 점프로 못 오른다");
        check(faceReach <= R.baseH, "구간3: 맨몸 점프로 벽면에 손이 닿는다 (grip 루트 성립)");
        check(s3gap  > R.lowCeilD, "구간3: 저천장 통로는 점프로 못 건넌다");

        const char *names[AB_COUNT] = { "jump2", "dash", "glide", "grip", "boot" };
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

            printf("    %-6s  1:%s 2:%s 3:%s%s\n", names[a],
                   s1 ? "O" : "X", s2 ? "O" : "X", s3 ? "O" : "X",
                   (s1 && s2 && s3) ? "   <- 마스터키" : "");
            if (s1 && s2 && s3) masterKey = 1;
        }
        check(!masterKey, "단독으로 전 구간을 여는 능력이 없다");
    }

    int combos = 0;
    for (int m = 1; m < (1 << AB_COUNT); m++) {
        int cost = 0;
        for (int a = 0; a < AB_COUNT; a++) if (m & (1 << a)) cost += ABILITY_COST[a];
        if (cost <= CAP_SLOTS) combos++;
    }
    printf("\n  12칸에 들어가는 로드아웃 %d가지\n", combos);
    check(combos >= 10, "로드아웃 선택지가 충분하다");

    printf("\n%s  (%d 실패)\n\n", failures ? "== 실패 ==" : "== 전부 통과 ==", failures);
    return failures ? 1 : 0;
}
