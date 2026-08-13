/* main.c — 창·입력·렌더링만. 게임 규칙은 전부 sim.c 에 있다.
 *
 * 실행 인자:  --shot [x]   30프레임 뒤 shot.png 를 남기고 종료 (x = 시작 타일)  */
#include "raylib.h"
#include "sim.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const Color C_BG      = {  10,  13,  24, 255 };
static const Color C_GRID    = {  17,  22,  38, 255 };
static const Color C_SOLID   = {  44,  51,  72, 255 };
static const Color C_SOLID_T = {  74,  86, 118, 255 };
static const Color C_GOAL    = {  92, 226, 224, 255 };
static const Color C_TERM    = { 120, 240, 180, 255 };
static const Color C_BODY    = {  38,  42,  58, 255 };
static const Color C_TEXT    = { 196, 204, 226, 255 };
static const Color C_DIM     = {  86,  96, 124, 255 };
static const Color C_OFF     = {  52,  60,  84, 255 };
static const Color C_FMT     = { 214,  60,  90, 255 };

static const Color AB_COLOR[AB_COUNT] = {
    { 118, 200, 255, 255 },   /* jump2 */
    { 255, 150,  80, 255 },   /* dash  */
    { 170, 230, 150, 255 },   /* glide */
    { 230, 150, 230, 255 },   /* grip  */
    { 245, 215, 110, 255 },   /* boot  */
};

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
                    /* 눈에 확 띄어야 한다 — 여기서만 로드아웃을 바꿀 수 있으므로 */
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

    /* ── 플레이어 ── */
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

    Color face = (Color){ 120, 220, 240, 255 };
    if (p->dashTimer > 0.0f)   face = AB_COLOR[AB_DASH];
    else if (p->wallDir != 0)  face = AB_COLOR[AB_GRIP];
    else if (!p->onGround)     face = AB_COLOR[AB_JUMP2];
    int ex = (p->facing > 0) ? 1 : -1;
    DrawRectangle(x + 3 + ex, y + 6, 2, 2, face);
    DrawRectangle(x + 7 + ex, y + 6, 2, 2, face);
    DrawRectangle(x + 4 + ex, y + 10, p->onGround ? 4 : 2, 1, face);
    if (p->has[AB_JUMP2] && p->airJumps > 0 && !p->onGround)
        DrawRectangle(x + PLAYER_W / 2 - 1, y - 3, 2, 2, AB_COLOR[AB_JUMP2]);

    /* ── 포맷 파동 ── 왼쪽은 이미 지워진 영역 ── */
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
    int freeSlots = sim_free_slots(w);
    char buf[96], num[32];

    DrawRectangle(0, 0, VIEW_W, 30, (Color){ 8, 10, 18, 215 });
    commas(TOTAL_BYTES, num, sizeof(num));
    snprintf(buf, sizeof(buf), "MY DISK  %s B", num);
    DrawText(buf, 6, 4, 10, C_DIM);

    int cell = 0;
    for (int a = 0; a < AB_COUNT; a++)
        if (p->has[a])
            for (int k = 0; k < ABILITY_COST[a]; k++, cell++)
                DrawRectangle(6 + cell * 11, 17, 9, 9, AB_COLOR[a]);
    for (; cell < CAP_SLOTS; cell++)
        DrawRectangle(6 + cell * 11, 17, 9, 9, (Color){ 22, 26, 40, 255 });
    for (int i = 0; i < CAP_SLOTS; i++)
        DrawRectangleLines(6 + i * 11, 17, 9, 9, (Color){ 52, 60, 84, 255 });

    commas((long)freeSlots * BYTES_PER_SLOT, num, sizeof(num));
    snprintf(buf, sizeof(buf), "%s B FREE", num);
    DrawText(buf, 6 + CAP_SLOTS * 11 + 8, 17, 10, freeSlots ? C_TEXT : C_DIM);

    snprintf(buf, sizeof(buf), "%.1fs  DEATHS %d", w->time, w->deaths);
    DrawText(buf, VIEW_W - MeasureText(buf, 10) - 6, 4, 10, C_DIM);

    /* 잠금 상태 — 이 게임의 규칙이 여기에 다 있다 */
    const char *lock = w->atTerminal ? "TERMINAL  -  1-5 to swap" : "LOADOUT LOCKED";
    Color lc = w->atTerminal ? C_TERM : (Color){ 150, 110, 110, 255 };
    DrawText(lock, VIEW_W - MeasureText(lock, 10) - 6, 17, 10, lc);

    /* 로드아웃 패널 — 터미널에서만 밝게 */
    int px = 6, py = 36, pw = 148, ph = AB_COUNT * 12 + 16;
    DrawRectangle(px, py, pw, ph, (Color){ 8, 10, 18, w->atTerminal ? 225 : 150 });
    DrawRectangleLines(px, py, pw, ph, w->atTerminal ? Fade(C_TERM, 0.7f) : (Color){ 34, 40, 60, 255 });
    DrawText(w->atTerminal ? "LOADOUT  (editable)" : "LOADOUT  (locked)",
             px + 5, py + 4, 10, w->atTerminal ? C_TERM : C_OFF);
    for (int a = 0; a < AB_COUNT; a++) {
        int ly = py + 17 + a * 12;
        int on = p->has[a];
        Color c = on ? AB_COLOR[a] : C_OFF;
        if (!w->atTerminal && !on) c = (Color){ 38, 44, 62, 255 };
        DrawRectangle(px + 5, ly + 1, 6, 6, on ? AB_COLOR[a] : (Color){ 24, 28, 42, 255 });
        DrawRectangleLines(px + 5, ly + 1, 6, 6, c);
        snprintf(buf, sizeof(buf), "%d %-10s %d", a + 1, ABILITY_FILE[a], ABILITY_COST[a]);
        DrawText(buf, px + 15, ly, 10, c);
    }

    const char *keys = "MOVE arrows   JUMP space   DASH shift   RESET r   TUNE f5";
    DrawRectangle(0, VIEW_H - 13, VIEW_W, 13, (Color){ 8, 10, 18, 200 });
    DrawText(keys, VIEW_W / 2 - MeasureText(keys, 10) / 2, VIEW_H - 11, 10, C_DIM);

    if (toastTimer > 0.0f)
        DrawText(toast, px + pw + 10, 36, 10, Fade(C_GOAL, toastTimer > 1.0f ? 1.0f : toastTimer));
    if (w->checkFlash > 0.0f) {
        const char *m = "CHECKPOINT  -  format halted";
        DrawText(m, VIEW_W / 2 - MeasureText(m, 10) / 2, 60, 10, Fade(C_TERM, w->checkFlash));
    }
    if (w->denyFlash > 0.0f) {
        const char *d = w->atTerminal ? "NOT ENOUGH SPACE" : "CAN ONLY SWAP AT A TERMINAL";
        DrawText(d, VIEW_W / 2 - MeasureText(d, 10) / 2, 74, 10,
                 Fade((Color){ 255, 120, 90, 255 }, w->denyFlash * 2));
    }

    if (w->state == ST_WON) {
        DrawRectangle(0, 0, VIEW_W, VIEW_H, (Color){ 6, 14, 16, 195 });
        const char *a = "DISK RECOVERED";
        DrawText(a, VIEW_W / 2 - MeasureText(a, 20) / 2, VIEW_H / 2 - 22, 20, C_GOAL);
        snprintf(buf, sizeof(buf), "%.1fs   %d deaths   loadout %d slots",
                 w->time, w->deaths, sim_used_slots(w));
        DrawText(buf, VIEW_W / 2 - MeasureText(buf, 10) / 2, VIEW_H / 2 + 4, 10, C_TEXT);
        const char *c = "press R and try a different loadout";
        DrawText(c, VIEW_W / 2 - MeasureText(c, 10) / 2, VIEW_H / 2 + 18, 10, C_DIM);
    }
}

int main(int argc, char **argv)
{
    int shotMode = (argc > 1 && strcmp(argv[1], "--shot") == 0);
    int shotTile = (argc > 2) ? atoi(argv[2]) : -1;

    Tune tune;
    tune_defaults(&tune);
    char toast[128] = { 0 };
    float toastTimer = 0.0f;
    load_tune(&tune, "tune.txt", toast, sizeof(toast));
    toastTimer = 2.5f;

    World world;
    memset(&world, 0, sizeof(world));
    sim_init(&world);
    world.p.has[AB_BOOT] = 1;   /* 3칸 */
    world.p.has[AB_DASH] = 1;   /* 4칸 — 7/12, 터미널에서 나머지를 채우면 된다 */
    if (shotTile >= 0) { world.p.x = shotTile * (float)TILE; world.formatX = world.p.x - 500.0f; }

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(VIEW_W * 2, VIEW_H * 2, "FLOPPY - 1.44MB prototype");
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

    int frames = 0;
    while (!WindowShouldClose()) {
        float dt = 1.0f / 60.0f;

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

        if (IsKeyPressed(KEY_R)) sim_init(&world);
        if (IsKeyPressed(KEY_F5)) { load_tune(&tune, "tune.txt", toast, sizeof(toast)); toastTimer = 2.5f; }
        if (toastTimer > 0) toastTimer -= dt;

        sim_step(&world, &tune, &in, dt);

        int camX = (int)(world.p.x + PLAYER_W / 2.0f) - VIEW_W / 2;
        if (camX < 0) camX = 0;
        if (camX > MAP_W * TILE - VIEW_W) camX = MAP_W * TILE - VIEW_W;

        BeginTextureMode(canvas);
            draw_world(&world, (float)GetTime(), camX);
            draw_hud(&world, toast, toastTimer);
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
