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

/* ══════════════════════════════════════════════════════════════════════
 *  06 — DIARY.TXT   [중간]   보상: 없음
 *
 *  grip 도입. 그리고 **여기서 처음 12칸을 넘긴다** (boot3+dash4+glide4+grip4 = 15칸).
 *  뭘 두고 갈지 고르게 되는 첫 스테이지다.
 *
 *  굴뚝(양쪽 벽 사이 3칸)으로 만든다. 벽 하나짜리 등반은 벽점프로 튕겨 나갔다가
 *  다시 붙어야 해서 손맛이 나쁘다 — 마주 보는 벽이 있으면 지그재그로 올라간다.
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L06_BOX[] = {
    {  0,  0, 56,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 55,  0,  1, 18, T_SOLID },
    {  1, 15, 20,  3, T_SOLID },      /* 바닥 A */
    { 13,  4,  2, 10, T_SOLID },      /* 굴뚝 왼벽 (rows 4~13) */
    { 18,  4,  2, 10, T_SOLID },      /* 굴뚝 오른벽 — 사이 x15~17 이 통로 */
    { 20,  4, 10,  2, T_SOLID },      /* 굴뚝 위 발판 (surface row4) */
    { 21, 17, 12,  1, T_SPIKE },
    { 33, 15, 22,  3, T_SOLID },      /* 바닥 B — 위에서 떨어져 내려온다 */
};
static const TermSpec L06_TERM[] = { { 3, 14, 0 }, { 22, 3, 0 }, { 46, 14, 0 } };
static const HintSpec L06_HINT[] = {
    {  3, 12, 0,       "15 SLOTS OF UTILITIES", "you have 12" },
    { 16, 12, "4",     "grip.exe",              "jump at a wall and hold toward it" },
    { 16,  2, 0,       "ZIGZAG UP",             0 },
    { 27,  2, 0,       "THEN DROP",             0 },
    { 50, 12, 0,       "DIARY.TXT",             "entries stop in march" },
    { 53, 13, 0,       "DRIVE SLOT",            0 },
};

/* ══════════════════════════════════════════════════════════════════════
 *  07 — MAZE.DAT   [중간]   보상: 없음
 *
 *  구간마다 **서로 다른 능력으로 가는 두 갈래 길**.
 *    구간1  9칸 장벽      : grip 직등 / boot 로 3칸 발판 2개
 *    구간2  6칸 구덩이 x2 : dash
 *    구간3  저천장 통로   : dash·glide  /  위층 9칸 : grip
 *
 *  ★ 불변식: 단독으로 전 구간을 여는 능력이 없어야 한다.
 *    구간3 저천장(도달 2.2칸)이 유일한 jump2 차단 수단이다 — 나중에 jump2 를 얻어도 유지된다.
 *  ★ 위층 아랫면은 row12 여야 맨몸 점프(38px)로 벽면에 손이 닿는다.
 *    row10 까지만 내려오면 grip 루트가 통째로 막힌다.
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L07_BOX[] = {
    {  0,  0, 96,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 95,  0,  1, 18, T_SOLID },
    {  1, 15, 31,  3, T_SOLID },
    { 13, 12,  2,  1, T_SOLID },      /* 발판 (3칸) */
    { 16,  9,  2,  1, T_SOLID },      /* 발판 (3칸) */
    { 19,  6,  3,  9, T_SOLID },      /* 장벽 — 꼭대기는 바닥에서 9칸 */
    { 32, 15,  7,  3, T_SOLID },
    { 39, 17,  6,  1, T_SPIKE },
    { 45, 15,  6,  3, T_SOLID },
    { 51, 17,  6,  1, T_SPIKE },
    { 57, 15,  5,  3, T_SOLID },
    { 62,  6, 29,  7, T_SOLID },      /* 위층 x62~90, rows 6~12 */
    { 62, 15,  6,  3, T_SOLID },      /* 통로 바닥 — 머리 위 여유 2칸 */
    { 72, 17, 23,  1, T_SOLID },
    { 72, 14, 19,  1, T_SOLID },      /* 낮은 천장 */
};
static const TermSpec L07_TERM[] = { { 2, 14, 0 }, { 29, 14, 0 }, { 58, 14, 0 } };
static const HintSpec L07_HINT[] = {
    {  2, 12, 0,       "TWO WAYS PAST EACH PART", "pick what you brought" },
    { 17,  4, 0,       "grip.exe OR boot.exe",    0 },
    { 33, 12, "SHIFT", "dash.exe",                "flat gaps" },
    { 58, 12, 0,       "LOW CEILING BELOW",       "or climb over the top" },
    { 90, 13, 0,       "MAZE.DAT",                "someone mapped this by hand" },
};

/* ══════════════════════════════════════════════════════════════════════
 *  08 — THEME.MOD   [중간]   보상: jump2
 *
 *  세로 등반. 앞은 3칸 지그재그(boot), 마지막은 12칸 민벽(grip 전용).
 *  jump2 를 여기서 주는 이유: 이 다음부터 20칸이 되어 8칸을 버려야 한다.
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L08_BOX[] = {
    {  0,  0, 60,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 59,  0,  1, 18, T_SOLID },
    {  1, 15, 16,  3, T_SOLID },
    {  6, 12,  3,  1, T_SOLID },      /* 3칸 */
    { 11,  9,  3,  1, T_SOLID },      /* 3칸 */
    {  6,  6,  3,  1, T_SOLID },      /* 3칸 — 되돌아간다 */
    { 11,  3,  3,  1, T_SOLID },      /* 3칸 */
    { 14,  3, 10,  2, T_SOLID },      /* 위 발판 (surface row3) */
    { 17, 17,  7,  1, T_SPIKE },
    { 24, 15, 20,  3, T_SOLID },      /* 바닥 B */
    { 44,  3,  2, 15, T_SOLID },      /* 12칸 민벽 — grip 전용 */
    { 46,  3, 10,  2, T_SOLID },      /* 꼭대기 */
};
static const TermSpec L08_TERM[] = { { 3, 14, 0 }, { 20, 2, 0 }, { 40, 14, 0 } };
static const HintSpec L08_HINT[] = {
    {  3, 12, 0,       "STRAIGHT UP",     0 },
    {  8, 10, "1",     "boot.exe",        "3 tiles at a time" },
    { 20,  1, 0,       "NOW DROP RIGHT",  0 },
    { 43, 12, "4",     "grip.exe",        "12 tiles, no ledges" },
    { 52,  1, 0,       "THEME.MOD",       "his favourite" },
};

/* ══════════════════════════════════════════════════════════════════════
 *  09 — SAVE02.DAT   [중간]   보상: 없음
 *
 *  여기서부터 20칸이다. jump2(5)+dash(4)+boot(3) = 정확히 12칸.
 *  4칸 턱(64px)은 boot(55px)로 안 되고 jump2(70px)로만 된다 — 두 능력을 다 요구한다.
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L09_BOX[] = {
    {  0,  0, 68,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 67,  0,  1, 18, T_SOLID },
    {  1, 15, 12,  3, T_SOLID },
    { 13, 11, 12,  7, T_SOLID },      /* 4칸 턱 (surface row11) — jump2 */
    { 25, 17,  6,  1, T_SPIKE },      /* 6칸 평지 구덩이 — dash */
    { 31, 11, 10,  7, T_SOLID },
    { 41,  7, 10, 11, T_SOLID },      /* 4칸 턱 (surface row7) — jump2 */
    { 51, 17,  6,  1, T_SPIKE },      /* 6칸 — dash */
    { 57,  7, 10, 11, T_SOLID },
};
static const TermSpec L09_TERM[] = { { 3, 14, 0 }, { 20, 10, 0 }, { 36, 10, 0 }, { 60, 6, 0 } };
static const HintSpec L09_HINT[] = {
    {  3, 12, 0,       "20 SLOTS NOW",  "12 fit" },
    { 13,  9, "5",     "jump2.exe",     "boot alone is 3 tiles" },
    { 25,  9, "SHIFT", "dash.exe",      "and you still need this" },
    { 63,  5, 0,       "SAVE02.DAT",    "a later save, same game" },
};

/* ══════════════════════════════════════════════════════════════════════
 *  10 — README.1ST   [중간]   보상: 없음
 *
 *  속도. 파동 48px/s 에 터미널을 둘만 둔다. 한 번 커밋하면 오래 달려야 한다.
 *  관문 자체는 쉽되(6칸 구덩이) **연속**이라 멈추면 잡힌다.
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L10_BOX[] = {
    {  0,  0, 72,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 71,  0,  1, 18, T_SOLID },
    {  1, 15, 14,  3, T_SOLID },
    { 15, 17,  6,  1, T_SPIKE },
    { 21, 15, 12,  3, T_SOLID },
    { 33, 17,  6,  1, T_SPIKE },
    { 39, 15, 12,  3, T_SOLID },
    { 51, 17,  6,  1, T_SPIKE },
    { 57, 15, 14,  3, T_SOLID },
    { 27, 14,  2,  1, T_SPIKE },
    { 41, 14,  2,  1, T_SPIKE },      /* 터미널(x45)과 겹치지 않게 떨어뜨린다 */
};
static const TermSpec L10_TERM[] = { { 3, 14, 0 }, { 45, 14, 0 } };
static const HintSpec L10_HINT[] = {
    {  3, 12, 0,       "THE FORMAT IS FASTER HERE", "do not stop" },
    { 15, 12, 0,       "THREE GAPS",               "no terminal between" },
    { 66, 12, 0,       "README.1ST",               "read me first" },
    { 69, 13, 0,       "DRIVE SLOT",               0 },
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

    { "s06", "SECTOR 06", "DIARY.TXT", "entries stop in march", 8192,
      L06_BOX,  (int)(sizeof(L06_BOX) /sizeof(L06_BOX[0])),
      L06_TERM, (int)(sizeof(L06_TERM)/sizeof(L06_TERM[0])),
      L06_HINT, (int)(sizeof(L06_HINT)/sizeof(L06_HINT[0])),
      0, AB(AB_GRIP),
      56, 2, 15,  53, 14,  50, 14,  32.0f },

    /* needs = 0 : 구간마다 길이 갈려서 **어느 한 능력도 필수가 아니다.**
     * (다만 구간2 평지 구덩이는 dash 가 사실상 유일한 답이다 — 아래 needs 에 넣어둔다) */
    { "s07", "SECTOR 07", "MAZE.DAT", "someone mapped this by hand", 12288,
      L07_BOX,  (int)(sizeof(L07_BOX) /sizeof(L07_BOX[0])),
      L07_TERM, (int)(sizeof(L07_TERM)/sizeof(L07_TERM[0])),
      L07_HINT, (int)(sizeof(L07_HINT)/sizeof(L07_HINT[0])),
      0, AB(AB_DASH),
      96, 2, 15,  93, 16,  90, 16,  34.0f },

    { "s08", "SECTOR 08", "THEME.MOD", "his favourite", 57344,
      L08_BOX,  (int)(sizeof(L08_BOX) /sizeof(L08_BOX[0])),
      L08_TERM, (int)(sizeof(L08_TERM)/sizeof(L08_TERM[0])),
      L08_HINT, (int)(sizeof(L08_HINT)/sizeof(L08_HINT[0])),
      AB(AB_JUMP2), AB(AB_BOOT) | AB(AB_GRIP),
      60, 2, 15,  54, 2,  51, 2,  34.0f },

    { "s09", "SECTOR 09", "SAVE02.DAT", "a later save, same game", 81920,
      L09_BOX,  (int)(sizeof(L09_BOX) /sizeof(L09_BOX[0])),
      L09_TERM, (int)(sizeof(L09_TERM)/sizeof(L09_TERM[0])),
      L09_HINT, (int)(sizeof(L09_HINT)/sizeof(L09_HINT[0])),
      0, AB(AB_JUMP2) | AB(AB_DASH),
      68, 2, 15,  65, 6,  62, 6,  36.0f },

    { "s10", "SECTOR 10", "README.1ST", "read me first", 2048,
      L10_BOX,  (int)(sizeof(L10_BOX) /sizeof(L10_BOX[0])),
      L10_TERM, (int)(sizeof(L10_TERM)/sizeof(L10_TERM[0])),
      L10_HINT, (int)(sizeof(L10_HINT)/sizeof(L10_HINT[0])),
      0, AB(AB_DASH),
      72, 2, 15,  69, 14,  66, 14,  48.0f },
};

const int LEVEL_COUNT = (int)(sizeof(LEVELS) / sizeof(LEVELS[0]));
