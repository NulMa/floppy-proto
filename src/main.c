/* main.c — 창·입력·렌더링만. 게임 규칙은 전부 sim.c 에 있다.
 *
 * 실행 인자:  --shot [x] [level]   30프레임 뒤 shot.png 를 남기고 종료 */
#include "raylib.h"
#include "sim.h"
#include "progress.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const Color C_BG      = {  10,  13,  24, 255 };
static const Color C_GRID    = {  17,  22,  38, 255 };
static const Color C_SOLID   = {  44,  51,  72, 255 };
static const Color C_SOLID_T = {  74,  86, 118, 255 };
static const Color C_GOAL    = {  92, 226, 224, 255 };
static const Color C_TERM    = { 120, 240, 180, 255 };
static const Color C_BODY    = {  38,  42,  58, 255 };
static const Color C_TEXT    = { 196, 204, 226, 255 };
static const Color C_DIM     = {  86,  96, 124, 255 };
static const Color C_OFF     = {  46,  54,  76, 255 };
static const Color C_FMT     = { 214,  60,  90, 255 };

/* 순서는 sim.h 의 Ability 와 같아야 한다 (= 획득 순서 = 키 번호) */
static const Color AB_COLOR[AB_COUNT] = {
    { 245, 215, 110, 255 },   /* 1 boot  */
    { 255, 150,  80, 255 },   /* 2 dash  */
    { 170, 230, 150, 255 },   /* 3 glide */
    { 230, 150, 230, 255 },   /* 4 grip  */
    { 118, 200, 255, 255 },   /* 5 jump2 */
};
static const Color C_SPIKE = { 224,  92, 108, 255 };
static const Color C_PAD   = { 250, 196,  86, 255 };
static const Color C_EXIT  = { 120, 240, 180, 255 };

typedef enum { SCR_TITLE = 0, SCR_MENU, SCR_PLAY, SCR_CLEAR, SCR_END } Screen;

#define SAVE_PATH "save.dat"

/* 난이도 티어 — 스테이지 인덱스(0부터)로 판정한다.
 *   s01~s05 쉬움 / s06~s15 중간 / s16~s20 어려움  */
static int tier_of(int idx) { return (idx < 5) ? 0 : (idx < 15 ? 1 : 2); }
static const char *TIER_NAME[3] = { "EASY", "MEDIUM", "HARD" };
static const Color TIER_COLOR[3] = {
    { 120, 220, 150, 255 },   /* easy   */
    { 240, 190,  90, 255 },   /* medium */
    { 236, 110, 110, 255 },   /* hard   */
};

/* ── tune.txt ───────────────────────────────────────────────────────── */
static int load_tune(Tune *t, const char *path, char *msg, int n)
{
    FILE *f = fopen(path, "r");
    if (!f) { snprintf(msg, n, "tune.txt not found - using defaults"); return 0; }
    char line[256];
    int applied = 0, bad = 0;
    while (fgets(line, sizeof(line), f)) {
        char *h = strchr(line, '#');
        if (h) *h = '\0';
        char key[64]; float v;
        if (sscanf(line, "%63s %f", key, &v) == 2) { if (tune_set(t, key, v)) applied++; else bad++; }
    }
    fclose(f);
    snprintf(msg, n, "tune.txt reloaded: %d values%s", applied, bad ? " (unknown ignored)" : "");
    return applied;
}

static void commas(long v, char *out, int n)
{
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%ld", v);
    int len = (int)strlen(tmp), o = 0;
    for (int i = 0; i < len && o < n - 1; i++) {
        if (i > 0 && (len - i) % 3 == 0) out[o++] = ',';
        out[o++] = tmp[i];
    }
    out[o] = '\0';
}

static void center(const char *s, int y, int size, Color c)
{
    DrawText(s, VIEW_W / 2 - MeasureText(s, size) / 2, y, size, c);
}

/* ── 힌트 ────────────────────────────────────────────────────────────
 *
 * 월드 좌표에 붙어서 **항상** 떠 있는다. 박스 없이 텍스트만.
 * 화면 밖은 카메라가 잘라내므로 별도 범위 판정이 필요 없다.
 *
 * 배경이 없으니 지형 위에서 글자가 묻힌다 → 1px 그림자로 읽히게 한다. */
static void text_shadow(const char *s, int x, int y, Color c)
{
    DrawText(s, x + 1, y + 1, 10, (Color){ 6, 8, 14, 220 });
    DrawText(s, x, y, 10, c);
}

/* "SPACE" → "[SPACE]" 로 감싼 문자열을 만든다 */
static void bracket_keys(const char *keys, char *out, int n)
{
    int o = 0;
    const char *s = keys;
    while (*s && o < n - 4) {
        const char *e = s;
        while (*e && *e != ' ') e++;
        if (o) out[o++] = ' ';
        out[o++] = '[';
        for (const char *p = s; p < e && o < n - 3; p++) out[o++] = *p;
        out[o++] = ']';
        s = (*e) ? e + 1 : e;
    }
    out[o] = '\0';
}

#define HINT_MAX 24

static void draw_hints(const World *w, int camX)
{
    const Level *L = sim_level(w);
    /* 이미 그린 자리 — 겹치면 위로 밀어낸다. 레벨 작성이 조금 틀려도 읽히게. */
    int px0[HINT_MAX], py0[HINT_MAX], px1[HINT_MAX], py1[HINT_MAX];
    int placed = 0;

    for (int i = 0; i < L->hintCount; i++) {
        const HintSpec *h = &L->hints[i];
        int ax = h->x * TILE + TILE / 2 - camX;
        int ay = h->y * TILE;

        char keys[64] = { 0 };
        if (h->keys) bracket_keys(h->keys, keys, sizeof(keys));

        int wk = keys[0] ? MeasureText(keys, 10) : 0;
        int w1 = h->l1 ? MeasureText(h->l1, 10) : 0;
        int w2 = h->l2 ? MeasureText(h->l2, 10) : 0;
        int bw = wk > w1 ? wk : w1;
        if (w2 > bw) bw = w2;
        int bh = (keys[0] ? 12 : 0) + (h->l1 ? 12 : 0) + (h->l2 ? 11 : 0);

        int bx = ax - bw / 2;
        int by = ay - bh - 4;

        /* 화면 밖이면 그릴 필요 없다 */
        if (bx > VIEW_W + 8 || bx + bw < -8) continue;
        if (bx < 3) bx = 3;
        if (bx + bw > VIEW_W - 3) bx = VIEW_W - 3 - bw;
        if (by < 32) by = 32;

        /* 겹치면 비켜준다. 위로만 밀면 천장에 막힌 것들이 한자리에 쌓인다 —
         * 위/아래를 번갈아 시도하고, 다 막히면 원위치로 둔다. */
        {
            int step = bh + 5, base = by, best = by;
            for (int tryN = 0; tryN < 7; tryN++) {
                int cand = base + ((tryN % 2) ? (tryN / 2 + 1) * step : -(tryN / 2) * step);
                if (tryN == 0) cand = base;
                if (cand < 32 || cand + bh > VIEW_H - 16) continue;
                int hit = 0;
                for (int k = 0; k < placed; k++)
                    /* 가로 여유를 넉넉히 준다. 딱 붙으면 겹치진 않아도 한 줄로 읽힌다. */
                    if (bx < px1[k] + 14 && bx + bw > px0[k] - 14 &&
                        cand < py1[k] + 3 && cand + bh > py0[k] - 3) { hit = 1; break; }
                if (!hit) { best = cand; break; }
            }
            by = best;
            if (by < 32) by = 32;
        }

        int cy = by;
        if (keys[0]) {
            text_shadow(keys, bx + (bw - wk) / 2, cy, (Color){ 150, 200, 255, 255 });
            cy += 12;
        }
        if (h->l1) {
            text_shadow(h->l1, bx + (bw - w1) / 2, cy, (Color){ 232, 238, 255, 255 });
            cy += 12;
        }
        if (h->l2)
            text_shadow(h->l2, bx + (bw - w2) / 2, cy, (Color){ 132, 146, 178, 255 });

        if (placed < HINT_MAX) {
            px0[placed] = bx; py0[placed] = by;
            px1[placed] = bx + bw; py1[placed] = by + bh;
            placed++;
        }
    }
}

/* ── 월드 ───────────────────────────────────────────────────────────── */
static void draw_world(const World *w, float t, int camX)
{
    ClearBackground(C_BG);
    int c0 = camX / TILE, c1 = (camX + VIEW_W) / TILE + 1;
    if (c0 < 0) c0 = 0;
    if (c1 > MAP_W) c1 = MAP_W;

    for (int r = 0; r < MAP_H; r++)
        for (int c = c0; c < c1; c++)
            if (((r + c) & 1) == 0)
                DrawRectangle(c * TILE - camX, r * TILE, TILE, TILE, C_GRID);

    for (int r = 0; r < MAP_H; r++)
        for (int c = c0; c < c1; c++) {
            int px = c * TILE - camX, py = r * TILE;
            switch (w->map[r][c]) {
                case T_SOLID:
                    DrawRectangle(px, py, TILE, TILE, C_SOLID);
                    if (r == 0 || w->map[r - 1][c] != T_SOLID)
                        DrawRectangle(px, py, TILE, 2, C_SOLID_T);
                    break;
                case T_TERMINAL: {
                    float pulse = 0.55f + 0.45f * (float)((int)(t * 4) % 2);
                    DrawRectangle(px - 2, py - TILE, TILE + 4, TILE * 2, Fade(C_TERM, 0.10f));
                    DrawRectangle(px + 1, py - TILE + 2, TILE - 2, TILE + 10, Fade(C_TERM, 0.22f));
                    DrawRectangleLines(px + 1, py - TILE + 2, TILE - 2, TILE + 10, Fade(C_TERM, pulse));
                    for (int i = 0; i < 3; i++)
                        DrawRectangle(px + 4, py - TILE + 6 + i * 4, TILE - 8, 2, Fade(C_TERM, pulse));
                    DrawRectangle(px - 2, py + TILE - 2, TILE + 4, 2, C_TERM);
                } break;
                case T_DISK: {
                    int bob = ((int)(t * 3) % 2);
                    DrawRectangle(px + 2, py + 2 + bob, TILE - 4, TILE - 4, C_GOAL);
                    DrawRectangle(px + 4, py + 3 + bob, TILE - 8, 3, (Color){ 20, 60, 70, 255 });
                    DrawRectangle(px + 4, py + 9 + bob, TILE - 8, 3, (Color){ 226, 226, 214, 255 });
                } break;

                case T_EXIT: {
                    /* 드라이브 슬롯 — 디스크가 들어가는 구멍 */
                    float pulse = 0.5f + 0.5f * (float)((int)(t * 3) % 2);
                    DrawRectangle(px, py + 1, TILE, TILE - 2, (Color){ 26, 34, 44, 255 });
                    DrawRectangleLines(px, py + 1, TILE, TILE - 2, Fade(C_EXIT, pulse));
                    DrawRectangle(px + 2, py + 6, TILE - 4, 4, (Color){ 6, 8, 14, 255 });
                    DrawRectangle(px + 3, py + 7, TILE - 6, 2, Fade(C_EXIT, pulse));
                } break;

                case T_SPIKE: {
                    /* 위를 향한 삼각 톱니 */
                    for (int s = 0; s < 4; s++) {
                        int sx = px + s * 4;
                        for (int k = 0; k < 4; k++)
                            DrawRectangle(sx + k, py + TILE - 1 - (k < 2 ? k * 3 : (3 - k) * 3 + 3),
                                          1, (k < 2 ? k * 3 : (3 - k) * 3 + 3) + 1, C_SPIKE);
                    }
                    DrawRectangle(px, py + TILE - 2, TILE, 2, (Color){ 120, 44, 56, 255 });
                } break;

                case T_PAD: {
                    int bounce = ((int)(t * 6) % 2) * 2;
                    DrawRectangle(px, py + 4, TILE, TILE - 4, (Color){ 56, 44, 26, 255 });
                    DrawRectangle(px, py + 2 - bounce, TILE, 5, C_PAD);
                    DrawRectangle(px + 3, py + 8, 2, 2, Fade(C_PAD, 0.6f));
                    DrawRectangle(px + 11, py + 8, 2, 2, Fade(C_PAD, 0.6f));
                } break;

                case T_ONEWAY:
                    DrawRectangle(px, py, TILE, 4, C_SOLID_T);
                    DrawRectangle(px, py + 4, TILE, 1, Fade(C_SOLID, 0.5f));
                    break;

                default: break;
            }
        }

    const Player *p = &w->p;
    int x = (int)p->x - camX, y = (int)p->y;

    if (p->dashTimer > 0.0f)
        for (int i = 1; i <= 3; i++)
            DrawRectangle(x - p->dashDir * i * 5, y, PLAYER_W, PLAYER_H, Fade(AB_COLOR[AB_DASH], 0.30f / i));
    if (p->has[AB_GLIDE] && !p->onGround && p->vy > 0 && p->vy <= 80.0f) {
        DrawRectangle(x - 3, y + PLAYER_H - 2, PLAYER_W + 6, 2, Fade(AB_COLOR[AB_GLIDE], 0.75f));
        DrawRectangle(x - 5, y + PLAYER_H + 2, PLAYER_W + 10, 1, Fade(AB_COLOR[AB_GLIDE], 0.35f));
    }
    if (p->wallDir != 0)
        DrawRectangle(p->wallDir < 0 ? x - 2 : x + PLAYER_W, y + 2, 2, PLAYER_H - 4, AB_COLOR[AB_GRIP]);

    DrawRectangle(x, y, PLAYER_W, PLAYER_H, C_BODY);
    DrawRectangleLines(x, y, PLAYER_W, PLAYER_H, C_DIM);
    DrawRectangle(x + 2, y + 1, PLAYER_W - 4, 3, (Color){ 132, 140, 162, 255 });

    /* 얼굴 — 말을 안 하는 캐릭터라 상태 표현이 전부 여기로 온다 */
    Color face = (Color){ 120, 220, 240, 255 };
    int scared = ((int)p->x - (int)w->formatX) < 90;
    if (p->dashTimer > 0.0f)   face = AB_COLOR[AB_DASH];
    else if (p->wallDir != 0)  face = AB_COLOR[AB_GRIP];
    else if (w->atTerminal)    face = C_TERM;
    else if (scared)           face = (Color){ 255, 140, 140, 255 };
    else if (!p->onGround)     face = AB_COLOR[AB_JUMP2];

    int ex = (p->facing > 0) ? 1 : -1;
    int shake = (scared && !w->atTerminal && ((int)(t * 20) & 1)) ? 1 : 0;
    if (w->atTerminal) {                                   /* 안도 — 눈을 감는다 */
        DrawRectangle(x + 3 + ex, y + 7, 2, 1, face);
        DrawRectangle(x + 7 + ex, y + 7, 2, 1, face);
        DrawRectangle(x + 4 + ex, y + 10, 4, 1, face);
    } else {
        DrawRectangle(x + 3 + ex + shake, y + 6, 2, 2, face);
        DrawRectangle(x + 7 + ex + shake, y + 6, 2, 2, face);
        DrawRectangle(x + 4 + ex, y + 10, p->onGround ? 4 : 2, 1, face);
    }
    if (p->has[AB_JUMP2] && p->airJumps > 0 && !p->onGround)
        DrawRectangle(x + PLAYER_W / 2 - 1, y - 3, 2, 2, AB_COLOR[AB_JUMP2]);

    /* 포맷 파동 */
    int fx = (int)w->formatX - camX;
    if (fx > 0) {
        if (fx > VIEW_W) fx = VIEW_W;
        DrawRectangle(0, 0, fx, VIEW_H, (Color){ 12, 4, 10, 240 });
        for (int i = 0; i < VIEW_H; i += 3) {
            int jitter = ((i * 37 + (int)(t * 220)) % 11) - 5;
            DrawRectangle(0, i, fx + jitter, 1, Fade(C_FMT, 0.10f));
        }
        DrawRectangle(fx - 2, 0, 3, VIEW_H, C_FMT);
        DrawRectangle(fx + 1, 0, 1, VIEW_H, Fade(C_FMT, 0.35f));
    }
}

/* ── HUD ────────────────────────────────────────────────────────────── */
static void draw_hud(const World *w, const char *toast, float toastTimer)
{
    const Player *p = &w->p;
    const Level *L = sim_level(w);
    int freeSlots = sim_free_slots(w);
    char buf[96], num[32];

    DrawRectangle(0, 0, VIEW_W, 30, (Color){ 8, 10, 18, 215 });
    commas((long)freeSlots * BYTES_PER_SLOT, num, sizeof(num));
    snprintf(buf, sizeof(buf), "%s   %s bytes free", L->sector, num);
    DrawText(buf, 6, 4, 10, freeSlots ? C_TEXT : C_DIM);

    int cell = 0;
    for (int a = 0; a < AB_COUNT; a++)
        if (p->has[a])
            for (int k = 0; k < ABILITY_COST[a]; k++, cell++)
                DrawRectangle(6 + cell * 11, 17, 9, 9, AB_COLOR[a]);
    for (; cell < CAP_SLOTS; cell++)
        DrawRectangle(6 + cell * 11, 17, 9, 9, (Color){ 22, 26, 40, 255 });
    for (int i = 0; i < CAP_SLOTS; i++)
        DrawRectangleLines(6 + i * 11, 17, 9, 9, (Color){ 52, 60, 84, 255 });

    snprintf(buf, sizeof(buf), "%.1fs  RETRIES %d", w->time, w->deaths);
    DrawText(buf, VIEW_W - MeasureText(buf, 10) - 6, 4, 10, C_DIM);

    /* 이 섹터의 디스크를 챙겼는지 — 출구 앞에서 되돌아갈지 판단하는 정보다 */
    if (L->diskCol >= 0) {
        const char *ds = w->hasDisk ? "DISK  [x]" : "DISK  [ ]";
        DrawText(ds, 6 + CAP_SLOTS * 11 + 12, 17, 10, w->hasDisk ? C_GOAL : C_DIM);
    }
    if (w->diskFlash > 0.0f) {
        snprintf(buf, sizeof(buf), "%s  recovered", L->file);
        center(buf, 46, 10, Fade(C_GOAL, w->diskFlash));
    }
    const char *lock = w->atTerminal ? "WRITE PROTECT DISABLED" : "WRITE PROTECTED";
    Color lc = w->atTerminal ? C_TERM : (Color){ 150, 110, 110, 255 };
    DrawText(lock, VIEW_W - MeasureText(lock, 10) - 6, 17, 10, lc);

    /* 로드아웃 패널 — **터미널 위에서만** 그린다.
     *
     * 상시로 띄우면 화면 좌상단을 계속 먹는다. 레벨이 위쪽에서 시작하면
     * 플레이어와 지형이 패널 뒤에 깔린다 (s04 에서 실제로 그랬다).
     * 장착 상태는 상단 용량 칸의 색으로 이미 보이므로 밖에서는 필요 없다. */
    int px = 6, pw = 152;
    if (w->atTerminal) {
        int nUnlocked = 0;
        for (int a = 0; a < AB_COUNT; a++) if (sim_ability_unlocked(w, a)) nUnlocked++;
        int py = 36, ph = nUnlocked * 12 + 16;
        DrawRectangle(px, py, pw, ph, (Color){ 8, 10, 18, 228 });
        DrawRectangleLines(px, py, pw, ph, Fade(C_TERM, 0.7f));
        DrawText("UTILITIES", px + 5, py + 4, 10, C_TERM);
        /* ★ 아직 손에 넣은 적 없는 유틸리티는 목록에 아예 안 뜬다.
         *   존재를 모르는 게 맞다 — 회수한 디스크에서 하나씩 나온다. */
        int row = 0;
        for (int a = 0; a < AB_COUNT; a++) {
            if (!sim_ability_unlocked(w, a)) continue;
            int ly = py + 17 + row * 12;
            row++;
            int on = p->has[a];
            int here = sim_ability_available(w, a);
            Color c = on ? AB_COLOR[a] : (here ? C_DIM : (Color){ 38, 44, 62, 255 });
            DrawRectangle(px + 5, ly + 1, 6, 6, on ? AB_COLOR[a] : (Color){ 24, 28, 42, 255 });
            DrawRectangleLines(px + 5, ly + 1, 6, 6, c);
            snprintf(buf, sizeof(buf), "%d %-10s %d", a + 1, ABILITY_FILE[a], ABILITY_COST[a]);
            DrawText(buf, px + 15, ly, 10, c);
            /* 이 섹터에 없는 유틸리티는 오른쪽에 따로 표기한다.
             * 같은 좌표에 겹쳐 그리면 둘 다 못 읽는다 */
            if (!here && !on)
                DrawText("absent", px + pw - 38, ly, 10, (Color){ 96, 62, 70, 255 });
        }
    }

    /* 조작 안내는 월드에 떠 있는 힌트가 맡는다.
     * 하단 바는 눈에 잘 안 들어와서 가르치는 용도로 못 쓴다 — 메타 키만 남긴다. */
    const char *keys = sim_ability_unlocked(w, AB_DASH)
                     ? "shift dash    R restart    F5 tune"
                     : "R restart    F5 tune";
    DrawRectangle(0, VIEW_H - 13, VIEW_W, 13, (Color){ 8, 10, 18, 200 });
    DrawText(keys, VIEW_W / 2 - MeasureText(keys, 10) / 2, VIEW_H - 11, 10,
             (Color){ 62, 70, 94, 255 });

    if (w->checkFlash > 0.0f)
        center("format halted at this sector", 60, 10, Fade(C_TERM, w->checkFlash));
    if (w->denyFlash > 0.0f) {
        const char *d = w->atTerminal ? "no space / not on this sector" : "terminals only";
        center(d, 74, 10, Fade((Color){ 255, 120, 90, 255 }, w->denyFlash * 2));
    }
    if (toastTimer > 0.0f)
        DrawText(toast, px + pw + 10, 36, 10, Fade(C_GOAL, toastTimer > 1.0f ? 1.0f : toastTimer));
}

/* ── 화면들 ─────────────────────────────────────────────────────────── */
static void draw_title(float t)
{
    ClearBackground(C_BG);
    for (int i = 0; i < VIEW_H; i += 4)
        DrawRectangle(0, i, VIEW_W, 1, (Color){ 14, 18, 32, 255 });

    int bx = VIEW_W / 2 - 118, by = 70;
    DrawRectangleLines(bx, by, 236, 42, C_DIM);
    center("L A S T   W R I T E", by + 15, 20, C_GOAL);
    center("1,474,560 bytes free", by + 58, 10, C_DIM);

    if (((int)(t * 2) % 2) == 0)
        center("[ENTER]  insert disk", by + 96, 10, C_TEXT);
    center("a floppy has one job left", VIEW_H - 40, 10, (Color){ 60, 68, 92, 255 });
}

/* ── 스테이지 선택 ──────────────────────────────────────────────────── */
#define MENU_COLS 5

static void draw_menu(const Progress *pr, int sel, float t)
{
    char buf[64];
    ClearBackground(C_BG);
    for (int i = 0; i < VIEW_H; i += 4)
        DrawRectangle(0, i, VIEW_W, 1, (Color){ 14, 18, 32, 255 });

    center("SELECT SECTOR", 14, 20, C_GOAL);

    int rows = (LEVEL_COUNT + MENU_COLS - 1) / MENU_COLS;
    int cw = 88, ch = 40, gx = 4, gy = 6;
    int gridW = MENU_COLS * cw + (MENU_COLS - 1) * gx;
    int x0 = (VIEW_W - gridW) / 2, y0 = 46;

    for (int i = 0; i < LEVEL_COUNT && i < MAX_STAGES; i++) {
        int r = i / MENU_COLS, c = i % MENU_COLS;
        int x = x0 + c * (cw + gx), y = y0 + r * (ch + gy);
        int open = (i <= pr->reached);
        int done = pr->cleared[i];
        Color tc = TIER_COLOR[tier_of(i)];

        Color border = open ? (done ? tc : Fade(tc, 0.55f)) : (Color){ 34, 40, 58, 255 };
        Color fill   = (i == sel) ? Fade(tc, 0.18f) : (Color){ 13, 17, 28, 255 };
        DrawRectangle(x, y, cw, ch, fill);
        DrawRectangleLines(x, y, cw, ch, border);
        if (i == sel) {
            float pulse = 0.55f + 0.45f * (float)((int)(t * 4) % 2);
            DrawRectangleLines(x - 1, y - 1, cw + 2, ch + 2, Fade(tc, pulse));
        }

        snprintf(buf, sizeof(buf), "%02d", i + 1);
        DrawText(buf, x + 6, y + 5, 20, open ? (done ? tc : C_TEXT) : (Color){ 46, 54, 76, 255 });

        if (!open) {
            DrawText("LOCKED", x + 32, y + 11, 10, (Color){ 46, 54, 76, 255 });
            continue;
        }
        if (done) {
            snprintf(buf, sizeof(buf), "%.1fs", pr->best[i]);
            DrawText(buf, x + 32, y + 8, 10, C_TEXT);
            if (pr->diskGot[i]) {
                DrawRectangle(x + cw - 16, y + 7, 10, 10, C_GOAL);
                DrawRectangle(x + cw - 14, y + 12, 6, 4, (Color){ 226, 226, 214, 255 });
            } else {
                DrawRectangleLines(x + cw - 16, y + 7, 10, 10, (Color){ 70, 84, 110, 255 });
            }
        } else {
            DrawText("NEW", x + 32, y + 8, 10, tc);
        }
        DrawText(LEVELS[i].file, x + 6, y + 27, 10, done ? C_DIM : (Color){ 58, 68, 92, 255 });
    }

    /* 티어 범례 */
    int ly = y0 + rows * (ch + gy) + 6;
    int lx = x0;
    for (int k = 0; k < 3; k++) {
        DrawRectangle(lx, ly + 2, 6, 6, TIER_COLOR[k]);
        DrawText(TIER_NAME[k], lx + 10, ly, 10, C_DIM);
        lx += 12 + MeasureText(TIER_NAME[k], 10) + 14;
    }
    snprintf(buf, sizeof(buf), "%d/%d cleared    %d disks    %d retries",
             progress_total_cleared(pr, LEVEL_COUNT), LEVEL_COUNT,
             progress_total_disks(pr, LEVEL_COUNT), pr->deaths);
    DrawText(buf, VIEW_W - MeasureText(buf, 10) - x0, ly, 10, C_DIM);

    center("[ARROWS] select    [ENTER] load    [ESC] title", VIEW_H - 20, 10, C_DIM);
}

static void draw_clear(const World *w, unsigned gained)
{
    const Level *L = sim_level(w);
    char buf[96], num[32];
    int hadDisk = (L->diskCol >= 0);

    DrawRectangle(0, 0, VIEW_W, VIEW_H, (Color){ 6, 14, 16, 205 });
    center("SECTOR CLEARED", 56, 20, C_EXIT);

    int y = 92;
    if (hadDisk && w->hasDisk) {
        commas(L->fileBytes, num, sizeof(num));
        snprintf(buf, sizeof(buf), "RECOVERED  %s   %s bytes", L->file, num);
        center(buf, y, 10, C_GOAL);
        center(L->meta, y + 14, 10, C_DIM);
        y += 38;
        for (int a = 0; a < AB_COUNT; a++) {
            if (!(gained & AB_BIT(a))) continue;
            center("the disk contained a utility", y, 10, C_DIM);
            snprintf(buf, sizeof(buf), "[%d]  + %s     %d slots", a + 1, ABILITY_FILE[a], ABILITY_COST[a]);
            center(buf, y + 14, 10, AB_COLOR[a]);
            y += 34;
        }
    } else if (hadDisk) {
        /* 출구만 통과했다. 디스크는 선택이지만, 그 안에 유틸리티가 있었다면 알려준다. */
        snprintf(buf, sizeof(buf), "%s  LEFT BEHIND", L->file);
        center(buf, y, 10, (Color){ 220, 130, 110, 255 });
        if (L->reward)
            center("it held a utility you did not get", y + 14, 10, C_DIM);
        center("[R]  go back for it", y + 34, 10, C_TEXT);
        y += 56;
    } else {
        center("no disk on this sector", y, 10, C_DIM);
        y += 24;
    }

    snprintf(buf, sizeof(buf), "%.1f seconds     %d retries", w->time, w->deaths);
    center(buf, y + 4, 10, C_DIM);
    center("[ENTER]  next sector", VIEW_H - 46, 10, C_TEXT);
}

static void draw_end(const World *w, const Progress *pr)
{
    char buf[96], num[32];
    long bytes = 0;
    for (int i = 0; i < LEVEL_COUNT && i < MAX_STAGES; i++)
        if (pr->diskGot[i]) bytes += LEVELS[i].fileBytes;
    float total = 0.0f;
    for (int i = 0; i < LEVEL_COUNT && i < MAX_STAGES; i++) total += pr->best[i];

    ClearBackground(C_BG);
    DrawText("A:\\> _", 40, 56, 20, C_GOAL);
    snprintf(buf, sizeof(buf), "%d of %d files recovered",
             progress_total_disks(pr, LEVEL_COUNT), LEVEL_COUNT);
    DrawText(buf, 56, 106, 10, C_TEXT);
    commas(bytes, num, sizeof(num));
    snprintf(buf, sizeof(buf), "%s bytes written", num);
    DrawText(buf, 56, 122, 10, C_TEXT);
    snprintf(buf, sizeof(buf), "%.1f seconds total     %d retries", total, pr->deaths);
    DrawText(buf, 56, 138, 10, C_DIM);
    DrawText("disk ejected.", 56, 170, 10, C_DIM);
    center("[ENTER]  back to sectors", VIEW_H - 40, 10, C_DIM);
    (void)w;
}

/* ── main ───────────────────────────────────────────────────────────── */
int main(int argc, char **argv)
{
    int shotMode = (argc > 1 && strcmp(argv[1], "--shot") == 0);
    int shotMenu = (argc > 1 && strcmp(argv[1], "--shotmenu") == 0);
    int shotTile = (argc > 2) ? atoi(argv[2]) : -1;
    int shotLvl  = (argc > 3) ? atoi(argv[3]) : 0;

    Tune tune;
    tune_defaults(&tune);
    char toast[128] = { 0 };
    float toastTimer = 0.0f;
    load_tune(&tune, "tune.txt", toast, sizeof(toast));

    Progress prog;
    progress_reset(&prog, AB_STARTING);
    progress_load(&prog, SAVE_PATH);

    World world;
    sim_new_game(&world, &tune);
    world.unlocked = prog.unlocked;
    if (shotMode && shotLvl > 0) {
        /* 스크린샷용: 그 레벨까지의 보상을 미리 지급한다 */
        for (int i = 0; i < shotLvl && i < LEVEL_COUNT; i++) world.unlocked |= LEVELS[i].reward;
        sim_load(&world, &tune, shotLvl);
    }

    Screen screen = shotMode ? SCR_PLAY : (shotMenu ? SCR_MENU : SCR_TITLE);
    int  menuSel = prog.reached;
    unsigned gained = 0;
    int quit = 0;

    if (shotMenu) {
        /* 메뉴 확인용 가짜 진행 상태 — 저장하지 않는다 */
        for (int i = 0; i < LEVEL_COUNT && i < 7; i++) {
            prog.cleared[i] = 1;
            prog.best[i] = 11.0f + i * 3.7f;
            if (i % 3 != 1) prog.diskGot[i] = 1;
        }
        prog.reached = (LEVEL_COUNT > 7) ? 7 : LEVEL_COUNT - 1;
        prog.deaths = 23;
        menuSel = prog.reached;
    }

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(VIEW_W * 2, VIEW_H * 2, "LAST WRITE");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);   /* ESC 는 화면별로 처리한다 (플레이 → 메뉴 → 타이틀 → 종료) */

    int mh = GetMonitorHeight(GetCurrentMonitor());
    int scale = (mh > 0) ? (mh - 140) / VIEW_H : 2;
    if (scale < 2) scale = 2;
    if (scale > 4) scale = 4;
    SetWindowSize(VIEW_W * scale, VIEW_H * scale);
    SetWindowPosition(60, 60);

    RenderTexture2D canvas = LoadRenderTexture(VIEW_W, VIEW_H);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);

    if (shotMode && shotTile >= 0) {
        world.p.x = shotTile * (float)TILE;
        world.formatX = world.p.x - 520.0f;
    }

    int frames = 0;
    while (!WindowShouldClose() && !quit) {
        float dt = 1.0f / 60.0f;
        int enter = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);

        if (IsKeyPressed(KEY_F5)) { load_tune(&tune, "tune.txt", toast, sizeof(toast)); toastTimer = 2.5f; }
        if (toastTimer > 0) toastTimer -= dt;

        int esc = IsKeyPressed(KEY_ESCAPE);

        if (screen == SCR_TITLE) {
            if (esc) quit = 1;
            if (enter) { menuSel = prog.reached; screen = SCR_MENU; }
        } else if (screen == SCR_MENU) {
            if (esc) screen = SCR_TITLE;
            if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) menuSel--;
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) menuSel++;
            if (IsKeyPressed(KEY_UP))   menuSel -= MENU_COLS;
            if (IsKeyPressed(KEY_DOWN)) menuSel += MENU_COLS;
            if (menuSel < 0) menuSel = 0;
            if (menuSel >= LEVEL_COUNT) menuSel = LEVEL_COUNT - 1;

            if (enter && menuSel <= prog.reached) {
                world.unlocked = prog.unlocked;   /* 세이브가 원본이다 */
                sim_load(&world, &tune, menuSel);
                gained = 0;
                screen = SCR_PLAY;
            }
        } else if (screen == SCR_CLEAR) {
            /* 디스크를 놓쳤으면 되돌아갈 수 있게 한다 — 진행이 막히면 안 된다 */
            if (IsKeyPressed(KEY_R)) {
                world.unlocked = prog.unlocked;
                sim_load(&world, &tune, world.levelIndex);
                gained = 0;
                screen = SCR_PLAY;
            } else if (enter || esc) {
                menuSel = world.levelIndex;
                if (menuSel + 1 < LEVEL_COUNT && prog.cleared[menuSel]) menuSel++;
                screen = (progress_total_cleared(&prog, LEVEL_COUNT) >= LEVEL_COUNT)
                       ? SCR_END : SCR_MENU;
            }
        } else if (screen == SCR_END) {
            if (enter || esc) { menuSel = prog.reached; screen = SCR_MENU; }
        } else {
            if (esc) { menuSel = world.levelIndex; screen = SCR_MENU; }
            Input in;
            memset(&in, 0, sizeof(in));
            in.toggleAbility = -1;
            in.left        = IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A);
            in.right       = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
            in.jumpHeld    = IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_Z) || IsKeyDown(KEY_K);
            in.jumpPressed = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_K);
            in.dashPressed = IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_RIGHT_SHIFT)
                          || IsKeyPressed(KEY_X) || IsKeyPressed(KEY_L);
            for (int a = 0; a < AB_COUNT; a++)
                if (IsKeyPressed(KEY_ONE + a)) in.toggleAbility = a;
            if (IsKeyPressed(KEY_R)) sim_load(&world, &tune, world.levelIndex);

            sim_step(&world, &tune, &in, dt);

            if (world.state == ST_WON) {
                gained = sim_grant_reward(&world);   /* 디스크 안에 들어 있던 유틸리티 */
                prog.unlocked = world.unlocked;
                prog.deaths  += world.deaths;
                progress_record(&prog, world.levelIndex, world.hasDisk, world.time, LEVEL_COUNT);
                progress_save(&prog, SAVE_PATH);
                screen = SCR_CLEAR;
            }
        }

        const Level *L = sim_level(&world);
        int maxCam = L->width * TILE - VIEW_W;
        if (maxCam < 0) maxCam = 0;
        int camX = (int)(world.p.x + PLAYER_W / 2.0f) - VIEW_W / 2;
        if (camX < 0) camX = 0;
        if (camX > maxCam) camX = maxCam;

        BeginTextureMode(canvas);
            if (screen == SCR_TITLE)     draw_title((float)GetTime());
            else if (screen == SCR_MENU) draw_menu(&prog, menuSel, (float)GetTime());
            else if (screen == SCR_END)  draw_end(&world, &prog);
            else {
                draw_world(&world, (float)GetTime(), camX);
                draw_hints(&world, camX);
                draw_hud(&world, toast, toastTimer);
                if (screen == SCR_CLEAR) draw_clear(&world, gained);
            }
        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK);
            Rectangle src = { 0, 0, (float)VIEW_W, -(float)VIEW_H };
            Rectangle dst = { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() };
            DrawTexturePro(canvas.texture, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
        EndDrawing();

        if ((shotMode || shotMenu) && ++frames >= 30) { TakeScreenshot("shot.png"); break; }
    }

    UnloadRenderTexture(canvas);
    CloseWindow();
    return 0;
}
