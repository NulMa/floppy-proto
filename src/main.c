/* main.c — 창·입력·렌더링만. 게임 규칙은 전부 sim.c 에 있다.
 *
 * 실행 인자:  --shot [x] [level] [frame]   30프레임 뒤 shot.png 를 남기고 종료
 *            --shotmenu · --shottitle · --shotclear · --shotdeath · --shotend
 *                                        각 화면을 그대로 찍는다 */
#include "raylib.h"
#include "sim.h"
#include "progress.h"
#include "sfx.h"
#include "bgm.h"
#include "art/floppy_png.h"
#include "art/ground_top_png.h"
#include "art/ground_fill_png.h"
#include "art/oneway_png.h"
#include "art/spike_png.h"
#include "art/pad_png.h"
#include "art/exit_png.h"
#include "art/disk_png.h"
#include "art/terminal_png.h"
#include "art/icons_png.h"
#include "art/bg_tile_png.h"
#include "art/logo_png.h"
#include "art/crumble_png.h"
#include "art/ending_png.h"
#include "art/font16_png.h"
#include "art/font16_map.h"
#include "strings.h"
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
/* 교대 가시 B — A(붉은색)와 **색으로 갈려야** 어느 쪽이 짝인지 한눈에 읽힌다.
 * 켜짐/꺼짐은 밝기로, A/B 짝은 색상으로 구분한다. */
static const Color C_SPIKE_B = { 216, 128,  76, 255 };
static const Color C_UPDRAFT = { 120, 200, 240, 255 };
static const Color C_CRUMBLE = {  72,  62,  58, 255 };
static const Color C_EXIT  = { 120, 240, 180, 255 };

/* ── 플레이어 스프라이트 ─────────────────────────────────────────────
 *
 * art/floppy.png 13프레임 (16x16). exe 안에 png 바이트로 들어 있다 (art/floppy_png.h).
 * 충돌은 12x14 인데 그림은 16x16 이라 **좌우 2px, 위 2px 이 판정 밖으로 삐져나온다.**
 * 벽에 붙으면 그림이 벽을 2px 파고드는 게 정상이다 — 지형 옆면은 그 전제로 그린다.
 *
 * 프레임 순서는 art/floppy.png 왼쪽부터. 바꾸면 아래 상수도 같이 고쳐야 한다. */
#define SPR 16
enum {
    FR_IDLE = 0,      /* 0,1   대기 — 2초 주기 */
    FR_RUN  = 2,      /* 2,3,4 달리기 — 8fps */
    FR_RISE = 5,
    FR_FALL = 6,
    FR_DASH = 7,
    FR_GLIDE = 8,     /* 8,9 */
    FR_WALL = 10,
    FR_TERM = 11      /* 11,12 쓰기 보호 해제 — 탭 구멍이 뚫린다 */
};
/* ── 아트 ────────────────────────────────────────────────────────────
 *
 * 전부 exe 안의 png 바이트다 (art/*_png.h). **아직 안 그려진 것은 코드가 계속
 * 사각형으로 그린다** — 파동·로고·엔딩. 아트가 절반만 끝나도 제출할 수 있어야 한다는
 * `아트설계.md` §0 의 전제가 여기서 지켜진다.
 *
 * 프레임은 **가로로만** 이어붙는다. 세로로 늘리면 여기가 행까지 알아야 한다. */
static Texture2D texFloppy, texGroundTop, texGroundFill, texOneway, texSpike,
                 texPad, texExit, texDisk, texTerminal, texIcons, texBgTile, texLogo,
                 texEnding, texCrumble;

/* ★ terminal.png 의 밝은 쪽이 0번이다 (실측 평균밝기 89 vs 58).
 *   아트설계 표의 "꺼짐 · 켜짐" 순서와 반대라, 뒤집고 싶으면 이 두 줄만 바꾼다. */
#define TERM_FR_ON  0
#define TERM_FR_OFF 1

/* spike.png 2프레임. 색은 코드가 입힌다 (A=ROSE / B=주황) */
/* crumble.png — 0 금감 · 1 무너지는 중 */
#define CRUMBLE_FR_IDLE  0
#define CRUMBLE_FR_BREAK 1

#define SPK_ON  0
#define SPK_OFF 1

static Texture2D load_png(const unsigned char *data, int size)
{
    Image im = LoadImageFromMemory(".png", data, size);
    Texture2D t = LoadTextureFromImage(im);
    UnloadImage(im);
    SetTextureFilter(t, TEXTURE_FILTER_POINT);
    return t;
}


/* 사각 테두리 — **raylib 의 DrawRectangleLines 를 안 쓴다.**
 *
 * ★ 5.5 의 그것은 GL_LINES 로 그리는데 두 군데가 어긋난다 (rshapes.c:808):
 *     · 세로선이 x+width 에 간다. 그런데 DrawRectangle(x,y,w,h) 가 칠하는 건
 *       x..x+w-1 이다 — 테두리가 채움면보다 **오른쪽·아래로 1px 크다**
 *     · 가로선을 양 끝으로 zoomFactor(=0.5px) 씩 늘린다. 512x288 캔버스에
 *       1:1 로 그리므로 그 0.5px 이 **오른쪽 위·아래 모서리에 튀어나온 점**이 된다
 *   확대해서 보는 픽셀아트에서는 둘 다 그대로 보인다. 그래서 1px 짜리 채움
 *   사각형 4개로 직접 그린다 — 정수 좌표라 어긋날 데가 없다. */
static void frame_rect(int x, int y, int w, int h, Color c)
{
    if (w <= 0 || h <= 0) return;
    DrawRectangle(x,         y,         w, 1, c);
    DrawRectangle(x,         y + h - 1, w, 1, c);
    if (h > 2) {
        DrawRectangle(x,         y + 1, 1, h - 2, c);
        DrawRectangle(x + w - 1, y + 1, 1, h - 2, c);
    }
}

/* 아틀라스에서 Font 를 **손으로 조립한다.**
 *
 * ★ raylib 의 `LoadFontFromImage` 는 글리프를 구분자 색으로 갈라 놓은 **한 줄짜리**
 *   그림만 받는다. 400개가 넘으면 폭이 6천 픽셀이 되어 텍스처 한 장에 안 들어간다.
 *   그래서 32칸씩 접은 격자로 굽고(`make-font-atlas.ps1`) 여기서 recs 를 직접 채운다.
 *   Font 는 그냥 구조체라 이렇게 채워도 DrawTextEx 가 그대로 쓴다.
 *
 * ★ `advanceX` 를 넣어 두면 DrawTextEx 가 recs 폭 대신 이걸 쓴다. 반각 8 / 전각 16
 *   이 그대로 지켜져서 spacing 을 0 으로 둘 수 있다 — 자간이 어긋날 데가 없다.
 *
 * ★ `glyphs[i].image` 는 비워 둔다. 텍스처를 이미 갖고 있어서 낱장 Image 가 필요 없고,
 *   UnloadFont 가 빈 Image 를 풀어도 안전하다 (NULL free). */
static Font load_font16(void)
{
    Font f = { 0 };
    Image im = LoadImageFromMemory(".png", FONT16_PNG, (int)sizeof(FONT16_PNG));

    f.baseSize     = FONT16_CELL;
    f.glyphCount   = FONT16_COUNT;
    f.glyphPadding = 0;
    f.texture      = LoadTextureFromImage(im);
    f.recs         = (Rectangle *)MemAlloc(FONT16_COUNT * sizeof(Rectangle));
    f.glyphs       = (GlyphInfo *)MemAlloc(FONT16_COUNT * sizeof(GlyphInfo));
    UnloadImage(im);

    for (int i = 0; i < FONT16_COUNT; i++) {
        int col = i % FONT16_COLS, row = i / FONT16_COLS;
        int adv = FONT16_ADV[i];
        /* recs 폭을 자간에 맞춘다. 반각 글자에 16px 을 주면 옆칸 글자가 딸려 나온다. */
        f.recs[i] = (Rectangle){ (float)(col * FONT16_CELL), (float)(row * FONT16_CELL),
                                 (float)adv, (float)FONT16_CELL };
        f.glyphs[i].value    = FONT16_CP[i];
        f.glyphs[i].offsetX  = 0;
        f.glyphs[i].offsetY  = 0;
        f.glyphs[i].advanceX = adv;
        f.glyphs[i].image    = (Image){ 0 };
    }

    /* 확대는 정수배로만 하고 보간은 안 한다. 픽셀 폰트라 흐려지면 끝이다. */
    SetTextureFilter(f.texture, TEXTURE_FILTER_POINT);
    return f;
}
/* 가로 스트립에서 프레임 하나를 찍는다 */
static void draw_frame(Texture2D tex, int frame, int fw, int fh, int x, int y, Color tint)
{
    Rectangle src = { (float)(frame * fw), 0.0f, (float)fw, (float)fh };
    Vector2   pos = { (float)x, (float)y };
    DrawTextureRec(tex, src, pos, tint);
}


/* 위 칸이 이 칸을 **덮는가.** 덮으면 윗면(ground_top)을 그릴 이유가 없다.
 *
 * ★ 예전엔 `T_SOLID` 만 봤다. 그런데 점프패드와 무너지는 발판은 **지형과 똑같이
 *   한 칸을 통째로 차지한다** (`sim.c` 의 solid 판정이 정확히 이 셋이다).
 *   그 아래에 윗면을 깔면, 덮여서 안 보이는 풀밭을 그리는 셈이고 —
 *   패드 옆에서 지형 실루엣이 한 칸 튀어 보인다. 아래는 속(ground_fill)이 맞다. */
static int tile_covers(int t)
{
    return t == T_SOLID || t == T_PAD || t == T_CRUMBLE;
}

/* 상태 → 프레임. 우선순위가 곧 "무엇이 제일 중요한 정보인가"의 순서다. */
static int floppy_frame(const World *w, const Player *p, float t)
{
    if (p->dashTimer > 0.0f) return FR_DASH;
    if (p->wallDir != 0)     return FR_WALL;
    if (!p->onGround) {
        /* 활공 판정은 아래 활공 잔상과 **같은 조건**이어야 한다. 어긋나면
         * 날개는 폈는데 잔상이 없는 프레임이 생긴다. */
        if (p->has[AB_GLIDE] && p->vy > 0 && p->vy <= 80.0f)
            return FR_GLIDE + ((int)(t * 6.0f) & 1);
        return (p->vy < 0) ? FR_RISE : FR_FALL;
    }
    /* 터미널 — 서 있을 때만. 지나쳐 뛰는 중에 쓰기 보호가 풀린 그림이 나오면 거짓말이다 */
    if (w->atTerminal && (p->vx > -8.0f && p->vx < 8.0f))
        return FR_TERM + ((int)(t / 0.75f) & 1);          /* 1.5초 주기 호흡 */
    if (p->vx < -8.0f || p->vx > 8.0f) {
        static const int cycle[4] = { 0, 1, 2, 1 };        /* 2-3-4-3 */
        return FR_RUN + cycle[(int)(t * 8.0f) & 3];
    }
    return FR_IDLE + ((int)t & 1);
}

typedef enum { SCR_TITLE = 0, SCR_MENU, SCR_PLAY, SCR_CLEAR, SCR_END } Screen;

#define SAVE_PATH "save.dat"

/* 볼륨 한 칸. 10% 면 기본값 30% 에서 0 까지 세 번이라 손이 안 아프다 */
#define VOL_STEP 0.10f

/* 난이도 티어 — 스테이지 인덱스(0부터)로 판정한다.
 *   s01~s05 쉬움 / s06~s15 중간 / s16~s20 어려움  */
static int tier_of(int idx) { return (idx < 5) ? 0 : (idx < 15 ? 1 : 2); }
static const char *TIER_NAME[3] = { S_TIER_EASY, S_TIER_MEDIUM, S_TIER_HARD };
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

/* ── 글자 ────────────────────────────────────────────────────────────
 *
 * raylib 기본 폰트를 안 쓴다. 한글이 없기 때문이다.
 *
 * ★ neoDGM(둥근모꼴) 16px 을 **아틀라스로 구워서** 넣는다. TTF 를 그대로 넣으면
 *   폰트만 651,044 B 로 예산 여유(444,416 B)를 넘고, raylib 의 TTF 경로는
 *   stb_truetype 고정비를 +38,912 B 더 낸다. 아틀라스는 +2 KB 대다.
 *   원본이 비트맵 폰트라 16px 로 구우면 픽셀이 원본과 **똑같다** — 잃는 게 없다.
 *   자세한 건 `art/font/README.md` · `게임스펙.md §9`.
 *
 * ★ **반각 8px · 전각 16px · 높이 16px 로 딱 떨어진다.** 그래서 폭이
 *   `8*영문 + 16*한글` 이고, 한 줄 한계가 **영문 64자 / 한글 32자**다.
 *
 * ★ 크기는 16 과 32 **둘뿐이다.** 정수배가 아니면 픽셀이 깨진다. 예전 10/20px
 *   두 단계를 그대로 옮긴 것이고, 본문이 10→16 으로 커진 만큼 화면마다
 *   자리를 다시 쟀다 (`docs/폰트_레이아웃.md`). */
static Font fnt;

#define FS   16.0f      /* 본문 */
#define FSB  32.0f      /* 제목 — 정수 2배 */

static void txt(const char *s, int x, int y, Color c)
{
    DrawTextEx(fnt, s, (Vector2){ (float)x, (float)y }, FS, 0.0f, c);
}

static void txtb(const char *s, int x, int y, Color c)
{
    DrawTextEx(fnt, s, (Vector2){ (float)x, (float)y }, FSB, 0.0f, c);
}

static int txtw(const char *s)  { return (int)MeasureTextEx(fnt, s, FS,  0.0f).x; }
static int txtwb(const char *s) { return (int)MeasureTextEx(fnt, s, FSB, 0.0f).x; }

static void center(const char *s, int y, Color c)
{
    txt(s, VIEW_W / 2 - txtw(s) / 2, y, c);
}

static void centerb(const char *s, int y, Color c)
{
    txtb(s, VIEW_W / 2 - txtwb(s) / 2, y, c);
}

/* 점멸 — **끄지 않고 어둡게 한다.**
 *
 * ★ 글자가 통째로 사라졌다 나타나면 그 자리가 깜빡이는 구멍이 되어 화면이 덜컹거린다.
 *   자리는 늘 차 있고 밝기만 오가면, 눈길을 끄는 정도는 같은데 화면은 조용하다.
 *   꺼진 쪽은 C_OFF(46,54,76) — C_DIM(86,96,124) 보다 확실히 어둡지만 배경에는 안 묻힌다. */
static void center_blink(const char *s, int y, Color on, int lit)
{
    center(s, y, lit ? on : C_OFF);
}

/* ── 힌트 ────────────────────────────────────────────────────────────
 *
 * 월드 좌표에 붙어서 **항상** 떠 있는다. 박스 없이 텍스트만.
 * 화면 밖은 카메라가 잘라내므로 별도 범위 판정이 필요 없다.
 *
 * 배경이 없으니 지형 위에서 글자가 묻힌다 → 1px 그림자로 읽히게 한다. */
static void text_shadow(const char *s, int x, int y, Color c)
{
    txt(s, x + 1, y + 1, (Color){ 6, 8, 14, 220 });
    txt(s, x, y, c);
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

        /* ★ 능력 사용법은 **막 얻은 스테이지에서만** 알려준다.
         *   관문마다 답을 적어두면 플레이어가 전략을 짜는 게 아니라 지시를 따르게 된다. */
        if (h->teaches && hint_school_level(h->teaches) != w->levelIndex) continue;

        int ax = h->x * TILE + TILE / 2 - camX;
        int ay = h->y * TILE;

        char keys[64] = { 0 };
        if (h->keys) bracket_keys(h->keys, keys, sizeof(keys));

        int wk = keys[0] ? txtw(keys) : 0;
        int w1 = h->l1 ? txtw(h->l1) : 0;
        int w2 = h->l2 ? txtw(h->l2) : 0;
        int bw = wk > w1 ? wk : w1;
        if (w2 > bw) bw = w2;
        /* 줄 높이 12 -> 18. 글자가 16px 이라 12 면 윗줄 아랫줄이 붙는다 */
        int bh = (keys[0] ? 18 : 0) + (h->l1 ? 18 : 0) + (h->l2 ? 17 : 0);

        int bx = ax - bw / 2;
        int by = ay - bh - 4;

        /* 화면 밖이면 그릴 필요 없다 */
        if (bx > VIEW_W + 8 || bx + bw < -8) continue;
        if (bx < 3) bx = 3;
        if (bx + bw > VIEW_W - 3) bx = VIEW_W - 3 - bw;
        if (by < 48) by = 48;   /* 상단 바가 44px */

        /* 겹치면 비켜준다. 위로만 밀면 천장에 막힌 것들이 한자리에 쌓인다 —
         * 위/아래를 번갈아 시도하고, 다 막히면 원위치로 둔다. */
        {
            int step = bh + 5, base = by, best = by;
            for (int tryN = 0; tryN < 7; tryN++) {
                int cand = base + ((tryN % 2) ? (tryN / 2 + 1) * step : -(tryN / 2) * step);
                if (tryN == 0) cand = base;
                if (cand < 48 || cand + bh > VIEW_H - 16) continue;
                int hit = 0;
                for (int k = 0; k < placed; k++)
                    /* 가로 여유를 넉넉히 준다. 딱 붙으면 겹치진 않아도 한 줄로 읽힌다. */
                    if (bx < px1[k] + 14 && bx + bw > px0[k] - 14 &&
                        cand < py1[k] + 3 && cand + bh > py0[k] - 3) { hit = 1; break; }
                if (!hit) { best = cand; break; }
            }
            by = best;
            if (by < 48) by = 48;
        }

        int cy = by;
        if (keys[0]) {
            text_shadow(keys, bx + (bw - wk) / 2, cy, (Color){ 150, 200, 255, 255 });
            cy += 18;
        }
        if (h->l1) {
            text_shadow(h->l1, bx + (bw - w1) / 2, cy, (Color){ 232, 238, 255, 255 });
            cy += 18;
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
static void draw_world(const World *w, const Tune *tn, float t, int camX)
{
    /* 지금 어느 쪽 교대 가시가 켜져 있는가 — **sim 과 같은 함수를 본다.**
     * 그림과 판정을 따로 계산하면 반드시 어긋나고, 어긋나면 "안 닿았는데 죽었다"가 된다. */
    unsigned char liveSpike = sim_spike_phase(w, tn);

    ClearBackground(C_BG);
    int c0 = camX / TILE, c1 = (camX + VIEW_W) / TILE + 1;
    if (c0 < 0) c0 = 0;
    if (c1 > MAP_W) c1 = MAP_W;

    /* 배경 — 빈 섹터. 월드 좌표에 붙여 타일링한다 (화면에 고정하면 스크롤할 때 미끄러진다) */
    for (int r = 0; r < MAP_H; r++)
        for (int c = c0; c < c1; c++)
            DrawTexture(texBgTile, c * TILE - camX, r * TILE, WHITE);

    for (int r = 0; r < MAP_H; r++)
        for (int c = c0; c < c1; c++) {
            int px = c * TILE - camX, py = r * TILE;
            switch (w->map[r][c]) {
                /* 지형 — 윗면 3장(왼끝/가운데/오른끝) + 속 1장.
                 * 윗면인지는 **위 칸이 지형이 아닌가**로 판정한다. 지도 밖은 지형으로 친다 */
                case T_SOLID: {
                    int top = (r == 0) || !tile_covers(w->map[r - 1][c]);
                    if (!top) { DrawTexture(texGroundFill, px, py, WHITE); break; }
                    int openL = (c == 0)         || (w->map[r][c - 1] != T_SOLID);
                    int openR = (c >= MAP_W - 1) || (w->map[r][c + 1] != T_SOLID);
                    /* 양쪽이 다 뚫린 1칸 기둥은 전용 프레임이 없다 — 왼끝으로 둔다 */
                    int fr = openL ? 0 : (openR ? 2 : 1);
                    draw_frame(texGroundTop, fr, TILE, TILE, px, py, WHITE);
                } break;

                /* 터미널 — 본체 위에 화면을 올린 스택. 판정은 아랫칸 하나뿐이고
                 * 그림만 위로 한 타일 넘친다. 지금 이 터미널에 서 있는가는
                 * spawn 좌표로 안다 (sim 이 밟은 칸을 거기 적어둔다). */
                case T_TERMINAL: {
                    int lit = w->atTerminal
                           && c == (int)w->spawnX / TILE
                           && r == (int)w->spawnY / TILE;
                    draw_frame(texTerminal, lit ? TERM_FR_ON : TERM_FR_OFF,
                               TILE, TILE * 2, px, py - TILE, WHITE);
                } break;

                case T_DISK:
                    DrawTexture(texDisk, px, py + ((int)(t * 3) % 2), WHITE);
                    break;

                case T_EXIT: {
                    /* 드라이브 슬롯 — 디스크가 들어가는 구멍.
                     * 보상 스테이지에서 디스크를 아직 안 주웠으면 **잠겨 있다** —
                     * 여기까지 와서야 알면 늦으므로 멀리서도 색으로 읽혀야 한다. */
                    /* 잠겼으면 빛나는 프레임으로 안 넘어간다 — 닫힌 채로 있는다 */
                    int lit = !sim_exit_locked(w) && ((int)(t * 3) % 2);
                    draw_frame(texExit, lit ? 1 : 0, TILE, TILE, px, py, WHITE);
                } break;

                /* 일반 가시 — 항상 켜져 있으므로 켜짐 프레임 하나만 쓴다 */
                case T_SPIKE:
                    draw_frame(texSpike, SPK_ON, TILE, TILE, px, py, C_SPIKE);
                    break;

                case T_PAD:
                    draw_frame(texPad, (int)(t * 4) % 2, TILE, TILE, px, py, WHITE);
                    break;

                case T_ONEWAY:
                    DrawTexture(texOneway, px, py, WHITE);
                    break;

                /* ── 교대 가시 ──
                 *
                 * ★ 꺼진 쪽도 **반드시 그린다.** 안 그리면 없던 가시가 갑자기 솟는
                 *   함정이 된다. 마리오 계열 설계론이 한목소리로 말하는 "블라인드 금지"가
                 *   이것이다 — 위험은 항상 보이고, 지금 켜져 있는지만 달라져야 한다.
                 *   꺼진 것은 바닥에 눕힌 납작한 밑동으로 그린다 (spike.png 1번 프레임).
                 *
                 * ★ 네 상태(A/B × 켜짐/꺼짐)를 **2프레임으로** 낸다.
                 *   그림은 명도만 갖고 있고(그레이스케일) 색은 여기서 입힌다 —
                 *   모양이 켜짐/꺼짐을, 색상이 A/B 를 가른다. 둘 다 밝기로 하면 안 갈린다. */
                case T_SPIKE_A:
                case T_SPIKE_B: {
                    int on = (w->map[r][c] == liveSpike);
                    Color base = (w->map[r][c] == T_SPIKE_A) ? C_SPIKE : C_SPIKE_B;
                    draw_frame(texSpike, on ? SPK_ON : SPK_OFF, TILE, TILE, px, py,
                               on ? base : Fade(base, 0.45f));
                } break;

                /* 상승 기류 — 위로 흐르는 점선. 활공 중일 때만 먹는다는 건
                 * 힌트가 말해준다. 여기서는 "무언가 위로 흐른다"만 읽히면 된다. */
                case T_UPDRAFT: {
                    /* ★ y 는 아래로 증가한다. 흐름을 **빼야** 위로 올라간다 —
                     *   예전엔 더해서 주석과 정반대로 아래로 흘렀다. 활공으로 타고
                     *   오르는 칸인데 아래로 흐르면 읽히는 방향이 뒤집힌다. */
                    int flow = (int)(t * 34.0f) % TILE;
                    DrawRectangle(px, py, TILE, TILE, Fade(C_UPDRAFT, 0.07f));
                    for (int k = 0; k < 3; k++) {
                        int uy = py + (TILE - 1 - ((flow + k * 6) % TILE));
                        DrawRectangle(px + 3 + (k % 2) * 7, uy - 2, 2, 3, Fade(C_UPDRAFT, 0.55f));
                    }
                    DrawRectangle(px, py, 1, TILE, Fade(C_UPDRAFT, 0.20f));
                    DrawRectangle(px + TILE - 1, py, 1, TILE, Fade(C_UPDRAFT, 0.20f));
                } break;

                /* 무너지는 발판 — `crumble.png` 2프레임 (금감 / 무너지는 중).
                 *
                 * ★ "무너지는 중"은 **맵에 안 적혀 있다.** 밟은 순간부터 사라질 때까지
                 *   타일 값은 계속 T_CRUMBLE 이고, 진행은 `w->crumble[]` 이 들고 있다
                 *   (`sim.h` — 리스폰 때 되돌리려고 원래 자리를 따로 보관한다).
                 *   그래서 여기서 그 목록을 뒤진다. 최대 32개라 훑어도 싸다.
                 *
                 * ★ 밟자마자 프레임이 바뀌어야 **밟기 전에 판단할 시간**이 생긴다.
                 *   crumbleDelay 가 0.35초뿐이라 이 0.35초가 경고의 전부다. */
                case T_CRUMBLE: {
                    int fr = CRUMBLE_FR_IDLE;
                    for (int k = 0; k < w->crumbleCount; k++)
                        if (w->crumble[k].r == r && w->crumble[k].c == c &&
                            w->crumble[k].state == 1) { fr = CRUMBLE_FR_BREAK; break; }
                    /* 무너지는 중엔 1px 씩 떤다. 정지 그림만으로는 "지금 진행 중"이 안 읽힌다 */
                    int shakeX = (fr == CRUMBLE_FR_BREAK) ? (((int)(t * 30) & 1) ? 1 : -1) : 0;
                    draw_frame(texCrumble, fr, TILE, TILE, px + shakeX, py, WHITE);
                } break;

                default: break;
            }
        }

    const Player *p = &w->p;
    int x = (int)p->x - camX, y = (int)p->y;

    /* 죽는 중 — 정상 스프라이트 대신 흩어지는 조각을 그린다.
     * 곧바로 되살리면 무슨 일이 일어났는지 못 본다. 낙사는 화면 밖이라 아무것도 안 그린다. */
    if (w->deathCause != DEATH_NONE) {
        if (w->deathCause != DEATH_FALL) {
            float k = 1.0f - (w->deathTimer / w->deathTimerMax);   /* 0 → 1 로 흩어진다 */
            Color dc = (w->deathCause == DEATH_SPIKE) ? C_SPIKE : C_FMT;
            for (int i = 0; i < 8; i++) {
                int ox = ((i * 37) % 13) - 6, oy = ((i * 53) % 11) - 5;
                int sz = 3 - (i % 2);
                DrawRectangle(x + PLAYER_W / 2 + (int)(ox * k * 3.0f),
                              y + PLAYER_H / 2 + (int)(oy * k * 3.0f) - (int)(k * 12),
                              sz, sz, Fade(dc, 1.0f - k * 0.75f));
            }
            frame_rect(x, y, PLAYER_W, PLAYER_H, Fade(dc, 0.7f - k * 0.7f));
        }
    } else {

    if (p->dashTimer > 0.0f)
        for (int i = 1; i <= 3; i++)
            DrawRectangle(x - p->dashDir * i * 5, y, PLAYER_W, PLAYER_H, Fade(AB_COLOR[AB_DASH], 0.30f / i));
    if (p->has[AB_GLIDE] && !p->onGround && p->vy > 0 && p->vy <= 80.0f) {
        DrawRectangle(x - 3, y + PLAYER_H - 2, PLAYER_W + 6, 2, Fade(AB_COLOR[AB_GLIDE], 0.75f));
        DrawRectangle(x - 5, y + PLAYER_H + 2, PLAYER_W + 10, 1, Fade(AB_COLOR[AB_GLIDE], 0.35f));
    }
    if (p->wallDir != 0)
        DrawRectangle(p->wallDir < 0 ? x - 2 : x + PLAYER_W, y + 2, 2, PLAYER_H - 4, AB_COLOR[AB_GRIP]);

    /* 몸 — art/floppy.png. 그림 16x16 을 판정 12x14 위에 **가로 중앙·아래 정렬**로 얹는다 */
    int scared = ((int)p->x - (int)w->formatX) < 90;
    int shake = (scared && !w->atTerminal && ((int)(t * 20) & 1)) ? 1 : 0;

    int fr = floppy_frame(w, p, t);
    /* 벽붙기 프레임은 오른쪽 벽에 붙은 모습으로 그려져 있다. 보는 방향이 아니라
     * **붙은 방향**으로 뒤집어야 벽에 등을 대고 서는 그림이 나온다. */
    int flip = (fr == FR_WALL) ? (p->wallDir < 0) : (p->facing < 0);
    Rectangle sr = { (float)(fr * SPR), 0.0f, flip ? -(float)SPR : (float)SPR, (float)SPR };
    Vector2  sp = { (float)(x - (SPR - PLAYER_W) / 2 + shake), (float)(y - (SPR - PLAYER_H)) };
    DrawTextureRec(texFloppy, sr, sp, WHITE);

    /* 파동이 가까우면 붉게 물든다. 스프라이트에는 표정이 없어서(§4) 이 경고가
     * 사라지면 "곧 죽는다"를 알 방법이 화면 왼쪽 파동뿐이 된다. */
    if (scared && !w->atTerminal)
        DrawTextureRec(texFloppy, sr, sp, Fade((Color){ 255, 90, 90, 255 }, 0.35f));
    if (p->has[AB_JUMP2] && p->airJumps > 0 && !p->onGround)
        DrawRectangle(x + PLAYER_W / 2 - 1, y - 3, 2, 2, AB_COLOR[AB_JUMP2]);

    /* 점프패드가 터졌을 때 — 발밑에서 퍼지는 링. 튕겼다는 게 보여야 한다 */
    if (w->padFlash > 0.0f) {
        float k = 1.0f - (w->padFlash / 0.25f);
        int rw = (int)(6 + k * 26), ry = y + PLAYER_H + (int)(k * 10);
        DrawRectangle(x + PLAYER_W / 2 - rw / 2, ry, rw, 2, Fade(C_PAD, 1.0f - k));
        DrawRectangle(x + PLAYER_W / 2 - rw / 3, ry + 4, rw * 2 / 3, 1, Fade(C_PAD, 0.6f - k * 0.6f));
    }
    }   /* deathTimer else */

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

        /* 멈춰 있는 동안은 그렇다고 말해준다. 죽고 되살아난 직후의 유예(formatGrace)와
         * 터미널 정지는 **플레이어가 쓸 수 있는 시간**인데, 안 알려주면 그냥 운으로 보인다. */
        if (w->atTerminal || w->formatGrace > 0.0f) {
            const char *msg = w->atTerminal ? "HALTED" : "RESUMING";
            DrawRectangle(fx + 4, VIEW_H / 2 - 8, 2, 16, Fade(C_TERM, 0.5f));
            txt(msg, fx + 9, VIEW_H / 2 - 4, Fade(C_TERM, 0.85f));
        }
    }
}

/* ── HUD ────────────────────────────────────────────────────────────── */
static void draw_hud(const World *w, const char *toast, float toastTimer)
{
    const Player *p = &w->p;
    const Level *L = sim_level(w);
    int freeSlots = sim_free_slots(w);
    char buf[96], num[32];

    /* ★ 44px = **두 줄.** 글자가 10px 에서 16px 이 되면서 세 줄이면 60px 이 된다 —
     *   3.75 타일이라 s08·s17 같은 세로 굴뚝에서 지형을 덮는다. 그래서 셋째 줄
     *   (조작 안내)을 없애고 둘째 줄의 빈 자리로 접었다. 화면 위쪽은 대개 비어 있어서
     *   바를 위에 두는 이유는 그대로다 — 가시는 바닥에 깔린다.
     *
     *     4..20   섹터·용량 (좌)   ·   기록 (우)
     *    24..40   용량칸 · 디스크 · [R] · 쓰기보호 (우)  */
    DrawRectangle(0, 0, VIEW_W, 44, (Color){ 8, 10, 18, 215 });
    commas((long)freeSlots * BYTES_PER_SLOT, num, sizeof(num));
    snprintf(buf, sizeof(buf), "%s   %s bytes free", L->sector, num);
    txt(buf, 6, 4, freeSlots ? C_TEXT : C_DIM);

    int cell = 0;
    for (int a = 0; a < AB_COUNT; a++)
        if (p->has[a])
            for (int k = 0; k < ABILITY_COST[a]; k++, cell++)
                DrawRectangle(6 + cell * 11, 27, 9, 9, AB_COLOR[a]);
    /* 들고 있는 디스크가 먹는 칸 (마지막 스테이지) */
    if (w->hasDisk && L->diskCost > 0)
        for (int k = 0; k < L->diskCost && cell < CAP_SLOTS; k++, cell++)
            DrawRectangle(6 + cell * 11, 27, 9, 9, C_GOAL);
    for (; cell < CAP_SLOTS; cell++)
        DrawRectangle(6 + cell * 11, 27, 9, 9, (Color){ 22, 26, 40, 255 });
    for (int i = 0; i < CAP_SLOTS; i++)
        frame_rect(6 + i * 11, 27, 9, 9, (Color){ 52, 60, 84, 255 });

    snprintf(buf, sizeof(buf), F_HUD_TIME, w->time, w->deaths);
    txt(buf, VIEW_W - txtw(buf) - 6, 4, C_DIM);

    /* ── 둘째 줄 오른쪽 ──
     *
     * ★ **자리를 고정값으로 박으면 안 된다.** 언어마다 글자 폭이 다르다 —
     *   `필수`(32px)일 때는 괜찮던 `[R]` 안내 자리를 `REQUIRED`(64px)가 침범했다.
     *   왼쪽부터 쌓아 올리고, 쓰기 보호 라벨에 닿으면 안내를 접는다.
     *
     *     [용량칸 x12] [디스크] [필수]  [R] ...            [WRITE PROTECT ...] */
    const char *lock = w->atTerminal ? "WRITE PROTECT DISABLED" : "WRITE PROTECTED";
    Color lc = w->atTerminal ? C_TERM : (Color){ 150, 110, 110, 255 };
    int lockX = VIEW_W - txtw(lock) - 6;
    txt(lock, lockX, 24, lc);

    int cur = 6 + CAP_SLOTS * 11 + 12;
    /* 이 섹터의 디스크를 챙겼는지 — 출구 앞에서 되돌아갈지 판단하는 정보다. */
    if (L->diskCol >= 0) {
        /* ★ 글자가 아니라 **그 물건 자체**로 보여준다. `DISK [x]` 는 인게임에서
         *   줍는 그림과 아무 관계가 없어서, 한 번 더 번역해야 읽힌다.
         *   챙긴 것은 원색, 아직인 것은 어두운 실루엣 — 명도로 가른다. */
        draw_frame(texDisk, 0, TILE, TILE, cur, 24,
                   w->hasDisk ? WHITE : (Color){ 58, 68, 92, 255 });
        cur += TILE + 4;
        /* 보상 스테이지는 디스크가 **필수**다. 선택인 줄 알고 지나쳤다가 출구에서
         * 막히면 파동을 등지고 되돌아가야 해서 사실상 리셋이다 — 그건 글자로 못박는다. */
        if (!w->hasDisk && L->reward) {
            txt(S_HUD_REQUIRED, cur, 24, C_SPIKE);
            cur += txtw(S_HUD_REQUIRED) + 10;
        }
    }
    /* 남는 자리에만. 재시작 키는 **있으면 좋은 것**이지 없으면 안 되는 게 아니다 —
     * 디스크가 필수인 스테이지에서 터미널에 서면 자리가 없고, 그때는 접는 게 맞다. */
    if (cur + txtw(S_HUD_KEYS) + 8 <= lockX)
        txt(S_HUD_KEYS, cur, 24, (Color){ 62, 70, 94, 255 });

    if (w->diskFlash > 0.0f) {
        snprintf(buf, sizeof(buf), F_HUD_GOT, L->file);
        center(buf, w->atTerminal ? 224 : 50, Fade(C_GOAL, w->diskFlash));
    }

    /* 로드아웃 패널 — **터미널 위에서만** 그린다.
     *
     * 상시로 띄우면 화면 좌상단을 계속 먹는다. 레벨이 위쪽에서 시작하면
     * 플레이어와 지형이 패널 뒤에 깔린다 (s04 에서 실제로 그랬다).
     * 장착 상태는 상단 용량 칸의 색으로 이미 보이므로 밖에서는 필요 없다. */
    int px = 6, pw = 200;   /* 16px 글자에 맞춰 170 -> 200 */
    /* ★ 열은 **좌표로** 맞춘다. raylib 기본 폰트는 가변폭이라 "%-10s" 로 칸을 채워도
     *   글자 폭이 제각각이라 숫자 열이 들쭉날쭉해진다 (실제로 그랬다). */
    int colName = px + 34;                 /* 파일명 왼쪽 정렬 (16px 기준 26 -> 34, 번호와 2px 밖에 안 떨어졌었다) */
    int colSize = px + pw - 7;             /* 크기  오른쪽 정렬 기준 */
    int colTag  = px + pw - 26;            /* absent 오른쪽 정렬 기준 */
    if (w->atTerminal) {
        int nUnlocked = 0;
        for (int a = 0; a < AB_COUNT; a++) if (sim_ability_unlocked(w, a)) nUnlocked++;
        int py = 52, ph = nUnlocked * 18 + 50;   /* 상단 바가 44px 이라 그 아래로 */
        DrawRectangle(px, py, pw, ph, (Color){ 8, 10, 18, 228 });
        frame_rect(px, py, pw, ph, Fade(C_TERM, 0.7f));
        txt("UTILITIES", px + 5, py + 4, C_TERM);
        /* 열 이름 — 도스 DIR 이 파일명과 크기를 찍던 그대로. 이 화면의 모든 글자가
         * 시스템 출력이라(세계관 §4) 여기도 영문 시스템 용어로 간다. */
        txt("FILE", colName, py + 24, C_DIM);
        txt("SIZE", colSize - txtw("SIZE"), py + 24, C_DIM);
        DrawRectangle(px + 5, py + 42, pw - 10, 1, Fade(C_TERM, 0.30f));
        /* ★ 아직 손에 넣은 적 없는 유틸리티는 목록에 아예 안 뜬다.
         *   존재를 모르는 게 맞다 — 회수한 디스크에서 하나씩 나온다. */
        int row = 0;
        for (int a = 0; a < AB_COUNT; a++) {
            if (!sim_ability_unlocked(w, a)) continue;
            int ly = py + 48 + row * 18;
            row++;
            int on = p->has[a];
            int here = sim_ability_available(w, a);
            Color c = on ? AB_COLOR[a] : (here ? C_DIM : (Color){ 38, 44, 62, 255 });
            /* 아이콘은 8x8 한 장씩(icons.png). **그림이 이미 능력 색을 갖고 있으므로
             * 물들이지 않는다** — 장착 안 한 것만 밝기로 눌러 둔다 (색조는 그대로) */
            draw_frame(texIcons, a, 8, 8, px + 5, ly + 4, on ? WHITE : Fade(WHITE, 0.40f));
            snprintf(buf, sizeof(buf), "%d", a + 1);
            txt(buf, px + 18, ly, c);
            txt(ABILITY_FILE[a], colName, ly, c);
            snprintf(buf, sizeof(buf), "%d", ABILITY_COST[a]);
            txt(buf, colSize - txtw(buf), ly, c);
            /* 이 섹터에 없는 유틸리티는 따로 표기한다. 같은 좌표에 겹쳐 그리면 둘 다 못 읽는다 */
            if (!here && !on)
                txt(S_HUD_ABSENT, colTag - txtw(S_HUD_ABSENT), ly,
                         (Color){ 96, 62, 70, 255 });
        }
    }

    /* 사망 연출 — 원인별로 다르게. 무엇에 죽었는지 읽혀야 다시 안 죽는다.
     * 연출이 끝나도 화면은 유지된다 — 되살아나는 타이밍은 사람이 정한다. */
    if (w->deathCause != DEATH_NONE) {
        float prog = w->deathTimer / w->deathTimerMax;   /* 1 → 0 */
        int   ready = (w->deathTimer <= 0.0f);           /* 키 입력 대기 중 */
        const char *l1, *l2;
        Color tint;
        switch (w->deathCause) {
            case DEATH_SPIKE:  tint = C_SPIKE; l1 = S_DEATH_SPIKE1;  l2 = S_DEATH_SPIKE2; break;
            case DEATH_FORMAT: tint = C_FMT;   l1 = S_DEATH_FMT1;     l2 = S_DEATH_FMT2;    break;
            default:           tint = C_DIM;   l1 = S_DEATH_FALL1;            l2 = S_DEATH_FALL2;      break;
        }
        /* 들어올 때 진하게 → 대기 중에는 옅게 깔아둔다 */
        float tintA = 0.16f + prog * 0.20f;
        DrawRectangle(0, 0, VIEW_W, VIEW_H, Fade(tint, tintA));
        for (int i = 0; i < VIEW_H; i += 3)
            if (((i / 3 + (int)(GetTime() * 40)) & 3) == 0)
                DrawRectangle(0, i, VIEW_W, 1, Fade(tint, 0.10f + prog * 0.25f));
        centerb(l1, VIEW_H / 2 - 34, tint);
        center(l2, VIEW_H / 2 + 6, C_TEXT);
        if (ready)
            center_blink(S_DEATH_ANYKEY, VIEW_H / 2 + 32, C_DIM,
                         ((int)(GetTime() * 2) % 2) == 0);
    }

    /* ★ 스쳐 지나가는 문구 셋(회수·정지·거절)은 **로드아웃 패널을 피한다.**
     *   셋 다 터미널에 선 순간에 뜰 수 있는데 패널이 좌상단 192px 까지 차지한다 —
     *   화면 가운데에 놓으면 매번 겹친다 (실제로 겹쳤다). 패널이 떠 있으면 그 아래로 내린다. */
    int flashY = w->atTerminal ? 202 : 56;
    if (w->checkFlash > 0.0f)
        center(S_HUD_HALTED, flashY, Fade(C_TERM, w->checkFlash));
    if (w->denyFlash > 0.0f) {
        /* 사유마다 플레이어가 할 일이 다르다. 한 문장으로 뭉뚱그리면
         * 디스크를 못 담는 사람에게 "터미널에서만"이라고 답하게 된다. */
        char dbuf[80];
        const char *d;
        switch (w->denyCause) {
            case DENY_ABSENT:      d = S_DENY_ABSENT;      break;
            case DENY_NO_SLOTS:    d = S_DENY_NOSPACE;                break;
            case DENY_EXIT_LOCKED: d = S_DENY_LOCKED;  break;
            case DENY_DISK_SLOTS:
                /* 몇 칸이 필요한지 안 말하면 알 방법이 없다 — s20 은 이게 스테이지의 요구다 */
                snprintf(dbuf, sizeof(dbuf), F_DENY_SLOTS,
                         L->file, L->diskCost);
                d = dbuf;
                break;
            default:               d = S_DENY_TERMINAL;          break;
        }
        float a = w->denyFlash * 2.0f;
        center(d, flashY + 22, Fade((Color){ 255, 120, 90, 255 }, a > 1.0f ? 1.0f : a));
    }
    if (toastTimer > 0.0f)
        txt(toast, px + pw + 10, 52, Fade(C_GOAL, toastTimer > 1.0f ? 1.0f : toastTimer));
}

/* ── 볼륨 오버레이 ───────────────────────────────────────────────────
 *
 * 볼륨은 화면 상태와 무관하게 어디서든 바꿀 수 있어서, HUD(=플레이 화면)가 아니라
 * 모든 화면 위에 마지막으로 덧그린다. 건드린 뒤 잠깐만 보였다 사라진다.
 * 숫자만 띄우면 "지금 몇인지"는 알아도 "얼마나 남았는지"가 안 보여서 막대를 같이 그린다. */
static void draw_volume(float timer)
{
    if (timer <= 0.0f) return;
    float a = (timer > 1.0f) ? 1.0f : timer;

    /* ★ 아래에 뭐가 깔려 있든 읽혀야 하니 **불투명 판**을 먼저 깐다.
     *   메뉴 하단(범례·조작 안내)과 정확히 겹치는 자리라, 반투명으로 두면 둘 다 안 읽힌다. */
    /* 16px 글자에 맞춰 다시 쟀다: 이름(24) + 막대(104) + 여백 + "100%%"(32) */
    const int pw = 212, ph = 48;
    int px = (VIEW_W - pw) / 2, py = VIEW_H - 56;
    DrawRectangle(px, py, pw, ph, Fade(C_BG, a));
    frame_rect(px, py, pw, ph, Fade(C_DIM, a * 0.7f));

    const int bw = 104, bh = 6;
    const char *name[2] = { "BGM", "SFX" };
    float      vol[2]   = { bgm_volume(), sfx_volume() };
    int        live[2]  = { bgm_enabled(), 1 };   /* BGM 은 M 으로 따로 끌 수 있다 */

    for (int i = 0; i < 2; i++) {
        int yy = py + 10 + i * 20;
        int bx = px + 40;
        int fill = (int)(vol[i] * (float)(bw - 2) + 0.5f);
        Color c = (live[i] && vol[i] > 0.0f) ? C_GOAL : C_OFF;

        txt(name[i], px + 8, yy - 5, Fade(C_DIM, a));
        frame_rect(bx, yy, bw, bh, Fade(C_DIM, a));
        if (fill > 0) DrawRectangle(bx + 1, yy + 1, fill, bh - 2, Fade(c, a));

        char buf[16];
        if (live[i]) snprintf(buf, sizeof(buf), "%d%%", (int)(vol[i] * 100.0f + 0.5f));
        else         snprintf(buf, sizeof(buf), "off");
        txt(buf, bx + bw + 6, yy - 5, Fade(live[i] ? C_TEXT : C_DIM, a));
    }
}

/* ── 화면들 ─────────────────────────────────────────────────────────── */
/* 폰트 크레딧 — **SIL OFL 1.1 이 요구하는 고지다.**
 *
 * ★ 단일 exe 라 옆에 라이선스 파일을 둘 수 없다. 화면 어딘가에 있어야 하고,
 *   그 자리는 **아무나 첫 화면에서 보는 곳**이어야 한다. 그래서 타이틀 최상단이다.
 *
 * ★ 여기만 raylib 기본 10px 폰트를 쓴다. 게임 글자(neoDGM)는 16px 이 최소인데
 *   그 절반인 8px 은 **정수배가 아니라 픽셀이 깨진다** (0.5배는 줄을 건너뛴다).
 *   기본 폰트는 원래 10px 짜리 비트맵이라 제 크기로 찍으면 선명하고,
 *   본문의 약 60% 라 "부속 표기"로 읽힌다.
 *
 * ★ 예약 폰트 이름이라 **`NeoDunggeunmo` 를 그대로 적는다.** 줄이거나 바꾸면 안 된다.
 *   원본 둥근모꼴(김중태)은 퍼블릭 도메인이라 별도 고지 의무가 없다. */
static void draw_font_credit(void)
{
    const char *c = "NeoDunggeunmo (c) 2017-2021 Eunbin Jeong  -  SIL OFL 1.1";
    DrawText(c, VIEW_W / 2 - MeasureText(c, 10) / 2, 4, 10, (Color){ 40, 47, 66, 255 });
}


static void draw_title(float t)
{
    ClearBackground(C_BG);
    for (int i = 0; i < VIEW_H; i += 4)
        DrawRectangle(0, i, VIEW_W, 1, (Color){ 14, 18, 32, 255 });

    draw_font_credit();

    /* 로고 240x56 (art/logo.png). 기본 폰트로 찍던 제목과 그 테두리 상자를 대신한다 */
    DrawTexture(texLogo, (VIEW_W - 240) / 2, 64, WHITE);
    center(S_TITLE_FREE, 130, C_DIM);

    center_blink(S_TITLE_ENTER, 168, C_TEXT, ((int)(t * 2) % 2) == 0);
    /* 볼륨 키는 오버레이가 뜬 뒤에는 가려지므로, 안 눌러본 사람을 위해 여기 적어둔다 */
    center(S_TITLE_VOLKEYS, VIEW_H - 64, C_DIM);
    center(S_TITLE_TAG, VIEW_H - 38, (Color){ 60, 68, 92, 255 });
}

/* ── 스테이지 선택 ────────────────────────────────────────────────────
 *
 * ★ **파일명을 칸 안에서 뺐다 (2026-08-19).** 글자가 16px 이 되면서
 *   `AUTOEXEC.BAT` 12자 = 96px 인데 칸 폭이 88px 이라 안 들어간다.
 *   칸을 넓히려면 열을 4개로 줄여야 하고, 그러면 세로가 288px 을 넘는다.
 *
 *   그래서 칸은 **번호·기록·디스크**만 갖고, 파일명과 그 한 줄은
 *   **고른 섹터 하나만** 격자 아래에 크게 띄운다. 20개를 다 작게 늘어놓는 것보다
 *   지금 고른 것을 제대로 읽히게 하는 쪽이 정보가 많다.
 *
 *     제목        8..40    (32px)
 *     격자       50..176   5열 x 4행, 칸 88x26
 *     고른 것   184..216   파일명(32px)  ·  섹터 표기(우)
 *     한 줄     222..238
 *     범례·집계 248..264
 *     조작      268..284                                              */
#define MENU_COLS 5

static void draw_menu(const Progress *pr, int sel, float t, int confirming)
{
    char buf[64];
    ClearBackground(C_BG);
    for (int i = 0; i < VIEW_H; i += 4)
        DrawRectangle(0, i, VIEW_W, 1, (Color){ 14, 18, 32, 255 });

    centerb(S_MENU_TITLE, 8, C_GOAL);
    /* 언어 전환 — 타이틀에도 있지만 여기서도 보여야 한다. 세이브를 지우지 않는 한
     * 타이틀은 한 번 지나가면 다시 안 보는 화면이다. */

    int cw = 88, ch = 26, gx = 4, gy = 6;
    int gridW = MENU_COLS * cw + (MENU_COLS - 1) * gx;
    int x0 = (VIEW_W - gridW) / 2, y0 = 50;
    /* ★ 하단 두 줄은 격자 x0(=28) 이 아니라 **화면 여백 12** 를 쓴다.
     *   x0 로 맞췄더니 범례가 198px 까지 가고 집계가 196px 에서 시작해 2px 겹쳤다.
     *   12 로 내리면 범례 182 · 집계 시작 212 로 30px 이 뜬다. */
    const int bx0 = 12;

    for (int i = 0; i < LEVEL_COUNT && i < MAX_STAGES; i++) {
        int r = i / MENU_COLS, c = i % MENU_COLS;
        int x = x0 + c * (cw + gx), y = y0 + r * (ch + gy);
        int open = (i <= pr->reached);
        int done = pr->cleared[i];
        Color tc = TIER_COLOR[tier_of(i)];

        Color border = open ? (done ? tc : Fade(tc, 0.55f)) : (Color){ 34, 40, 58, 255 };
        Color fill   = (i == sel) ? Fade(tc, 0.18f) : (Color){ 13, 17, 28, 255 };
        DrawRectangle(x, y, cw, ch, fill);
        frame_rect(x, y, cw, ch, border);
        if (i == sel) {
            float pulse = 0.55f + 0.45f * (float)((int)(t * 4) % 2);
            frame_rect(x - 1, y - 1, cw + 2, ch + 2, Fade(tc, pulse));
        }

        /* 번호는 16px 로 내렸다. 32px 이면 26px 짜리 칸에 안 들어간다 */
        snprintf(buf, sizeof(buf), "%02d", i + 1);
        txt(buf, x + 6, y + 5, open ? (done ? tc : C_TEXT) : (Color){ 46, 54, 76, 255 });

        if (!open) {
            txt(S_MENU_LOCKED, x + 28, y + 5, (Color){ 46, 54, 76, 255 });
            continue;
        }
        if (done) {
            snprintf(buf, sizeof(buf), "%.1fs", pr->best[i]);
            txt(buf, x + 28, y + 5, C_TEXT);
            /* 회수 여부는 **인게임에서 줍는 그 그림**으로 보여준다.
             * 손으로 그린 사각형이면 목록의 표식과 실제 물건이 따로 놀아서,
             * 무엇을 놓쳤는지 알려면 한 번 더 번역해야 한다.
             * 밝기로 가른다 — 챙긴 것은 원색, 안 챙긴 것은 어두운 실루엣. */
            draw_frame(texDisk, 0, TILE, TILE, x + cw - 20, y + 5,
                       pr->diskGot[i] ? WHITE : (Color){ 60, 70, 96, 255 });
        } else {
            txt(S_MENU_NEW, x + 28, y + 5, tc);
        }
    }

    /* 고른 섹터 하나 — 칸에서 뺀 파일명이 여기로 온다 */
    if (sel >= 0 && sel < LEVEL_COUNT) {
        int open = (sel <= pr->reached);
        Color tc = TIER_COLOR[tier_of(sel)];
        if (open) {
            txtb(LEVELS[sel].file, bx0, 184, tc);
            txt(LEVELS[sel].meta, bx0, 222, C_DIM);
        } else {
            txtb(S_MENU_LOCKED, bx0, 184, (Color){ 46, 54, 76, 255 });
        }
        txt(LEVELS[sel].sector, VIEW_W - txtw(LEVELS[sel].sector) - bx0, 194,
            (Color){ 58, 68, 92, 255 });
    }

    /* 티어 범례 + 집계 */
    int ly = 248;
    int lx = bx0;
    for (int k = 0; k < 3; k++) {
        DrawRectangle(lx, ly + 5, 6, 6, TIER_COLOR[k]);
        txt(TIER_NAME[k], lx + 10, ly, C_DIM);
        lx += 12 + txtw(TIER_NAME[k]) + 12;
    }
    snprintf(buf, sizeof(buf), F_MENU_STATS,
             progress_total_cleared(pr, LEVEL_COUNT), LEVEL_COUNT,
             progress_total_disks(pr, LEVEL_COUNT), pr->deaths);
    txt(buf, VIEW_W - txtw(buf) - bx0, ly, C_DIM);

    center(S_MENU_KEYS, VIEW_H - 20, C_DIM);

    /* ── 초기화 확인 ──
     *
     * ★ 되돌릴 수 없는 동작이라 **한 번 더 묻는다.** 그리고 무엇이 지워지는지
     *   나열한다 — "정말입니까?"만 띄우면 사람은 무엇을 잃는지 모르는 채로 누른다.
     *   확인 키를 DEL 로 둔 건, 실수로 두 번 눌릴 만한 자리가 아니어서다. */
    if (confirming) {
        /* 32px 제목 "기록을 모두 지운다" 가 288px 이라 320 으로는 여백이 안 남는다 */
        int bw = 352, bh = 118;
        int bx = (VIEW_W - bw) / 2, by = (VIEW_H - bh) / 2;
        DrawRectangle(0, 0, VIEW_W, VIEW_H, Fade(C_BG, 0.84f));
        DrawRectangle(bx, by, bw, bh, (Color){ 14, 8, 12, 246 });
        frame_rect(bx, by, bw, bh, C_FMT);
        centerb(S_ERASE_TITLE, by + 8,  C_FMT);
        center(S_ERASE_WHAT,   by + 48, C_TEXT);
        center(S_ERASE_WARN,   by + 68, (Color){ 150, 110, 110, 255 });
        center_blink(S_ERASE_KEYS, by + 90, C_DIM,
                     ((int)(t * 3) % 2) == 0);
    }
}

static void draw_clear(const World *w, unsigned gained)
{
    const Level *L = sim_level(w);
    char buf[96], num[32];
    int hadDisk = (L->diskCol >= 0);

    /* 거의 불투명하게. 205 였을 때 뒤의 지형과 힌트가 비쳐서 글자가 안 읽혔다 */
    DrawRectangle(0, 0, VIEW_W, VIEW_H, (Color){ 6, 12, 14, 248 });
    /* 세로 자리 —  제목 36..68  ·  디스크 76..108  ·  본문 118~ (16px 폰트 기준)
     * 제목이 32px 이라 36 에 두면 68 까지 차지한다. 셋을 겹치지 않게 떼어 놓는다 —
     * 예전에 제목과 디스크를 같은 y 에 얹었다가 통째로 겹친 적이 있다. */
    centerb(S_CLEAR_TITLE, 36, C_EXIT);

    /* 회수했는지 아닌지를 **그림 하나로** 먼저 보여준다. 글자를 읽기 전에 안다.
     * 2배로 키운다 — 정수배라 픽셀이 안 깨진다 (아트설계.md §1). */
    if (hadDisk) {
        Rectangle src = { 0, 0, (float)TILE, (float)TILE };
        Rectangle dst = { (float)(VIEW_W / 2 - TILE), 76.0f,
                          (float)(TILE * 2), (float)(TILE * 2) };
        Vector2   org = { 0, 0 };
        DrawTexturePro(texDisk, src, dst, org, 0.0f,
                       w->hasDisk ? WHITE : (Color){ 64, 74, 100, 255 });
    }

    int y = hadDisk ? 118 : 84;
    if (hadDisk && w->hasDisk) {
        commas(L->fileBytes, num, sizeof(num));
        snprintf(buf, sizeof(buf), F_CLEAR_GOT, L->file, num);
        center(buf, y, C_GOAL);
        center(L->meta, y + 20, C_DIM);
        y += 46;
        for (int a = 0; a < AB_COUNT; a++) {
            if (!(gained & AB_BIT(a))) continue;
            center(S_CLEAR_HELD, y, C_DIM);
            snprintf(buf, sizeof(buf), F_CLEAR_ABILITY, a + 1, ABILITY_FILE[a], ABILITY_COST[a]);
            center(buf, y + 20, AB_COLOR[a]);
            y += 42;
        }
    } else if (hadDisk) {
        /* 출구만 통과했다. 디스크는 선택이지만, 그 안에 유틸리티가 있었다면 알려준다. */
        snprintf(buf, sizeof(buf), F_CLEAR_LEFT, L->file);
        center(buf, y, (Color){ 220, 130, 110, 255 });
        if (L->reward)
            center(S_CLEAR_MISSED, y + 20, C_DIM);
        center(S_CLEAR_BACK, y + 44, C_TEXT);
        y += 68;
    } else {
        center(S_CLEAR_NODISK, y, C_DIM);
        y += 28;
    }

    snprintf(buf, sizeof(buf), F_CLEAR_TIME, w->time, w->deaths);
    center(buf, y + 6, C_DIM);
    center(S_CLEAR_NEXT, VIEW_H - 46, C_TEXT);
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
    /* 엔딩 일러스트 512x288 (art/ending.png) — 화면을 그대로 채운다.
     *
     * ★ 게임 내내 화면은 디스크 **안**이었다. 여기서 처음으로 밖에서 본 장면이 나오고,
     *   그게 이 한 장이 하는 일의 전부다. 글자는 그 위에 얹는다 —
     *   패널을 깔면 애써 그린 그림을 가린다. 대신 힌트와 같은 1px 그림자로 띄운다. */
    DrawTexture(texEnding, 0, 0, WHITE);

    /* 글자는 그림이 비어 있는 **왼쪽 기둥**에만 넣는다. 가운데는 기계가 차지한다.
     * 모니터 왼쪽 모서리가 x=225 쯤이라 **x=24 에서 폭 176px 까지**가 안전선이다.
     * 16px 폰트로 재보면 가장 긴 줄이 "실어낸 1,474,560 bytes" = 176px 이다. */
    txtb("A:\\> _", 25, 41, (Color){ 6, 8, 14, 210 });
    txtb("A:\\> _", 24, 40, C_GOAL);

    snprintf(buf, sizeof(buf), F_END_FILES,
             progress_total_disks(pr, LEVEL_COUNT), LEVEL_COUNT);
    text_shadow(buf, 24, 88, C_TEXT);
    commas(bytes, num, sizeof(num));
    /* ★ "written" 이 아니라 "carried out" 이다. 회수한 파일 크기의 **합**이라
     *   20개를 다 모으면 1,474,560 을 넘는데, 그 숫자는 이 게임의 전제다.
     *   한 번에 한 파일씩 실어 나른 것이므로 누적은 용량을 넘어도 맞다 —
     *   말이 그렇게 읽히게 바꾼다. */
    snprintf(buf, sizeof(buf), F_END_BYTES, num);
    text_shadow(buf, 24, 108, C_TEXT);
    /* 한 줄로 붙이면 기계 그림까지 뻗는다 — 두 줄로 끊는다 */
    snprintf(buf, sizeof(buf), F_END_TIME, total);
    text_shadow(buf, 24, 128, C_DIM);
    snprintf(buf, sizeof(buf), F_END_RETRY, pr->deaths);
    text_shadow(buf, 24, 148, C_DIM);
    text_shadow(S_END_EJECTED, 24, 176, C_DIM);
    text_shadow(S_END_BACK, 24, VIEW_H - 24, C_DIM);
    (void)w;
}

/* ── main ───────────────────────────────────────────────────────────── */
int main(int argc, char **argv)
{
    int shotMode = (argc > 1 && strcmp(argv[1], "--shot") == 0);
    int shotMenu = (argc > 1 && strcmp(argv[1], "--shotmenu") == 0);
    /* 타이틀·클리어도 찍을 수 있어야 한다. 글자 크기를 바꾸면 자리가 전부 밀리는데,
     * 눈으로 못 보면 넘쳤는지 겹쳤는지 알 방법이 없다 (한국어 작업에서 실제로 그랬다). */
    int shotTitle = (argc > 1 && strcmp(argv[1], "--shottitle") == 0);
    int shotClear = (argc > 1 && strcmp(argv[1], "--shotclear") == 0);
    /* --shotmenu erase : 초기화 확인창까지 띄운 채로 찍는다 */
    int shotErase = 0;
    for (int i = 2; i < argc; i++) if (strcmp(argv[i], "erase") == 0) shotErase = 1;
    int shotDeath = (argc > 1 && strcmp(argv[1], "--shotdeath") == 0);
    int shotEnd   = (argc > 1 && strcmp(argv[1], "--shotend") == 0);
    int shotTile = (argc > 2) ? atoi(argv[2]) : -1;
    int shotLvl  = (argc > 3) ? atoi(argv[3]) : 0;
    int shotAt   = (argc > 4) ? atoi(argv[4]) : 30;   /* 몇 프레임째를 찍을지 */

    Tune tune;
    tune_defaults(&tune);
    char toast[128] = { 0 };
    float toastTimer = 0.0f;
    float volTimer = 0.0f;   /* 볼륨 오버레이가 남아 있는 시간 */
    int   volDirty = 0;      /* 아직 저장 안 된 볼륨 변경이 있다 */
    load_tune(&tune, "tune.txt", toast, sizeof(toast));

    Progress prog;
    progress_reset(&prog, AB_STARTING);
    progress_load(&prog, SAVE_PATH);

    World world;
    sim_new_game(&world, &tune);
    world.unlocked = prog.unlocked;
    if ((shotMode || shotClear || shotDeath) && shotLvl > 0) {
        /* 스크린샷용: 그 레벨까지의 보상을 미리 지급한다 */
        for (int i = 0; i < shotLvl && i < LEVEL_COUNT; i++) world.unlocked |= LEVELS[i].reward;
        sim_load(&world, &tune, shotLvl);
    }

    Screen screen = (shotMode || shotClear || shotDeath) ? SCR_PLAY
                  : shotEnd  ? SCR_END
                  : shotMenu ? SCR_MENU : SCR_TITLE;
    int  menuSel = prog.reached;
    int  confirmErase = 0;   /* 기록 초기화 확인창이 떠 있는가 */
    unsigned gained = 0;
    /* 엔딩은 **마지막 한 칸이 채워지는 그 순간**에만 뜬다.
     *
     * ★ 예전 조건은 "20개 다 클리어했나" 하나뿐이었다. 그건 한 번 참이 되면 영영 참이라,
     *   엔딩을 본 뒤에 s01 을 다시 깨도 엔딩이 또 나왔다. 클리어할 때마다
     *   **이번 클리어로 처음 다 찼는가**를 보고 여기 적어둔다. */
    int endingDue = 0;
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

    sfx_init();
    bgm_init();   /* 오디오 장치를 sfx 와 공유한다. 순서가 바뀌면 안 된다 */
    /* 세이브에 남은 볼륨을 적용한다. 세이브가 없으면 progress_reset 이 넣은 30% */
    bgm_set_volume(prog.bgmVol);
    sfx_set_volume(prog.sfxVol);

    RenderTexture2D canvas = LoadRenderTexture(VIEW_W, VIEW_H);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);

    /* 아트는 파일이 아니라 exe 안의 png 바이트다. 옆에 png 를 두면 지워질 수 있고
     * 옆에 둬도 규정 위반은 아니지만 그 파일도 용량에 들어간다 (아트설계.md §6). */
    fnt = load_font16();
    texFloppy     = load_png(FLOPPY_PNG,      (int)sizeof(FLOPPY_PNG));
    texGroundTop  = load_png(GROUND_TOP_PNG,  (int)sizeof(GROUND_TOP_PNG));
    texGroundFill = load_png(GROUND_FILL_PNG, (int)sizeof(GROUND_FILL_PNG));
    texOneway     = load_png(ONEWAY_PNG,      (int)sizeof(ONEWAY_PNG));
    texSpike      = load_png(SPIKE_PNG,       (int)sizeof(SPIKE_PNG));
    texPad        = load_png(PAD_PNG,         (int)sizeof(PAD_PNG));
    texExit       = load_png(EXIT_PNG,        (int)sizeof(EXIT_PNG));
    texDisk       = load_png(DISK_PNG,        (int)sizeof(DISK_PNG));
    texTerminal   = load_png(TERMINAL_PNG,    (int)sizeof(TERMINAL_PNG));
    texIcons      = load_png(ICONS_PNG,       (int)sizeof(ICONS_PNG));
    texBgTile     = load_png(BG_TILE_PNG,     (int)sizeof(BG_TILE_PNG));
    texLogo       = load_png(LOGO_PNG,        (int)sizeof(LOGO_PNG));
    texEnding     = load_png(ENDING_PNG,      (int)sizeof(ENDING_PNG));
    texCrumble    = load_png(CRUMBLE_PNG,     (int)sizeof(CRUMBLE_PNG));

    if (shotErase) confirmErase = 1;
    if (shotClear) { screen = SCR_CLEAR; world.hasDisk = 1; world.time = 27.6f; world.deaths = 3; }
    if (shotDeath) { world.deathCause = DEATH_FORMAT; world.deathTimer = 0.0f; world.deathTimerMax = 1.0f; }
    if (shotMode && shotTile >= 0) {
        world.p.x = shotTile * (float)TILE;
        world.formatX = world.p.x - 520.0f;
    }

    int frames = 0;
    while (!WindowShouldClose() && !quit) {
        float dt = 1.0f / 60.0f;
        /* ── 전체화면 ── Alt+Enter. 도스·윈도우에서 30년째 같은 키다. 안내를 안 적어도 된다.
         *
         * ★ **경계 없는 전체화면**을 쓴다. 진짜 전체화면은 모니터 모드를 바꿔서
         *   전환할 때 화면이 한 번 꺼지고, 알트탭이 느려진다. 512x288 을 정수배로
         *   키우는 게임이라 모드를 바꿔서 얻을 게 없다.
         *
         * ★ Enter 는 메뉴 확정 키이기도 하다. Alt 를 누른 채면 **확정으로 안 친다** —
         *   안 그러면 전체화면으로 바뀌면서 스테이지가 같이 시작된다. */
        int altHeld = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
        int enter = !altHeld && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER));
        if (altHeld && IsKeyPressed(KEY_ENTER)) {
            ToggleBorderlessWindowed();
            /* 창으로 돌아올 때 크기가 풀린다 — 처음 정한 정수 배율로 되돌린다 */
            if (!IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) {
                SetWindowSize(VIEW_W * scale, VIEW_H * scale);
                SetWindowPosition(60, 60);
            }
        }

        bgm_update();   /* 스트림 버퍼가 비면 다음 구간을 합성해 넣는다 */

        if (IsKeyPressed(KEY_F5)) { load_tune(&tune, "tune.txt", toast, sizeof(toast)); toastTimer = 2.5f; }


        /* ── 볼륨 ── BGM 과 SFX 를 따로 잡는다. 음악만 줄이고 효과음은 남기는 게
         *   플랫포머에서는 흔한 요구다 — 착지·대시 소리는 조작 피드백이라서.
         *   화면과 무관하게 어디서든 먹힌다 (타이틀·메뉴·플레이 전부). */
        {
            int touched = 0, preview = 0;
            if (IsKeyPressed(KEY_LEFT_BRACKET))  { bgm_set_volume(bgm_volume() - VOL_STEP); touched = 1; }
            if (IsKeyPressed(KEY_RIGHT_BRACKET)) { bgm_set_volume(bgm_volume() + VOL_STEP); touched = 1; }
            if (IsKeyPressed(KEY_MINUS))         { sfx_set_volume(sfx_volume() - VOL_STEP); touched = preview = 1; }
            if (IsKeyPressed(KEY_EQUAL))         { sfx_set_volume(sfx_volume() + VOL_STEP); touched = preview = 1; }
            if (IsKeyPressed(KEY_M))             { bgm_toggle(); touched = 1; }
            if (touched) {
                /* SFX 는 소리를 내줘야 지금 크기를 판단할 수 있다. BGM 은 이미 흐르고 있다 */
                if (preview) sfx_play(SFX_EQUIP);
                prog.bgmVol = bgm_volume();
                prog.sfxVol = sfx_volume();
                volTimer = 2.0f;
                volDirty = 1;
            }
        }
        if (volTimer > 0.0f) {
            volTimer -= dt;
            /* 한 칸 누를 때마다 파일을 쓰면 낭비다. 손을 뗀 뒤 한 번만 쓴다 */
            if (volTimer <= 0.0f && volDirty) {
                if (!shotMenu) progress_save(&prog, SAVE_PATH);   /* shotMenu 의 prog 는 가짜다 */
                volDirty = 0;
            }
        }

        if (toastTimer > 0) toastTimer -= dt;

        int esc = IsKeyPressed(KEY_ESCAPE);

        if (screen == SCR_TITLE) {
            if (esc) quit = 1;
            if (enter) { menuSel = prog.reached; screen = SCR_MENU; }
        } else if (screen == SCR_MENU && confirmErase) {
            /* ── 초기화 확인 중 ── 다른 입력은 전부 막는다.
             * 특히 ENTER 가 그대로 살아 있으면, 확인창을 못 본 사람이 스테이지를
             * 시작하려다 누른 키가 다음 프레임에 지우기로 이어질 수 있다. */
            if (IsKeyPressed(KEY_DELETE)) {
                /* 볼륨은 진행이 아니라 설정이다 — 기록만 지우고 설정은 남긴다 */
                float bv = prog.bgmVol, sv = prog.sfxVol;
                progress_reset(&prog, AB_STARTING);
                prog.bgmVol = bv; prog.sfxVol = sv;
                if (!shotMenu) progress_save(&prog, SAVE_PATH);

                world.unlocked = prog.unlocked;
                menuSel = 0;
                confirmErase = 0;
                snprintf(toast, sizeof(toast), S_TOAST_ERASED);
                toastTimer = 2.5f;
            } else if (esc || enter) {
                confirmErase = 0;
            }
        } else if (screen == SCR_MENU) {
            if (esc) screen = SCR_TITLE;
            if (IsKeyPressed(KEY_DELETE)) confirmErase = 1;
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
                screen = endingDue ? SCR_END : SCR_MENU;
                endingDue = 0;
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
            /* 사망 후 리스폰용 — 아무 키나. GetKeyPressed 는 IsKeyPressed 와
             * 다른 큐를 쓰므로 위 입력들과 간섭하지 않는다 */
            in.anyKey = (GetKeyPressed() != 0);
            if (IsKeyPressed(KEY_R)) sim_load(&world, &tune, world.levelIndex);

            sim_step(&world, &tune, &in, dt);
            /* 사건 하나에 소리 하나. sim 이 이벤트로 알려주므로 렌더/사운드가
             * 게임 규칙을 다시 판정할 필요가 없다 */
            if (world.evPad)         sfx_play(SFX_PAD);
            else if (world.evJump)   sfx_play(SFX_JUMP);
            if (world.evDoubleJump)  sfx_play(SFX_JUMP2);
            if (world.evWallJump)    sfx_play(SFX_WALL);
            if (world.evDash)        sfx_play(SFX_DASH);
            if (world.evLand && !world.evPad) sfx_play(SFX_LAND);
            if (world.evDisk)        sfx_play(SFX_DISK);
            if (world.evEquip)       sfx_play(SFX_EQUIP);
            if (world.evDeny)        sfx_play(SFX_DENY);
            /* 무너지기 시작한 순간에 소리가 나야 "밟은 것이 원인"이라고 읽힌다.
             * 실제로 꺼지는 건 0.35초 뒤라, 그때 울리면 인과가 안 붙는다. */
            if (world.evCrumble)     sfx_play(SFX_WALL);
            if (world.evCheckpoint)  sfx_play(SFX_CHECK);
            if (world.evDeath)
                sfx_play(world.deathCause == DEATH_SPIKE ? SFX_HURT : SFX_ERASE);

            if (world.state == ST_WON) {
                sfx_play(SFX_EXIT);
                gained = sim_grant_reward(&world);   /* 디스크 안에 들어 있던 유틸리티 */
                prog.unlocked = world.unlocked;
                prog.deaths  += world.deaths;
                int wasAll = (progress_total_cleared(&prog, LEVEL_COUNT) >= LEVEL_COUNT);
                progress_record(&prog, world.levelIndex, world.hasDisk, world.time, LEVEL_COUNT);
                endingDue = !wasAll
                         && (progress_total_cleared(&prog, LEVEL_COUNT) >= LEVEL_COUNT);
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
            else if (screen == SCR_MENU) draw_menu(&prog, menuSel, (float)GetTime(), confirmErase);
            else if (screen == SCR_END)  draw_end(&world, &prog);
            else {
                draw_world(&world, &tune, (float)GetTime(), camX);
                /* 죽었거나 클리어 화면일 때는 힌트를 끈다.
                 * 오버레이보다 먼저 그려지므로 반투명 뒤로 비쳐서 둘 다 안 읽힌다. */
                if (world.deathCause == DEATH_NONE && screen != SCR_CLEAR)
                    draw_hints(&world, camX);
                draw_hud(&world, toast, toastTimer);
                if (screen == SCR_CLEAR) draw_clear(&world, gained);
            }
            /* 메뉴·타이틀에는 HUD 가 없어서 토스트를 여기서 띄운다.
             * 초기화하고 나면 화면이 통째로 잠긴 상태로 바뀌긴 하지만,
             * "내가 방금 뭘 했는지"는 글자로 한 번 확인시켜 주는 게 맞다. */
            if (toastTimer > 0.0f && (screen == SCR_MENU || screen == SCR_TITLE))
                center(toast, VIEW_H - 33,
                       Fade(C_GOAL, toastTimer > 1.0f ? 1.0f : toastTimer));
            draw_volume(volTimer);   /* 화면 종류와 무관하게 맨 위에 */
        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK);
            /* ★ **정수 배율로만 확대하고 남는 자리는 검게 둔다.**
             *   창 모드에서는 창 크기가 이미 512x288 의 정수배라 딱 맞는다.
             *   전체화면에서는 안 맞는다 — 1920x1080 은 288 의 3.75배라
             *   그대로 늘이면 어떤 픽셀은 4px, 어떤 픽셀은 3px 이 되어 **글자가 울퉁불퉁해진다.**
             *   3배(1536x864)로 두고 위아래에 검은 띠를 남기는 쪽이 맞다 (`아트설계.md §1`).
             *
             *   화면이 정확히 16:9 라 가로세로 비율은 어차피 안 틀어진다 —
             *   문제는 비율이 아니라 **픽셀 크기가 고르지 않은 것**이다. */
            int sw = GetScreenWidth(), sh = GetScreenHeight();
            int k = sw / VIEW_W;
            if (sh / VIEW_H < k) k = sh / VIEW_H;
            if (k < 1) k = 1;
            int dw = VIEW_W * k, dh = VIEW_H * k;
            Rectangle src = { 0, 0, (float)VIEW_W, -(float)VIEW_H };
            Rectangle dst = { (float)((sw - dw) / 2), (float)((sh - dh) / 2),
                              (float)dw, (float)dh };
            DrawTexturePro(canvas.texture, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
        EndDrawing();

        if ((shotMode || shotMenu || shotTitle || shotClear || shotDeath || shotEnd) && ++frames >= shotAt) { TakeScreenshot("shot.png"); break; }
    }

    /* 볼륨만 바꾸고 바로 껐다면 아직 안 써졌다 */
    if (volDirty && !shotMenu) progress_save(&prog, SAVE_PATH);

    bgm_shutdown();
    sfx_shutdown();
    UnloadTexture(texFloppy);     UnloadTexture(texGroundTop);  UnloadTexture(texGroundFill);
    UnloadTexture(texOneway);     UnloadTexture(texSpike);      UnloadTexture(texPad);
    UnloadTexture(texExit);       UnloadTexture(texDisk);       UnloadTexture(texTerminal);
    UnloadTexture(texIcons);      UnloadTexture(texBgTile);     UnloadTexture(texLogo);
    UnloadTexture(texEnding);     UnloadTexture(texCrumble);
    UnloadFont(fnt);
    UnloadRenderTexture(canvas);
    CloseWindow();
    return 0;
}
