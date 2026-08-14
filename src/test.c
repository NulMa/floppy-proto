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

    /* ── 레벨 데이터가 예산을 얼마나 먹는가 ──
     * "레벨을 더 넣어도 되나"에 답하려면 추정이 아니라 실제 구조체 크기를 봐야 한다. */
    printf("\n== 레벨 데이터 용량 ==\n");
    printf("  Box %d B / TermSpec %d B / MsgSpec %d B / Level %d B\n",
           (int)sizeof(Box), (int)sizeof(TermSpec), (int)sizeof(MsgSpec), (int)sizeof(Level));
    {
        long total = 0;
        for (int i = 0; i < LEVEL_COUNT; i++) {
            const Level *L = &LEVELS[i];
            long b = (long)sizeof(Level)
                   + (long)L->boxCount * sizeof(Box)
                   + (long)L->termCount * sizeof(TermSpec)
                   + (long)L->msgCount * sizeof(MsgSpec);
            /* 문자열은 대략치 — 실제로는 .rdata 에 들어간다 */
            b += 160;
            printf("  [%s] 약 %ld B  (box %d / term %d / msg %d)\n",
                   L->id, b, L->boxCount, L->termCount, L->msgCount);
            total += b;
        }
        printf("  현재 %d개 합계 약 %ld B = 예산의 %.3f%%\n",
               LEVEL_COUNT, total, 100.0 * total / TOTAL_BYTES);
        printf("  → 이 페이스면 레벨 100개를 넣어도 %.1f KB. 레벨은 용량 제약을 받지 않는다.\n",
               (total / (double)LEVEL_COUNT) * 100.0 / 1024.0);
    }

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

    /* ── 해금 곡선 ──
     * 각 스테이지에 들어갈 때 손에 있는 능력만으로 그 스테이지를 깰 수 있어야 한다.
     * 보상으로 능력을 주는 구조에서는 이게 깨지면 진행이 막힌다. */
    printf("\n== 해금 곡선 ==\n");
    {
        unsigned unl = AB_STARTING;
        for (int i = 0; i < LEVEL_COUNT; i++) {
            int cost = 0, n = 0;
            char names[128] = { 0 };
            for (int a = 0; a < AB_COUNT; a++)
                if (unl & AB_BIT(a)) {
                    cost += ABILITY_COST[a]; n++;
                    strcat(names, ABILITY_FILE[a]); strcat(names, " ");
                }
            printf("  [%s] 진입 시 %d종 %2d칸 %-40s %s\n", LEVELS[i].id, n, cost, names,
                   cost > CAP_SLOTS ? "<- 여기서 조인다" : "");
            unl |= LEVELS[i].reward;
        }
        int finalCost = 0;
        for (int a = 0; a < AB_COUNT; a++) if (unl & AB_BIT(a)) finalCost += ABILITY_COST[a];
        printf("  최종 해금 %d칸 / 용량 %d칸\n", finalCost, CAP_SLOTS);
        check((unl & AB_BIT(AB_BOOT)) != 0, "boot 는 처음부터 있다");
        check(finalCost > CAP_SLOTS || LEVEL_COUNT < 4,
              "최종적으로는 전부 못 들고 간다 (레벨이 다 만들어졌다면)");
    }

    /* ── SECTOR 00 (튜토리얼) ── */
    printf("\n== SECTOR 00 튜토리얼 ==\n");
    memset(&w, 0, sizeof(w));
    sim_load(&w, &t, 0);
    {
        const Level *L = sim_level(&w);
        float step  = (float)(surface_row(&w, 2) - surface_row(&w, 10)) * TILE;
        float ledge = (float)(surface_row(&w, 2) - surface_row(&w, 20)) * TILE;
        float gap   = (float)widest_gap(&w, 15, 17, 1, L->width - 2) * TILE;
        printf("  계단 %.0fpx / 구덩이 %.0fpx / 턱 %.0fpx\n", step, gap, ledge);

        /* 튜토리얼에서 쓸 수 있는 건 boot 하나뿐이다 */
        check(step  <= R.baseH, "계단은 맨몸으로 넘어진다");
        check(gap   <= R.baseD, "구덩이는 맨몸으로 건너진다 (점프 학습용)");
        check(ledge >  R.baseH, "턱은 맨몸으로 못 넘는다 (능력의 필요를 가르침)");
        check(ledge <= R.bootH, "턱은 boot 로 넘어진다");

        int nearTerm = 0;
        for (int i = 0; i < L->termCount; i++)
            if (L->terms[i].x >= 15 && L->terms[i].x <= 19) nearTerm = 1;
        check(nearTerm, "턱 바로 앞에 터미널이 있다 (파동을 거슬러 되돌아가지 않게)");
        check(L->reward == AB_BIT(AB_DASH), "s00 클리어 보상은 dash");
    }

    /* ── SECTOR 01 을 boot+dash 만으로 깰 수 있는가 ── */
    printf("\n== SECTOR 01 진입 시점 통과 가능성 ==\n");
    memset(&w, 0, sizeof(w));
    sim_load(&w, &t, 1);
    {
        int floorR = surface_row(&w, 2);
        float s1step = (float)(floorR - surface_row(&w, 13)) * TILE;
        float s2gap  = (float)widest_gap(&w, 15, 17, 32, 61) * TILE;
        float s3gap  = (float)widest_gap(&w, 15, 17, 62, 94) * TILE;
        check(s1step <= R.bootH, "구간1: boot 로 넘어진다");
        check(s2gap  <= R.dashD, "구간2: dash 로 건너진다");
        check(s3gap  <= R.dashD, "구간3: dash 로 건너진다");
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
