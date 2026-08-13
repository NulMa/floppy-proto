/* test.c — 헤드리스 검증. raylib 없이 sim.c 만 돌린다.
 *
 * 플랫포머 AI 대신, **각 구간의 관문 크기를 실제 물리 수치와 대조**한다.
 * 핵심 질문 하나: "단독으로 전 구간을 여는 능력이 있는가?"
 * 있으면 로드아웃 선택이 무의미해지므로, 그건 실패로 친다. */
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

static int surface_row(const World *w, int c)
{
    for (int r = 1; r < MAP_H; r++)
        if (w->map[r][c] == T_SOLID && w->map[r - 1][c] != T_SOLID) return r;
    return -1;
}

/* x0..x1 에서, rowFrom..rowTo 사이에 발 디딜 곳이 아예 없는 가장 긴 구간.
 *
 * 특정 행 하나만 보면 안 된다 — 구간3 처럼 바닥 높이가 달라지는 곳에서
 * 실제로는 4칸 구덩이인데 17칸으로 잘못 재게 된다. */
static int widest_gap(const World *w, int rowFrom, int rowTo, int x0, int x1)
{
    int best = 0, run = 0;
    for (int c = x0; c <= x1; c++) {
        int hasFloor = 0;
        for (int r = rowFrom; r <= rowTo; r++) if (w->map[r][c] == T_SOLID) hasFloor = 1;
        if (!hasFloor) { run++; if (run > best) best = run; }
        else run = 0;
    }
    return best;
}

int main(void)
{
    Tune t;
    tune_defaults(&t);
    World w;
    memset(&w, 0, sizeof(w));
    sim_init(&w);

    /* ── 능력별 도달 범위 ── */
    float g = t.gravity, vmax = t.maxSpeed;
    float baseH  = (t.jumpVel * t.jumpVel) / (2.0f * g);
    float bootV  = t.jumpVel * t.bootMult;
    float bootH  = (bootV * bootV) / (2.0f * g);
    float j2V    = t.jumpVel * t.jump2Mult;
    float jump2H = baseH + (j2V * j2V) / (2.0f * g);

    float airBase = 2.0f * fabsf(t.jumpVel) / g;
    float baseD   = vmax * airBase;
    float bootD   = vmax * (2.0f * fabsf(bootV) / g);
    float jump2D  = vmax * (airBase + 2.0f * fabsf(j2V) / g);
    float dashD   = baseD + (t.dashSpeed - vmax) * t.dashTime;

    /* 낮은 천장 아래: 머리가 천장에 닿아 상승이 잘리므로 체공이 짧다 */
    float head    = 2.0f * TILE - PLAYER_H;                 /* 2칸 통로의 여유 높이 */
    float tUp     = (fabsf(t.jumpVel) - sqrtf(fmaxf(0.0f, t.jumpVel * t.jumpVel - 2.0f * g * head))) / g;
    float lowCeilD= vmax * (tUp + sqrtf(2.0f * head / g));

    printf("\n== 능력별 도달 범위 ==\n");
    printf("  높이  맨몸 %.1f칸  boot %.1f칸  jump2 %.1f칸  grip 제한없음\n",
           baseH / TILE, bootH / TILE, jump2H / TILE);
    printf("  거리  맨몸 %.1f칸  boot %.1f칸  dash %.1f칸  jump2 %.1f칸\n",
           baseD / TILE, bootD / TILE, dashD / TILE, jump2D / TILE);
    printf("  저천장(2칸) 통로에서의 거리: %.1f칸\n", lowCeilD / TILE);

    /* ── 레벨 ── */
    printf("\n== 레벨 ==\n");
    int goals = 0, terms = 0;
    for (int r = 0; r < MAP_H; r++)
        for (int c = 0; c < MAP_W; c++) {
            if (w.map[r][c] == T_GOAL)     goals++;
            if (w.map[r][c] == T_TERMINAL) terms++;
        }
    printf("  목표 %d개 / 터미널 %d개\n", goals, terms);
    check(goals == 1, "목표가 정확히 1개");
    check(terms >= 3, "터미널이 구간마다 있다");

    int floorR = surface_row(&w, 2);
    check(floorR == 15, "스폰 아래에 바닥이 있다");

    /* 구간1: 발판 3칸씩 / 장벽 직등 9칸 */
    float s1step = (float)(floorR - surface_row(&w, 13)) * TILE;
    float s1wall = (float)(floorR - surface_row(&w, 20)) * TILE;
    /* 구간2: 평지 구덩이 */
    float s2gap  = (float)widest_gap(&w, 15, 17, 32, 61) * TILE;
    /* 구간3: 저천장 통로 구덩이 / 위층 높이 */
    float s3gap  = (float)widest_gap(&w, 15, 17, 62, 94) * TILE;
    float s3top  = (float)(floorR - surface_row(&w, 70)) * TILE;

    /* grip 등반이 성립하려면 위층 아랫면이 점프 도달 높이 안에 있어야 한다 */
    int   blockBottom = 0;
    for (int r = MAP_H - 1; r >= 0; r--) if (w.map[r][62] == T_SOLID && r < 14) { blockBottom = r; break; }
    /* 발이 아니라 머리가 벽면 아랫단에 닿으면 잡힌다 */
    float headAtRest = (float)floorR * TILE - PLAYER_H;
    float faceBottom = (float)(blockBottom + 1) * TILE;
    float faceReach  = headAtRest - faceBottom;

    printf("\n== 구간별 관문 ==\n");
    printf("  구간1  발판 %.1f칸 / 장벽 직등 %.1f칸\n", s1step / TILE, s1wall / TILE);
    printf("  구간2  구덩이 %.1f칸\n", s2gap / TILE);
    printf("  구간3  통로 구덩이 %.1f칸 / 위층 %.1f칸\n", s3gap / TILE, s3top / TILE);

    check(s1step > baseH, "구간1: 맨몸으로는 발판에 못 오른다");
    check(s2gap  > baseD, "구간2: 맨몸으로는 구덩이를 못 건넌다");
    check(s3top  > jump2H, "구간3: 위층은 점프로 못 오른다 (grip 전용)");
    printf("  구간3  벽면을 잡으려면 %.0fpx 상승 필요 (맨몸 점프 %.0fpx)\n", faceReach, baseH);
    check(faceReach <= baseH, "구간3: 맨몸 점프로 벽면에 손이 닿는다 (grip 등반 시작 가능)");
    check(s3gap > lowCeilD, "구간3: 저천장 통로는 맨몸/jump2 로 못 건넌다");

    /* ── 마스터키 검사 — 이게 이 테스트의 존재 이유다 ── */
    printf("\n== 단독 통과 판정 ==\n");
    const char *names[AB_COUNT] = { "jump2", "dash", "glide", "grip", "boot" };
    int masterKey = 0;
    for (int a = 0; a < AB_COUNT; a++) {
        float hgt = baseH, dst = baseD;
        int   climbsAnyWall = 0, glides = 0;
        if (a == AB_JUMP2) { hgt = jump2H; dst = jump2D; }
        if (a == AB_BOOT)  { hgt = bootH;  dst = bootD;  }
        if (a == AB_DASH)  { dst = dashD; }
        if (a == AB_GRIP)  { climbsAnyWall = 1; }
        if (a == AB_GLIDE) { glides = 1; }

        int s1 = (hgt >= s1step) || climbsAnyWall;
        int s2 = (dst >= s2gap);
        /* 구간3 은 저천장 통로(거리 제한) 또는 위층(벽등반) 둘 중 하나 */
        float corridorD = (a == AB_DASH) ? dashD : (glides ? lowCeilD * 1.6f : lowCeilD);
        int s3 = (corridorD >= s3gap) || climbsAnyWall;

        printf("  %-6s  구간1 %s  구간2 %s  구간3 %s%s\n", names[a],
               s1 ? "O" : "X", s2 ? "O" : "X", s3 ? "O" : "X",
               (s1 && s2 && s3) ? "   <- 마스터키" : "");
        if (s1 && s2 && s3) masterKey = 1;
    }
    check(!masterKey, "단독으로 전 구간을 여는 능력이 없다");

    /* ── 12칸에 들어가는 조합 ── */
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
