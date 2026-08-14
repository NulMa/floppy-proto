#include "levels.h"
#include "sim.h"

#define AB(x) (1u << (x))

/* ══════════════════════════════════════════════════════════════════════
 *  도달 범위 참고 (tune.txt 기본값 기준, test.bat 이 매번 출력한다)
 *
 *    높이   맨몸 2.4칸  boot 3.4칸  jump2 4.4칸  grip 제한없음
 *    거리   맨몸 4.7칸  boot 5.6칸  dash 7.0칸  jump2 8.9칸
 *    저천장 2칸 통로 안에서의 거리  2.2칸
 *
 *    활공은 **내려갈 때만** 값을 한다. 낙하속도 60px/s 이므로
 *    D px 를 내려가는 동안 가로로 2.13 * D px 를 간다.
 *      3칸(48px) 하강 → 6.4칸 도달
 *      5칸(80px) 하강 → 10.6칸 도달   (같은 조건에서 dash 는 8.8칸)
 * ══════════════════════════════════════════════════════════════════════ */

/* ══════════════════════════════════════════════════════════════════════
 *  01 — AUTOEXEC.BAT   [쉬움]   보상: dash
 *
 *  가르치는 것 넷: 이동·점프 / 포맷 파동 / 터미널 / 능력으로 통과.
 *  쓸 수 있는 능력이 boot 하나뿐이라 "고르는 법"이 아니라 "쓰는 법"만 가르친다.
 *  두 번째 터미널을 턱 바로 앞에 둔다 — 못 넘고 파동을 거슬러 되돌아가면 첫 경험이 최악이 된다.
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L01_BOX[] = {
    {  0,  0, 46,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 45,  0,  1, 18, T_SOLID },
    {  1, 15, 14,  3, T_SOLID },      /* 바닥 A */
    { 10, 14,  2,  1, T_SOLID },      /* 1칸 계단 — 맨몸으로 넘어진다 */
    { 15, 17,  2,  1, T_SPIKE },      /* 2칸 구덩이 바닥 */
    { 17, 15, 28,  3, T_SOLID },      /* 바닥 B */
    { 19, 12,  4,  6, T_SOLID },      /* 3칸 턱 (48px) — boot 필요 */
    { 28, 14,  2,  1, T_SPIKE },      /* 지면 가시 — 뛰어넘는다 */
    { 38, 15,  1,  1, T_PAD },        /* 점프패드 — 디스크가 그 위에 있다 */
};
static const TermSpec L01_TERM[] = { { 5, 14, 0 }, { 17, 14, 0 } };
static const HintSpec L01_HINT[] = {
    {  2, 13, "< >",   "MOVE",              0 },
    {  5,  9, 0,       "TERMINAL",          "the format stops here" },
    { 10, 13, "SPACE", "JUMP",              "hold longer to go higher" },
    { 13,  8, 0,       "FORMAT IS COMING",  "keep moving right" },
    { 19, 11, "1",     "LOAD boot.exe",     "the ledge needs it" },
    { 28, 10, 0,       "SPIKES",            "jump over them" },
    { 34, 13, 0,       "JUMP PAD",          "it throws you up" },
    { 38,  6, 0,       "AUTOEXEC.BAT",      "optional - it holds a utility" },
    { 42, 12, 0,       "DRIVE SLOT",        "this is the way out" },
};

/* ══════════════════════════════════════════════════════════════════════
 *  02 — CONFIG.SYS   [쉬움]   보상: 없음
 *
 *  dash 를 쓰게 만든다. 6칸 구덩이(96px)는 boot(89px)로 안 되고 dash(113px)로만 된다.
 *  턱은 그대로 boot. 두 능력을 번갈아 쓰는 감각만 붙이면 된다.
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L02_BOX[] = {
    {  0,  0, 60,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 59,  0,  1, 18, T_SOLID },
    {  1, 15, 25,  3, T_SOLID },
    { 12, 12,  4,  6, T_SOLID },      /* 3칸 턱 — boot */
    { 26, 17,  6,  1, T_SPIKE },      /* 6칸 구덩이 — dash */
    { 32, 15, 10,  3, T_SOLID },
    { 37, 14,  2,  1, T_SPIKE },
    { 42, 17,  6,  1, T_SPIKE },      /* 6칸 구덩이 — dash */
    { 48, 15, 11,  3, T_SOLID },
};
static const TermSpec L02_TERM[] = { { 3, 14, 0 }, { 34, 14, 0 } };
static const HintSpec L02_HINT[] = {
    {  3, 12, 0,       "TERMINAL",       "dash.exe is yours now" },
    { 13, 10, "1",     "boot.exe",       "gets you up" },
    { 26, 12, "SHIFT", "dash.exe",       "gets you across" },
    { 37, 11, 0,       "SPIKES",         0 },
    { 54, 11, 0,       "CONFIG.SYS",     0 },
    { 57, 12, 0,       "DRIVE SLOT",     0 },
};

/* ══════════════════════════════════════════════════════════════════════
 *  03 — LETTER.TXT   [쉬움]   보상: glide
 *
 *  저천장 2칸 통로. 머리가 천장에 닿아 상승이 잘리므로 도달거리가 2.2칸으로 떨어진다.
 *  4칸(64px) 구덩이는 점프로 절대 못 건넌다 — dash 전용 구간이다.
 *  나중에 jump2 를 얻어도 여기는 안 통한다는 걸 미리 심어둔다.
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L03_BOX[] = {
    {  0,  0, 64,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 63,  0,  1, 18, T_SOLID },
    {  1, 15, 13,  3, T_SOLID },
    { 14,  0, 40, 13, T_SOLID },      /* 통로 천장 (rows 0~12) → 여유 2칸 */
    { 14, 15,  8,  3, T_SOLID },
    { 22, 17,  4,  1, T_SPIKE },
    { 26, 15,  8,  3, T_SOLID },
    { 34, 17,  4,  1, T_SPIKE },
    { 38, 15,  8,  3, T_SOLID },
    { 46, 17,  4,  1, T_SPIKE },
    { 50, 15, 13,  3, T_SOLID },
};
static const TermSpec L03_TERM[] = { { 3, 14, 0 }, { 30, 14, 0 } };
static const HintSpec L03_HINT[] = {
    {  3, 12, 0,       "TERMINAL",         0 },
    { 12, 12, 0,       "LOW CEILING AHEAD", "a jump reaches 2 tiles there" },
    { 22, 11, "SHIFT", "dash.exe",         "the only way over" },
    { 44, 11, 0,       "KEEP GOING",       0 },
    { 58, 12, 0,       "LETTER.TXT",       "never sent" },
    { 61, 13, 0,       "DRIVE SLOT",       0 },
};

/* ══════════════════════════════════════════════════════════════════════
 *  04 — SONG.MOD   [쉬움]   보상: 없음
 *
 *  활공의 집. 9칸 구덩이를 5칸 내려가며 건넌다.
 *    glide 10.6칸 ✓ / dash 8.8칸 ✗ / 맨몸+점프 6.4칸 ✗
 *
 *  ★ 굴뚝을 세로로 파면 가로 파동이 안 쫓아와서 압박이 사라진다.
 *    그래서 **대각선으로 내려가게** 배치했다 (D-37 안 ②).
 *    파동 코드를 안 건드리고 레벨로 해결한다.
 * ══════════════════════════════════════════════════════════════════════ */
/* ★ 시작 표면을 row4 아래로 둔다. row2 에서 시작하면 플레이어가
 *   상단 HUD 바(0~30px = row0~1) 뒤에 깔려서 안 보인다. */
static const Box L04_BOX[] = {
    {  0,  0, 72,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 71,  0,  1, 18, T_SOLID },
    {  1,  4,  9,  2, T_SOLID },      /* 표면 row4 */
    { 10, 17,  9,  1, T_SPIKE },
    { 19,  9,  9,  2, T_SOLID },      /* 표면 row9  (5칸 하강, 9칸 구덩이) */
    { 28, 17,  9,  1, T_SPIKE },
    { 37, 14,  9,  2, T_SOLID },      /* 표면 row14 (5칸 하강, 9칸 구덩이) */
    { 46, 17,  5,  1, T_SPIKE },
    { 51, 17, 20,  1, T_SOLID },      /* 표면 row17 (3칸 하강, 5칸 — 여긴 쉽게) */
};
static const TermSpec L04_TERM[] = { { 3, 3, 0 }, { 21, 8, 0 }, { 39, 13, 0 } };
static const HintSpec L04_HINT[] = {
    {  4,  3, 0,       "A LONG WAY DOWN",   0 },
    { 10,  3, "3",     "glide.exe",         "hold jump while falling" },
    { 21,  7, 0,       "DASH IS TOO SHORT", "you have to fall slowly" },
    { 39, 12, 0,       "ONE MORE",          0 },
    { 65, 15, 0,       "SONG.MOD",          "4 channels, 3:12" },
    { 68, 16, 0,       "DRIVE SLOT",        0 },
};

/* ══════════════════════════════════════════════════════════════════════
 *  05 — PHOTO.PCX   [쉬움]   보상: grip
 *
 *  dash 구간과 glide 구간을 한 스테이지에 붙였다. 둘 다 들고 다녀야 한다.
 *    (boot 3 + dash 4 + glide 4 = 11칸 이라 아직 12칸에 다 들어간다.
 *     "무엇을 두고 갈까"는 grip 을 받는 다음 스테이지부터 시작된다)
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L05_BOX[] = {
    {  0,  0, 76,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 75,  0,  1, 18, T_SOLID },
    {  1,  8, 12,  2, T_SOLID },      /* 표면 row8 */
    { 13, 17,  6,  1, T_SPIKE },      /* 6칸 평지 구덩이 — dash */
    { 19,  8, 10,  2, T_SOLID },
    { 29, 17,  6,  1, T_SPIKE },      /* 6칸 — dash */
    { 35,  8, 10,  2, T_SOLID },
    { 45, 17,  9,  1, T_SPIKE },      /* 9칸 + 5칸 하강 — glide */
    { 54, 13, 20,  2, T_SOLID },      /* 표면 row13 */
};
static const TermSpec L05_TERM[] = { { 3, 7, 0 }, { 40, 7, 0 }, { 58, 12, 0 } };
static const HintSpec L05_HINT[] = {
    {  3,  6, 0,       "BOTH TOOLS TODAY", 0 },
    { 13,  6, "SHIFT", "dash.exe",         "flat gaps" },
    { 45,  6, "3",     "glide.exe",        "this one drops" },
    { 68, 11, 0,       "PHOTO.PCX",        "scanned from paper" },
    { 71, 12, 0,       "DRIVE SLOT",       0 },
};

/* ══════════════════════════════════════════════════════════════════════ */

/* 해금 곡선 — 능력 합계가 12칸을 넘는 순간부터 "무엇을 두고 갈까"가 시작된다.
 *
 *   01 진입  boot                3칸
 *   01 클리어 +dash              7칸
 *   03 클리어 +glide            11칸
 *   05 클리어 +grip             15칸   ← 06(중간 난이도)부터 조인다
 *   08 클리어 +jump2            20칸
 */
const Level LEVELS[] = {
    { "s01", "SECTOR 01", "AUTOEXEC.BAT", "the file this machine read first", 512,
      L01_BOX,  (int)(sizeof(L01_BOX) /sizeof(L01_BOX[0])),
      L01_TERM, (int)(sizeof(L01_TERM)/sizeof(L01_TERM[0])),
      L01_HINT, (int)(sizeof(L01_HINT)/sizeof(L01_HINT[0])),
      AB(AB_DASH), AB(AB_BOOT),
      46, 2, 15,  42, 14,  38, 10,  22.0f },

    { "s02", "SECTOR 02", "CONFIG.SYS", "twelve lines that ran everything", 1024,
      L02_BOX,  (int)(sizeof(L02_BOX) /sizeof(L02_BOX[0])),
      L02_TERM, (int)(sizeof(L02_TERM)/sizeof(L02_TERM[0])),
      L02_HINT, (int)(sizeof(L02_HINT)/sizeof(L02_HINT[0])),
      0, AB(AB_BOOT) | AB(AB_DASH),
      60, 2, 15,  57, 14,  54, 14,  26.0f },

    { "s03", "SECTOR 03", "LETTER.TXT", "never sent", 4096,
      L03_BOX,  (int)(sizeof(L03_BOX) /sizeof(L03_BOX[0])),
      L03_TERM, (int)(sizeof(L03_TERM)/sizeof(L03_TERM[0])),
      L03_HINT, (int)(sizeof(L03_HINT)/sizeof(L03_HINT[0])),
      AB(AB_GLIDE), AB(AB_DASH),
      64, 2, 15,  61, 14,  58, 14,  28.0f },

    { "s04", "SECTOR 04", "SONG.MOD", "4 channels, 3:12", 61440,
      L04_BOX,  (int)(sizeof(L04_BOX) /sizeof(L04_BOX[0])),
      L04_TERM, (int)(sizeof(L04_TERM)/sizeof(L04_TERM[0])),
      L04_HINT, (int)(sizeof(L04_HINT)/sizeof(L04_HINT[0])),
      0, AB(AB_GLIDE),
      72, 2, 4,  68, 16,  65, 16,  30.0f },

    { "s05", "SECTOR 05", "PHOTO.PCX", "scanned from paper", 24576,
      L05_BOX,  (int)(sizeof(L05_BOX) /sizeof(L05_BOX[0])),
      L05_TERM, (int)(sizeof(L05_TERM)/sizeof(L05_TERM[0])),
      L05_HINT, (int)(sizeof(L05_HINT)/sizeof(L05_HINT[0])),
      AB(AB_GRIP), AB(AB_DASH) | AB(AB_GLIDE),
      76, 2, 8,  71, 12,  68, 12,  30.0f },
};

const int LEVEL_COUNT = (int)(sizeof(LEVELS) / sizeof(LEVELS[0]));
