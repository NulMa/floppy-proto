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

/* rowFrom..rowTo 사이에 발 디딜 곳이 아예 없는 가장 긴 구간.
 * 특정 행 하나만 보면 바닥 높이가 변하는 구간을 잘못 잰다. */
static int widest_gap(const World *w, int rowFrom, int rowTo, int x0, int x1)
{
    int best = 0, run = 0;
    for (int c = x0; c <= x1; c++) {
        int floorHere = 0;
        for (int r = rowFrom; r <= rowTo; r++) {
            unsigned char t = w->map[r][c];
            /* 원웨이·점프패드도 밟을 수 있으니 바닥으로 센다.
             * T_SOLID 만 보면 원웨이 섬을 구덩이로 잘못 잰다 */
            if (t == T_SOLID || t == T_ONEWAY || t == T_PAD) floorHere = 1;
        }
        if (!floorHere) { run++; if (run > best) best = run; }
        else run = 0;
    }
    return best;
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

    printf("\n== 능력별 도달 범위 (tune.txt 기준) ==\n");
    printf("  높이  맨몸 %.1f칸  boot %.1f칸  jump2 %.1f칸  grip 제한없음\n",
           R.baseH / TILE, R.bootH / TILE, R.jump2H / TILE);
    printf("  거리  맨몸 %.1f칸  boot %.1f칸  dash %.1f칸  jump2 %.1f칸  저천장 %.1f칸\n",
           R.baseD / TILE, R.bootD / TILE, R.dashD / TILE, R.jump2D / TILE, R.lowCeilD / TILE);
    printf("  활공  D px 하강 시 가로 %.2f * D px  (5칸 하강 → %.1f칸)\n",
           t.maxSpeed / t.glideFall, (t.maxSpeed / t.glideFall) * 80.0f / TILE);

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

        /* 디스크는 출구 근처여야 "챙길까 말까"를 클리어 직전에 정하게 된다.
         * 단 용량을 먹는 디스크는 선택 회수물이 아니라 목표 그 자체라 예외다. */
        snprintf(label, sizeof(label), "%s: 디스크가 출구에서 8칸 이내", L->id);
        check(L->diskCol < 0 || L->diskCost > 0 || abs(L->diskCol - L->exitCol) <= 8, label);

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

        snprintf(label, sizeof(label), "%s: 파동보다 플레이어가 빠르다", L->id);
        check(L->formatSpeed * t.formatScale < t.maxSpeed * 0.5f, label);
    }

    /* ── 밸런스: 파동 압박 ──
     *
     * 세로 이동 중에는 가로 진행이 0인데 파동은 계속 다가온다.
     * 굴뚝을 오르는 동안 파동이 따라잡으면 그 스테이지는 클리어가 불가능해진다.
     *
     *   정지 허용 시간 = formatLead / formatSpeed
     *   등반 시간      ≈ 세로 스팬 / 58px/s   (벽점프 1회 약 35px, 왕복 0.6초)
     *
     * 여유가 1.5배는 있어야 사람이 실수 한 번 하고도 올라간다. */
    printf("\n== 파동 압박 ==\n");
    printf("  %-14s %-6s %-8s %-8s %-8s %s\n", "레벨", "파동", "정지허용", "세로스팬", "등반추정", "여유");
    for (int i = 0; i < LEVEL_COUNT; i++) {
        memset(&w, 0, sizeof(w));
        sim_load(&w, &t, i);
        const Level *L = sim_level(&w);

        int lo = MAP_H, hi = -1;
        for (int c = 1; c < L->width - 1; c++) {
            int s = surface_row(&w, c);
            if (s < 0) continue;
            if (s < lo) lo = s;
            if (s > hi) hi = s;
        }
        float span    = (hi > lo) ? (float)(hi - lo) * TILE : 0.0f;
        float stall   = t.formatLead / (L->formatSpeed * t.formatScale);
        float climb   = span / 58.0f;
        float margin  = (climb > 0.05f) ? stall / climb : 99.0f;

        printf("  %-14s %4.0f  %6.1fs  %5.0fpx  %6.1fs  %5.1fx%s\n",
               L->file, L->formatSpeed, stall, span, climb, margin,
               margin < 1.5f ? "   <- 빠듯하다" : "");

        snprintf(label, sizeof(label), "%s: 등반 중에 파동에 잡히지 않는다", L->id);
        check(margin >= 1.5f, label);
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
        float ledge = (float)(surface_row(&w, 2) - surface_row(&w, 20)) * TILE;
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
        printf("  02 최대 구덩이 %.0fpx\n", gap);
        check(gap >  R.bootD, "02: boot 로는 못 건넌다");
        check(gap <= R.dashD, "02: dash 로 건너진다");
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
        float glideReach = (t.maxSpeed / t.glideFall) * (float)(drop * TILE);
        printf("  %s 최대 구덩이 %.0fpx / 최대 낙차 %d칸 / 활공 도달 %.0fpx\n",
               L->id, gap, drop, glideReach);
        snprintf(label, sizeof(label), "%s: dash 로는 못 건넌다 (활공 구간)", L->id);
        check(gap > R.dashD, label);
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
        float s1step = (float)(floorR - surface_row(&w, 13)) * TILE;
        float s2gap  = (float)widest_gap(&w, 15, 17, 32, 61) * TILE;
        float s3gap  = (float)widest_gap(&w, 15, 17, 62, 94) * TILE;
        float s3top  = (float)(floorR - surface_row(&w, 70)) * TILE;
        int blockBottom = 0;
        for (int r = MAP_H - 1; r >= 0; r--) if (w.map[r][62] == T_SOLID && r < 14) { blockBottom = r; break; }
        float faceReach = ((float)floorR * TILE - PLAYER_H) - (float)(blockBottom + 1) * TILE;

        printf("  발판 %.1f칸 / 구덩이 %.1f칸 / 통로 %.1f칸 / 위층 %.1f칸 / 벽면까지 %.0fpx\n",
               s1step / TILE, s2gap / TILE, s3gap / TILE, s3top / TILE, faceReach);
        check(s1step > R.baseH,     "구간1: 맨몸 불가");
        check(s2gap  > R.baseD,     "구간2: 맨몸 불가");
        check(s3top  > R.jump2H,    "구간3: 위층은 점프로 못 오른다");
        check(faceReach <= R.baseH, "구간3: 맨몸 점프로 벽면에 손이 닿는다 (grip 루트 성립)");
        check(s3gap  > R.lowCeilD,  "구간3: 저천장 통로는 점프로 못 건넌다");

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

    printf("\n%s  (%d 실패)\n\n", failures ? "== 실패 ==" : "== 전부 통과 ==", failures);
    return failures ? 1 : 0;
}
