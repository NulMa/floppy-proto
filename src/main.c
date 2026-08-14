/* main.c — 창·입력·렌더링만. 게임 규칙은 전부 sim.c 에 있다.
 *
 * 실행 인자:  --shot [x] [level]   30프레임 뒤 shot.png 를 남기고 종료 */
#include "raylib.h"
#include "sim.h"
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

static const Color AB_COLOR[AB_COUNT] = {
    { 118, 200, 255, 255 }, { 255, 150,  80, 255 }, { 170, 230, 150, 255 },
    { 230, 150, 230, 255 }, { 245, 215, 110, 255 },
};

typedef enum { SCR_TITLE = 0, SCR_PLAY, SCR_CLEAR, SCR_END } Screen;

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
 * 월드 좌표에 붙어서, 가까이 있는 동안 계속 떠 있는다.
 * 화면 중앙에 한 번 띄우고 끝내면 읽을 타이밍을 플레이어가 못 고른다. */

/* 키캡 하나. 다음 키캡의 x 오프셋을 반환한다 */
static int draw_keycap(int x, int y, const char *label, int len, float a)
{
    char buf[16];
    int n = len < 15 ? len : 15;
    memcpy(buf, label, n);
    buf[n] = '\0';
    int tw = MeasureText(buf, 10);
    int bw = tw + 9;
    DrawRectangle(x, y, bw, 14, Fade((Color){ 40, 48, 70, 255 }, a));
    DrawRectangle(x, y + 12, bw, 2, Fade((Color){ 22, 26, 40, 255 }, a));
    DrawRectangleLines(x, y, bw, 14, Fade((Color){ 150, 168, 210, 255 }, a));
    DrawText(buf, x + 5, y + 3, 10, Fade((Color){ 226, 234, 255, 255 }, a));
    return bw + 4;
}

#define HINT_MAX_SHOWN 2

static void draw_hints(const World *w, int camX, float t)
{
    const Level *L = sim_level(w);
    float pcx = w->p.x + PLAYER_W * 0.5f, pcy = w->p.y + PLAYER_H * 0.5f;

    /* 가까운 것 몇 개만 남긴다. 전부 그리면 서로 가려서 하나도 못 읽는다. */
    int   pick[HINT_MAX_SHOWN];
    float pickD[HINT_MAX_SHOWN];
    int   n = 0;

    for (int i = 0; i < L->hintCount; i++) {
        const HintSpec *h = &L->hints[i];
        float ax = h->x * (float)TILE + TILE * 0.5f;
        float ay = h->y * (float)TILE + TILE * 0.5f;
        /* 세로 거리는 절반만 센다. 겹침을 피하려고 앵커를 위로 올린 것이
         * 가시성을 깎으면 안 된다 — 레벨은 대체로 가로로 흐른다. */
        float dx = (pcx - ax) / TILE, dy = (pcy - ay) / TILE * 0.45f;
        float d = sqrtf(dx * dx + dy * dy);
        if (d > h->range + 1.5f) continue;

        int at = n;
        while (at > 0 && pickD[at - 1] > d) at--;
        if (at >= HINT_MAX_SHOWN) continue;
        for (int k = (n < HINT_MAX_SHOWN ? n : HINT_MAX_SHOWN - 1); k > at; k--) {
            pick[k] = pick[k - 1]; pickD[k] = pickD[k - 1];
        }
        pick[at] = i; pickD[at] = d;
        if (n < HINT_MAX_SHOWN) n++;
    }

    for (int s = 0; s < n; s++) {
        const HintSpec *h = &L->hints[pick[s]];
        float ax = h->x * (float)TILE + TILE * 0.5f;
        float ay = h->y * (float)TILE;
        float d = pickD[s], r = (float)h->range;
        float a = (d <= r) ? 1.0f : (1.0f - (d - r) / 1.5f);
        if (a <= 0.0f) continue;

        /* 크기 계산 */
        int kw = 0, kn = 0;
        if (h->keys) {
            const char *s = h->keys;
            while (*s) {
                const char *e = s;
                while (*e && *e != ' ') e++;
                char tmp[16]; int n = (int)(e - s); if (n > 15) n = 15;
                memcpy(tmp, s, n); tmp[n] = '\0';
                kw += MeasureText(tmp, 10) + 13;
                kn++;
                s = (*e) ? e + 1 : e;
            }
            if (kw > 0) kw -= 4;
        }
        int tw = h->l1 ? MeasureText(h->l1, 10) : 0;
        int t2 = h->l2 ? MeasureText(h->l2, 10) : 0;
        if (t2 > tw) tw = t2;
        int bw = (kw > tw ? kw : tw) + 14;
        int bh = 8 + (kn ? 17 : 0) + (h->l1 ? 12 : 0) + (h->l2 ? 11 : 0);

        int bx = (int)ax - camX - bw / 2;
        int by = (int)ay - bh - 10;
        if (bx < 2) bx = 2;
        if (bx + bw > VIEW_W - 2) bx = VIEW_W - 2 - bw;
        if (by < 32) by = 32;

        DrawRectangle(bx, by, bw, bh, Fade((Color){ 10, 13, 24, 255 }, a * 0.92f));
        DrawRectangleLines(bx, by, bw, bh, Fade((Color){ 70, 84, 118, 255 }, a));
        /* 앵커를 가리키는 꼬리 */
        DrawRectangle((int)ax - camX - 1, by + bh, 2, 6, Fade((Color){ 70, 84, 118, 255 }, a));

        int cy = by + 5;
        if (kn) {
            int kx = bx + (bw - kw) / 2;
            const char *s = h->keys;
            while (*s) {
                const char *e = s;
                while (*e && *e != ' ') e++;
                kx += draw_keycap(kx, cy, s, (int)(e - s), a);
                s = (*e) ? e + 1 : e;
            }
            cy += 17;
        }
        if (h->l1) {
            DrawText(h->l1, bx + (bw - MeasureText(h->l1, 10)) / 2, cy, 10,
                     Fade((Color){ 226, 234, 255, 255 }, a));
            cy += 12;
        }
        if (h->l2)
            DrawText(h->l2, bx + (bw - MeasureText(h->l2, 10)) / 2, cy, 10,
                     Fade(C_DIM, a));
    }
    (void)t;
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
                case T_GOAL: {
                    int bob = ((int)(t * 3) % 2);
                    DrawRectangle(px + 2, py + 2 + bob, TILE - 4, TILE - 4, C_GOAL);
                    DrawRectangle(px + 4, py + 3 + bob, TILE - 8, 3, (Color){ 20, 60, 70, 255 });
                    DrawRectangle(px + 4, py + 9 + bob, TILE - 8, 3, (Color){ 226, 226, 214, 255 });
                } break;
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
    const char *lock = w->atTerminal ? "WRITE PROTECT DISABLED" : "WRITE PROTECTED";
    Color lc = w->atTerminal ? C_TERM : (Color){ 150, 110, 110, 255 };
    DrawText(lock, VIEW_W - MeasureText(lock, 10) - 6, 17, 10, lc);

    /* 로드아웃 패널 */
    int nUnlocked = 0;
    for (int a = 0; a < AB_COUNT; a++) if (sim_ability_unlocked(w, a)) nUnlocked++;
    int px = 6, py = 36, pw = 152, ph = nUnlocked * 12 + 16;
    DrawRectangle(px, py, pw, ph, (Color){ 8, 10, 18, w->atTerminal ? 228 : 150 });
    DrawRectangleLines(px, py, pw, ph, w->atTerminal ? Fade(C_TERM, 0.7f) : (Color){ 34, 40, 60, 255 });
    DrawText(w->atTerminal ? "UTILITIES" : "UTILITIES  (locked)",
             px + 5, py + 4, 10, w->atTerminal ? C_TERM : C_OFF);
    /* ★ 아직 손에 넣은 적 없는 유틸리티는 목록에 아예 안 뜬다.
     *   존재를 모르는 게 맞다 — 회수한 디스크에서 하나씩 나온다. */
    int row = 0;
    for (int a = 0; a < AB_COUNT; a++) {
        if (!sim_ability_unlocked(w, a)) continue;
        int ly = py + 17 + row * 12;
        row++;
        int on = p->has[a];
        int here = sim_ability_available(w, a);
        Color c = on ? AB_COLOR[a] : (w->atTerminal && here ? C_DIM : (Color){ 38, 44, 62, 255 });
        DrawRectangle(px + 5, ly + 1, 6, 6, on ? AB_COLOR[a] : (Color){ 24, 28, 42, 255 });
        DrawRectangleLines(px + 5, ly + 1, 6, 6, c);
        snprintf(buf, sizeof(buf), "%d %-10s %d", a + 1, ABILITY_FILE[a], ABILITY_COST[a]);
        DrawText(buf, px + 15, ly, 10, c);
        /* 이 섹터에 없는 유틸리티는 오른쪽에 따로 표기한다.
         * 같은 좌표에 겹쳐 그리면 둘 다 못 읽는다 */
        if (w->atTerminal && !here && !on)
            DrawText("absent", px + pw - 38, ly, 10, (Color){ 96, 62, 70, 255 });
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

static void draw_clear(const World *w, unsigned gained)
{
    const Level *L = sim_level(w);
    char buf[96], num[32];
    DrawRectangle(0, 0, VIEW_W, VIEW_H, (Color){ 6, 14, 16, 205 });
    center("RECOVERED", 62, 20, C_GOAL);
    commas(L->fileBytes, num, sizeof(num));
    snprintf(buf, sizeof(buf), "%s        %s bytes", L->file, num);
    center(buf, 96, 10, C_TEXT);
    center(L->meta, 112, 10, C_DIM);

    /* 회수한 디스크 안에 유틸리티가 들어 있었다 */
    int y = 142;
    for (int a = 0; a < AB_COUNT; a++) {
        if (!(gained & AB_BIT(a))) continue;
        center("the disk contained a utility", y, 10, C_DIM);
        snprintf(buf, sizeof(buf), "+ %s     %d slots", ABILITY_FILE[a], ABILITY_COST[a]);
        center(buf, y + 14, 10, AB_COLOR[a]);
        y += 34;
    }
    snprintf(buf, sizeof(buf), "%.1f seconds     %d retries", w->time, w->deaths);
    center(buf, y + 4, 10, C_DIM);
    center("[ENTER]  next sector", VIEW_H - 46, 10, C_TEXT);
}

static void draw_end(const World *w, int recovered, long bytes)
{
    char buf[96], num[32];
    ClearBackground(C_BG);
    DrawText("A:\\> _", 40, 60, 20, C_GOAL);
    snprintf(buf, sizeof(buf), "%d files recovered", recovered);
    DrawText(buf, 56, 110, 10, C_TEXT);
    commas(bytes, num, sizeof(num));
    snprintf(buf, sizeof(buf), "%s bytes written", num);
    DrawText(buf, 56, 126, 10, C_TEXT);
    snprintf(buf, sizeof(buf), "%d retries", w->deaths);
    DrawText(buf, 56, 142, 10, C_DIM);
    DrawText("disk ejected.", 56, 174, 10, C_DIM);
    center("[ENTER]  again", VIEW_H - 40, 10, C_DIM);
}

/* ── main ───────────────────────────────────────────────────────────── */
int main(int argc, char **argv)
{
    int shotMode = (argc > 1 && strcmp(argv[1], "--shot") == 0);
    int shotTile = (argc > 2) ? atoi(argv[2]) : -1;
    int shotLvl  = (argc > 3) ? atoi(argv[3]) : 0;

    Tune tune;
    tune_defaults(&tune);
    char toast[128] = { 0 };
    float toastTimer = 0.0f;
    load_tune(&tune, "tune.txt", toast, sizeof(toast));

    World world;
    sim_new_game(&world, &tune);
    if (shotMode && shotLvl > 0) {
        /* 스크린샷용: 그 레벨까지의 보상을 미리 지급한다 */
        for (int i = 0; i < shotLvl && i < LEVEL_COUNT; i++) world.unlocked |= LEVELS[i].reward;
        sim_load(&world, &tune, shotLvl);
    }

    Screen screen = shotMode ? SCR_PLAY : SCR_TITLE;
    int  recovered = 0;
    long bytesSaved = 0;
    unsigned gained = 0;

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(VIEW_W * 2, VIEW_H * 2, "LAST WRITE");
    SetTargetFPS(60);
    SetExitKey(KEY_ESCAPE);

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
    while (!WindowShouldClose()) {
        float dt = 1.0f / 60.0f;
        int enter = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);

        if (IsKeyPressed(KEY_F5)) { load_tune(&tune, "tune.txt", toast, sizeof(toast)); toastTimer = 2.5f; }
        if (toastTimer > 0) toastTimer -= dt;

        if (screen == SCR_TITLE) {
            if (enter) { recovered = 0; bytesSaved = 0; gained = 0;
                         sim_new_game(&world, &tune); screen = SCR_PLAY; }
        } else if (screen == SCR_CLEAR) {
            if (enter) {
                if (world.levelIndex + 1 < LEVEL_COUNT) {
                    sim_load(&world, &tune, world.levelIndex + 1);
                    screen = SCR_PLAY;
                } else screen = SCR_END;
            }
        } else if (screen == SCR_END) {
            if (enter) screen = SCR_TITLE;
        } else {
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
                recovered++;
                bytesSaved += sim_level(&world)->fileBytes;
                gained = sim_grant_reward(&world);   /* 디스크 안에 들어 있던 유틸리티 */
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
            else if (screen == SCR_END)  draw_end(&world, recovered, bytesSaved);
            else {
                draw_world(&world, (float)GetTime(), camX);
                draw_hints(&world, camX, (float)GetTime());
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

        if (shotMode && ++frames >= 30) { TakeScreenshot("shot.png"); break; }
    }

    UnloadRenderTexture(canvas);
    CloseWindow();
    return 0;
}
