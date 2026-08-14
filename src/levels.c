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
    { 19, 11, "1",     "LOAD boot.exe",     "the ledge needs it",  AB(AB_BOOT) },
    { 28, 10, 0,       "SPIKES",            "jump over them" },
    { 34, 13, 0,       "JUMP PAD",          "it throws you up" },
    { 38,  6, 0,       "AUTOEXEC.BAT",      "optional - it holds a utility" },
    { 42, 12, 0,       "DRIVE SLOT",        "this is the way out" },
};

/* ══════════════════════════════════════════════════════════════════════
 *  02 — CONFIG.SYS   [쉬움]   보상: 없음
 *
 *  dash 를 쓰게 만든다.
 *
 *  ★ 원래 6칸(96px)이었다. **boot 로도 건너졌다** — 코요테 타임(0.09초 = 12px)을
 *    계산에 안 넣어서 boot 를 89px 로 봤는데 실제로는 101px 이다.
 *    8칸(128px)으로 넓혔다: boot 101 ✗ / dash 163 ✓.
 *  턱은 그대로 boot. 두 능력을 번갈아 쓰는 감각만 붙이면 된다.
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L02_BOX[] = {
    {  0,  0, 72,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 71,  0,  1, 18, T_SOLID },
    {  1, 15, 22,  3, T_SOLID },
    { 11, 12,  4,  6, T_SOLID },      /* 3칸 턱 — boot */
    { 23, 17,  8,  1, T_SPIKE },      /* 8칸 구덩이 — dash */
    { 31, 15,  9,  3, T_SOLID },
    { 35, 14,  2,  1, T_SPIKE },
    { 40, 17,  8,  1, T_SPIKE },      /* 8칸 구덩이 — dash */
    { 48, 15,  6,  3, T_SOLID },
    /* 轉: 마지막 착지가 2칸 높다. 7칸이라 대시 단독(131px)으로도 되지만
     * boot 를 겹치면(158px) 훨씬 편하다 — boot 단독은 77px 라 절대 안 된다 */
    { 54, 17,  7,  1, T_SPIKE },
    { 61, 13, 10,  5, T_SOLID },
};
static const TermSpec L02_TERM[] = { { 3, 14, 0 }, { 32, 14, 0 }, { 49, 14, 0 } };
static const HintSpec L02_HINT[] = {
    {  3, 12, 0,       "TERMINAL",       "dash.exe is yours now" },
    { 12, 10, "1",     "boot.exe",       "gets you up",           AB(AB_BOOT) },
    { 23, 12, "SHIFT", "dash.exe",       "gets you across",       AB(AB_DASH) },
    { 35, 11, 0,       "SPIKES",         0 },
    { 56, 11, 0,       "THIS ONE LANDS HIGH", "jump and dash together" },
    { 65,  9, 0,       "CONFIG.SYS",     0 },
    { 68, 10, 0,       "DRIVE SLOT",     0 },
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
    /* 轉: 마지막은 한 번에 못 건넌다. 중간에 얇은 발판을 밟고 대시를 두 번 —
     * 거리가 아니라 **쿨다운 타이밍**이 문제가 된다 */
    { 46, 17,  3,  1, T_SPIKE },
    { 49, 15,  2,  1, T_ONEWAY },
    { 51, 17,  3,  1, T_SPIKE },
    { 54, 15,  9,  3, T_SOLID },
};
static const TermSpec L03_TERM[] = { { 3, 14, 0 }, { 30, 14, 0 } };
static const HintSpec L03_HINT[] = {
    {  3, 12, 0,       "TERMINAL",         0 },
    { 12, 12, 0,       "LOW CEILING AHEAD", "a jump reaches 2 tiles there" },
    { 22, 11, "SHIFT", "dash.exe",         "the only way over", AB(AB_DASH) },
    { 44, 11, 0,       "KEEP GOING",       0 },
    { 58, 12, 0,       "LETTER.TXT",       "never sent" },
    { 61, 13, 0,       "DRIVE SLOT",       0 },
};

/* ══════════════════════════════════════════════════════════════════════
 *  04 — SONG.MOD   [쉬움]   보상: 없음
 *
 *  활공의 집. 16칸 구덩이를 5칸 내려가며 건넌다.
 *    활공 18.8칸 ✓ / boot+dash 13.5칸 ✗ / boot 8.0칸 ✗
 *
 *  ★ 원래 9칸이었고 **점프 후 대시로 그냥 건너졌다.** 사람이 알려줘서 잡혔다.
 *    대시를 "조금 더 먼 점프"로 봤던 게 틀렸다 — 대시는 중력을 멈추고,
 *    낙하 도중에 쓰면 쌓인 낙하속도까지 지운다. 낙차가 클수록 이득이 커져서
 *    **활공의 자리를 그대로 침범한다.** 5칸 낙차에서 boot+dash 는 217px 이다.
 *    조작감을 건드리지 않고 구덩이를 16칸(256px)으로 벌려서 갈랐다.
 *
 *  ★ 굴뚝을 세로로 파면 가로 파동이 안 쫓아와서 압박이 사라진다.
 *    그래서 **대각선으로 내려가게** 배치했다 (D-37 안 ②).
 *    파동 코드를 안 건드리고 레벨로 해결한다.
 * ══════════════════════════════════════════════════════════════════════ */
/* ★ 시작 표면을 row4 아래로 둔다. row2 에서 시작하면 플레이어가
 *   상단 HUD 바(0~30px = row0~1) 뒤에 깔려서 안 보인다. */
static const Box L04_BOX[] = {
    {  0,  0, 86,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 85,  0,  1, 18, T_SOLID },
    {  1,  4,  9,  2, T_SOLID },      /* 표면 row4 */
    { 10, 17, 16,  1, T_SPIKE },
    { 26,  9,  9,  2, T_SOLID },      /* 표면 row9  (5칸 하강, 16칸 구덩이) */
    { 35, 17, 16,  1, T_SPIKE },
    { 51, 14,  9,  2, T_SOLID },      /* 표면 row14 (5칸 하강, 16칸 구덩이) */
    { 60, 17,  5,  1, T_SPIKE },
    /* 轉: 마지막 착지가 3칸뿐이다. 거리가 아니라 **멈추는 정확도**가 문제 */
    { 65, 17,  3,  1, T_SOLID },
    { 68, 17,  4,  1, T_SPIKE },
    { 72, 17, 13,  1, T_SOLID },
};
static const TermSpec L04_TERM[] = { { 3, 3, 0 }, { 28, 8, 0 }, { 53, 13, 0 } };
static const HintSpec L04_HINT[] = {
    {  4,  3, 0,       "A LONG WAY DOWN",   0 },
    { 10,  3, "3",     "glide.exe",         "hold jump while falling", AB(AB_GLIDE) },
    { 28,  7, 0,       "DASH IS TOO SHORT", "you have to fall slowly" },
    { 53, 12, 0,       "ONE MORE",          0 },
    { 79, 15, 0,       "SONG.MOD",          "4 channels, 3:12" },
    { 82, 16, 0,       "DRIVE SLOT",        0 },
};

/* ══════════════════════════════════════════════════════════════════════
 *  05 — PHOTO.PCX   [쉬움]   보상: grip
 *
 *  dash 구간과 glide 구간을 한 스테이지에 붙였다. 둘 다 들고 다녀야 한다.
 *    (boot 3 + dash 4 + glide 4 = 11칸 이라 아직 12칸에 다 들어간다.
 *     "무엇을 두고 갈까"는 grip 을 받는 다음 스테이지부터 시작된다)
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L05_BOX[] = {
    {  0,  0, 84,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 83,  0,  1, 18, T_SOLID },
    {  1,  8, 12,  2, T_SOLID },      /* 표면 row8 */
    { 13, 17,  8,  1, T_SPIKE },      /* 8칸 평지 구덩이 — dash (boot 는 6.3칸까지) */
    { 21,  8, 10,  2, T_SOLID },
    { 31, 17,  8,  1, T_SPIKE },      /* 8칸 — dash */
    { 39,  8, 10,  2, T_SOLID },
    { 49, 17, 16,  1, T_SPIKE },      /* 16칸 + 5칸 하강 — glide 전용 */
    { 65, 13, 18,  2, T_SOLID },      /* 표면 row13 */
};
static const TermSpec L05_TERM[] = { { 3, 7, 0 }, { 41, 7, 0 }, { 69, 12, 0 } };
static const HintSpec L05_HINT[] = {
    {  3,  6, 0,       "BOTH TOOLS TODAY", 0 },
    { 13,  6, "SHIFT", "dash.exe",         "flat gaps",      AB(AB_DASH) },
    { 49,  6, "3",     "glide.exe",        "this one drops", AB(AB_GLIDE) },
    { 76, 11, 0,       "PHOTO.PCX",        "scanned from paper" },
    { 79, 12, 0,       "DRIVE SLOT",       0 },
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
    { 16, 12, "4",     "grip.exe",              "jump at a wall and hold toward it", AB(AB_GRIP) },
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
    { 33, 12, "SHIFT", "dash.exe",                "flat gaps", AB(AB_DASH) },
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
    {  8, 10, "1",     "boot.exe",        "3 tiles at a time",   AB(AB_BOOT) },
    { 20,  1, 0,       "NOW DROP RIGHT",  0 },
    { 43, 12, "4",     "grip.exe",        "12 tiles, no ledges", AB(AB_GRIP) },
    { 52,  1, 0,       "THEME.MOD",       "his favourite" },
};

/* ══════════════════════════════════════════════════════════════════════
 *  09 — SAVE02.DAT   [중간]   보상: 없음
 *
 *  여기서부터 20칸이다. jump2(5)+dash(4)+boot(3) = 정확히 12칸.
 *
 *  ★ 원래 4칸 턱이었다. **grip 으로 그냥 올라가졌다** — 사람이 알려줘서 잡혔다.
 *
 *    벽 모양으로는 grip 을 못 막는다. 잡는 판정이 "머리가 벽면에 닿으면"이라
 *    맨몸 점프로 닿는 52px 위까지는 어떤 형태든 잡힌다. 턱을 ㄱ자로 파도
 *    그 처마의 옆면이 또 벽이 된다 — 벽을 만드는 한 언제나 열린다.
 *
 *    ★★ **원웨이 발판은 T_SOLID 가 아니라서 grip 이 붙지 않는다.**
 *      게임 안에서 grip 을 확실히 막는 유일한 지형이다.
 *      그래서 턱을 걷어내고 **구덩이 위에 뜬 원웨이 선반**으로 바꿨다:
 *        grip   붙을 벽이 없다            ✗
 *        boot   55px                     ✗
 *        dash   가로 전용, 높이를 못 준다  ✗
 *        jump2  70px                     ✗   ← 단독으로도 안 된다
 *        boot+jump2  101px               ✓
 *      선반을 5칸(80px)에 두면 **boot+jump2(8칸) + dash(4칸) = 정확히 12칸**.
 *      "20칸 중 12칸"이라는 이 스테이지의 주제가 그대로 관문이 된다.
 *
 *    구덩이를 12칸으로 벌려 건너편 벽(x25)에 못 닿게 했다 —
 *    boot+dash 로도 181px 이라 196px 을 못 넘는다. 선반이 유일한 길이다.
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L09_BOX[] = {
    {  0,  0, 76,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 75,  0,  1, 18, T_SOLID },
    {  1, 15, 12,  3, T_SOLID },      /* 바닥 A  표면 row15 */
    { 13, 17, 12,  1, T_SPIKE },      /* 12칸 구덩이 — 벽에 못 닿는 거리 */
    { 16, 10,  7,  1, T_ONEWAY },     /* ★ 뜬 선반 5칸 위 — boot+jump2 전용 */
    { 25, 11, 14,  7, T_SOLID },      /* 바닥 B  표면 row11 */
    { 39, 17,  8,  1, T_SPIKE },      /* 8칸 평지 구덩이 — dash */
    { 47, 11, 12,  7, T_SOLID },      /* 바닥 C  표면 row11 */
    { 59, 17, 12,  1, T_SPIKE },      /* 12칸 구덩이 */
    { 62,  6,  7,  1, T_ONEWAY },     /* ★ 두 번째 선반 5칸 위 */
    { 71,  7,  4, 11, T_SOLID },      /* 꼭대기 표면 row7 */
};
static const TermSpec L09_TERM[] = { { 3, 14, 0 }, { 28, 10, 0 }, { 50, 10, 0 }, { 72, 6, 0 } };
static const HintSpec L09_HINT[] = {
    {  3, 12, 0,       "20 SLOTS NOW",    "12 fit" },
    { 15,  7, "5",     "jump2.exe",       "with boot.exe - neither alone", AB(AB_JUMP2) },
    { 21, 13, 0,       "NOTHING TO GRAB", "the shelf is not a wall" },
    { 41,  9, "SHIFT", "dash.exe",        "and you still need this",      AB(AB_DASH) },
    { 72,  4, 0,       "SAVE02.DAT",      "a later save, same game" },
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
    /* 轉: 마지막 구덩이는 7칸이다. 여기선 **네가 아니라 레벨이 속도를 준다** */
    { 49, 15,  1,  1, T_PAD },
    { 51, 17,  7,  1, T_SPIKE },
    { 58, 15, 13,  3, T_SOLID },
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

/* ══════════════════════════════════════════════════════════════════════
 *  11 — NOTES.TXT   [중간]   보상: 없음
 *
 *  지면 가시 밭. 가시 폭으로 요구 능력이 갈린다.
 *    4칸(64px) 맨몸 점프(75px)로 넘어짐 / 5칸(80px) 은 boot(89px) 필요
 *
 *  ⚠️ 천장 가시를 구덩이 위에 두면 안 된다. 구덩이는 점프+대시로만 건너는데
 *    점프하면 천장 가시에 죽는다 = 아무도 못 지나간다. 가시는 지면에만.
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L11_BOX[] = {
    {  0,  0, 72,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 71,  0,  1, 18, T_SOLID },
    {  1, 15, 21,  3, T_SOLID },
    {  7, 14,  4,  1, T_SPIKE },      /* 4칸 — 맨몸으로 넘어진다 */
    { 14, 14,  5,  1, T_SPIKE },      /* 5칸 — boot 필요 */
    { 22, 17,  6,  1, T_SPIKE },      /* 6칸 구덩이 — 점프+dash */
    { 28, 15, 13,  3, T_SOLID },
    { 33, 14,  5,  1, T_SPIKE },
    { 41, 17,  6,  1, T_SPIKE },
    { 47, 15, 24,  3, T_SOLID },
    { 53, 14,  4,  1, T_SPIKE },
    { 60, 14,  5,  1, T_SPIKE },
};
static const TermSpec L11_TERM[] = { { 3, 14, 0 }, { 30, 14, 0 }, { 49, 14, 0 } };
static const HintSpec L11_HINT[] = {
    {  3, 12, 0,       "SPIKE FIELD",   "width decides what you need" },
    { 16, 11, "1",     "boot.exe",      "5 tiles is too far bare", AB(AB_BOOT) },
    { 24, 11, "SHIFT", "dash.exe",      "for the pits",            AB(AB_DASH) },
    { 66, 12, 0,       "NOTES.TXT",     "half a sentence, then nothing" },
    { 69, 13, 0,       "DRIVE SLOT",    0 },
};

/* ══════════════════════════════════════════════════════════════════════
 *  12 — TRACK02.MOD   [중간]   보상: 없음
 *
 *  점프패드 릴레이로 올라갔다가, 12칸을 활공으로 내려온다.
 *
 *  ★ 패드를 발판 **바로 아래**에 두면 밑면에 부딪힌다.
 *    한 칸 옆에 두면 포물선으로 발판 위에 올라탄다 (0.478초 상승 = 가로 3.8칸).
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L12_BOX[] = {
    {  0,  0, 72,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 71,  0,  1, 18, T_SOLID },
    {  1, 15, 12,  3, T_SOLID },
    {  9, 15,  1,  1, T_PAD },        /* 발판(x11~) 왼쪽에 둔다 */
    { 11,  9,  8,  1, T_SOLID },      /* surface row9 */
    { 16,  9,  1,  1, T_PAD },
    { 18,  3,  8,  1, T_SOLID },      /* surface row3 */
    { 26, 17, 15,  1, T_SPIKE },      /* 12칸 하강 · 15칸 구덩이 — 활공 전용 */
    { 41, 15, 30,  3, T_SOLID },
};
static const TermSpec L12_TERM[] = { { 3, 14, 0 }, { 20, 2, 0 }, { 44, 14, 0 } };
static const HintSpec L12_HINT[] = {
    {  3, 12, 0,       "JUMP PADS",   "step on, do not aim" },
    { 12,  7, 0,       "AGAIN",       0 },
    { 22,  1, "3",     "glide.exe",   "12 tiles down, 15 across", AB(AB_GLIDE) },
    { 66, 12, 0,       "TRACK02.MOD", "an unfinished loop" },
    { 69, 13, 0,       "DRIVE SLOT",  0 },
};

/* ══════════════════════════════════════════════════════════════════════
 *  13 — SPRITE.PCX   [중간]   보상: 없음
 *
 *  원웨이 발판 층. 지면은 19칸짜리 가시밭이라 못 지나간다 —
 *  아래에서 뚫고 올라가 위층으로 우회하는 게 유일한 길이다.
 *  층 간격은 3칸이라 boot 로 오른다.
 *
 *  ★ 터미널은 원웨이 위에 못 둔다 (발판 위 검사가 T_SOLID 를 본다).
 *    꼭대기 층 끝을 solid 로 만들어 거기 둔다.
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L13_BOX[] = {
    {  0,  0, 72,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 71,  0,  1, 18, T_SOLID },
    {  1, 15, 70,  3, T_SOLID },
    { 10, 14, 19,  1, T_SPIKE },      /* 지면은 통과 불가 */
    {  8, 12, 14,  1, T_ONEWAY },     /* 3칸 위 — 아래에서 뚫고 올라간다 */
    { 12,  9, 14,  1, T_ONEWAY },
    { 16,  6, 12,  1, T_ONEWAY },
    { 28,  6,  4,  1, T_SOLID },      /* 터미널 자리 + 내려가는 발판 */
};
static const TermSpec L13_TERM[] = { { 3, 14, 0 }, { 29, 5, 0 }, { 45, 14, 0 } };
static const HintSpec L13_HINT[] = {
    {  3, 12, 0,       "THE FLOOR IS OUT",  "go over it" },
    {  8, 10, 0,       "THIN PLATFORMS",    "jump up through them" },
    { 29,  3, 0,       "DROP OFF THE END",  0 },
    { 66, 12, 0,       "SPRITE.PCX",        "16 by 16, four colours" },
    { 69, 13, 0,       "DRIVE SLOT",        0 },
};

/* ══════════════════════════════════════════════════════════════════════
 *  14 — BACKUP.ZIP   [중간]   보상: 없음
 *
 *  길다. 터미널이 둘뿐이라 한 번 커밋하면 오래 달린다.
 *  마지막에 저천장 통로를 두어 점프로 못 빠져나가게 한다.
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L14_BOX[] = {
    {  0,  0, 96,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 95,  0,  1, 18, T_SOLID },
    {  1, 15, 20,  3, T_SOLID },
    { 21, 17,  6,  1, T_SPIKE },
    { 27, 15, 14,  3, T_SOLID },
    { 35, 12,  4,  3, T_SOLID },      /* 3칸 턱 */
    { 41, 17,  6,  1, T_SPIKE },
    { 47, 15, 16,  3, T_SOLID },
    { 52,  0, 16, 13, T_SOLID },      /* 저천장 — 여유 2칸 */
    { 63, 17,  4,  1, T_SPIKE },      /* 저천장 아래 4칸 구덩이 — dash 전용 */
    { 67, 15, 28,  3, T_SOLID },
};
static const TermSpec L14_TERM[] = { { 3, 14, 0 }, { 50, 14, 0 } };
static const HintSpec L14_HINT[] = {
    {  3, 12, 0,       "TWO TERMINALS",     "the second is far" },
    { 30, 12, "1",     "boot.exe",          "for the step", AB(AB_BOOT) },
    { 50, 11, 0,       "LOW CEILING AHEAD", "dash under it" },
    { 90, 12, 0,       "BACKUP.ZIP",        "everything, once" },
    { 93, 13, 0,       "DRIVE SLOT",        0 },
};

/* ══════════════════════════════════════════════════════════════════════
 *  15 — INVOICE.TXT   [중간]   보상: 없음
 *
 *  출구 직전에 굴뚝을 하나 두고 그 꼭대기에 디스크를 놓는다.
 *  **클리어에는 필요 없지만 챙기려면 파동을 등지고 올라갔다 내려와야 한다.**
 *  "챙길까 말까"에 처음으로 실제 시간 비용이 붙는 스테이지.
 *
 *  ★ 왼쪽으로 되돌아가는 왕복은 안 된다 — 파동이 오른쪽으로 오므로 정면충돌이다.
 *    "왕복"은 세로 우회로 만든다.
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L15_BOX[] = {
    {  0,  0, 88,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 87,  0,  1, 18, T_SOLID },
    {  1, 15, 22,  3, T_SOLID },
    { 23, 17,  6,  1, T_SPIKE },
    { 29, 15, 18,  3, T_SOLID },
    { 37, 12,  4,  3, T_SOLID },      /* 3칸 턱 */
    { 47, 17,  6,  1, T_SPIKE },
    { 53, 15, 22,  3, T_SOLID },
    { 60, 14,  4,  1, T_SPIKE },
    { 75, 15, 12,  3, T_SOLID },
    { 75,  4,  2, 10, T_SOLID },      /* 굴뚝 왼벽 (rows 4~13) — row14 는 통로 */
    { 80,  4,  2, 10, T_SOLID },      /* 굴뚝 오른벽 */
};
static const TermSpec L15_TERM[] = { { 3, 14, 0 }, { 31, 14, 0 }, { 56, 14, 0 } };
static const HintSpec L15_HINT[] = {
    {  3, 12, 0,       "THE DISK IS UP A SHAFT", "you do not need it to leave" },
    { 33, 12, "SHIFT", "dash.exe",               0,                      AB(AB_DASH) },
    { 70, 12, "4",     "grip.exe",               "if you want the disk", AB(AB_GRIP) },
    { 78,  2, 0,       "INVOICE.TXT",            "unpaid" },
    { 84, 13, 0,       "DRIVE SLOT",             "right past the shaft" },
};

/* ══════════════════════════════════════════════════════════════════════
 *  16 — CRASH.LOG   [어려움]
 *
 *  저천장 통로 60칸을 파동 52px/s 로 통과한다. 점프로는 못 빠져나온다 —
 *  구덩이마다 대시를 정확히 써야 하고, 터미널은 셋뿐이다.
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L16_BOX[] = {
    {  0,  0, 88,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 87,  0,  1, 18, T_SOLID },
    {  1, 15, 12,  3, T_SOLID },
    { 13,  0, 60, 13, T_SOLID },      /* 저천장 x13~72 — 여유 2칸 */
    { 13, 15, 10,  3, T_SOLID },
    { 23, 17,  4,  1, T_SPIKE },
    { 27, 15, 10,  3, T_SOLID },
    { 37, 17,  4,  1, T_SPIKE },
    { 41, 15, 10,  3, T_SOLID },
    { 51, 17,  4,  1, T_SPIKE },
    { 55, 15, 10,  3, T_SOLID },
    { 65, 17,  4,  1, T_SPIKE },
    { 69, 15, 18,  3, T_SOLID },
    /* 轉: 천장이 끝나고 6칸 벽이 나온다. 60칸 내내 못 뛰게 해놓고
     * 마지막에 **점프로는 안 되는 벽**을 세운다 — 적응한 규칙이 뒤집힌다 */
    { 76,  9,  2,  9, T_SOLID },
    { 78,  9,  9,  2, T_SOLID },
};
static const TermSpec L16_TERM[] = { { 3, 14, 0 }, { 30, 14, 0 }, { 58, 14, 0 }, { 73, 14, 0 } };
static const HintSpec L16_HINT[] = {
    {  3, 12, 0,       "60 TILES OF LOW CEILING", "no jumping in there" },
    { 23, 11, "SHIFT", "dash.exe",                "four gaps", AB(AB_DASH) },
    { 74, 12, 0,       "THE CEILING ENDS",        "and now you have to climb" },
    { 81,  6, 0,       "CRASH.LOG",               "the last thing it wrote" },
    { 84,  7, 0,       "DRIVE SLOT",              0 },
};

/* ══════════════════════════════════════════════════════════════════════
 *  17 — KERNEL.SYS   [어려움]
 *
 *  굴뚝 둘. 첫째는 10칸, 둘째는 12칸. 사이 지형이 전부 가시라 떨어지면 끝이다.
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L17_BOX[] = {
    {  0,  0, 80,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 79,  0,  1, 18, T_SOLID },
    {  1, 15, 20,  3, T_SOLID },
    { 13,  4,  2, 10, T_SOLID },      /* 굴뚝1 (x15~17 통로) */
    { 18,  4,  2, 10, T_SOLID },
    { 20,  4, 10,  2, T_SOLID },      /* 위 발판 surface row4 */
    { 30, 17, 12,  1, T_SPIKE },
    { 42, 15, 16,  3, T_SOLID },
    { 44,  3,  2, 11, T_SOLID },      /* 굴뚝2 (x46~48 통로) — row14 는 통로 */
    { 49,  3,  2, 11, T_SOLID },
    { 51,  3, 16,  2, T_SOLID },      /* 위 발판 surface row3 */
    { 58, 17, 11,  1, T_SPIKE },
    { 69, 15, 10,  3, T_SOLID },
};
/* 굴뚝 바로 앞에 터미널을 둔다 — 올라가는 동안 파동이 계속 다가오므로
 * 등반 직전에 파동을 뒤로 물려주지 않으면 잡힌다. */
static const TermSpec L17_TERM[] = { { 3, 14, 0 }, { 11, 14, 0 }, { 24, 3, 0 }, { 46, 14, 0 }, { 56, 2, 0 } };
static const HintSpec L17_HINT[] = {
    {  3, 12, 0,       "TWO SHAFTS",  "nothing but spikes below" },
    { 16, 12, "4",     "grip.exe",    "zigzag between the walls", AB(AB_GRIP) },
    { 46, 12, 0,       "AGAIN, TALLER", 0 },
    { 74, 12, 0,       "KERNEL.SYS",  "the part that never crashed" },
    { 77, 13, 0,       "DRIVE SLOT",  0 },
};

/* ══════════════════════════════════════════════════════════════════════
 *  18 — LASTMAIL.TXT   [어려움]
 *
 *  구간마다 터미널이 **다른 유틸리티만** 갖고 있다.
 *    터미널1 boot|dash → 평지 구덩이 / 터미널2 boot|grip → 굴뚝 /
 *    터미널3 boot|glide → 하강
 *  셋을 다 들면 15칸이라 12칸에 안 들어간다 → **매번 갈아끼워야 한다.**
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L18_BOX[] = {
    {  0,  0, 96,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 95,  0,  1, 18, T_SOLID },
    {  1, 15, 14,  3, T_SOLID },
    { 15, 17,  6,  1, T_SPIKE },
    { 21, 15, 12,  3, T_SOLID },
    { 33, 17,  6,  1, T_SPIKE },
    { 39, 15, 14,  3, T_SOLID },
    { 43,  4,  2, 10, T_SOLID },      /* 굴뚝 (x45~47 통로) */
    { 48,  4,  2, 10, T_SOLID },
    { 50,  4, 12,  2, T_SOLID },      /* 위 발판 surface row4 */
    { 62, 17, 14,  1, T_SPIKE },
    { 71,  9, 10,  2, T_SOLID },      /* 5칸 하강 · 9칸 구덩이 → glide */
    { 81, 17,  6,  1, T_SPIKE },
    { 87, 15,  8,  3, T_SOLID },
};
static const TermSpec L18_TERM[] = {
    {  3, 14, AB(AB_BOOT) | AB(AB_DASH)  },
    { 41, 14, AB(AB_BOOT) | AB(AB_GRIP)  },
    { 58,  3, AB(AB_BOOT) | AB(AB_GLIDE) },
};
static const HintSpec L18_HINT[] = {
    {  3, 12, 0,       "EACH TERMINAL HAS ONE TOOL", "you cannot carry all three" },
    /* ★ 원래 "grip.exe only here" / "glide.exe only here" 였다.
     *   갈림길이 있다는 **정보**는 남기고 답은 지운다 — 뭘 들고 왔는지에 따라
     *   길이 갈리는 게 이 스테이지의 전부인데, 답을 적어두면 고를 것이 없어진다. */
    { 41, 12, 0,       "TWO WAYS UP",                "one of them is yours" },
    { 58,  1, 0,       "THE OTHER WAY",              0 },
    { 90, 12, 0,       "LASTMAIL.TXT",               "sent, no reply" },
    { 93, 13, 0,       "DRIVE SLOT",                 0 },
};

/* ══════════════════════════════════════════════════════════════════════
 *  19 — GOODBYE.TXT   [어려움]
 *
 *  종합. 대시 구간 → 굴뚝 → 활공 하강. 터미널 사이 간격이 넓다.
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L19_BOX[] = {
    {  0,  0, 96,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 95,  0,  1, 18, T_SOLID },
    {  1, 15, 16,  3, T_SOLID },
    { 17, 17,  6,  1, T_SPIKE },
    { 23, 15, 10,  3, T_SOLID },
    { 27, 14,  4,  1, T_SPIKE },
    { 33, 17,  6,  1, T_SPIKE },
    { 39, 15, 14,  3, T_SOLID },
    { 43,  4,  2, 10, T_SOLID },
    { 48,  4,  2, 10, T_SOLID },
    { 50,  4, 14,  2, T_SOLID },      /* surface row4 */
    { 64, 17, 10,  1, T_SPIKE },
    { 74,  9,  8,  2, T_SOLID },      /* 5칸 하강 · 10칸 구덩이 → glide */
    { 82, 17,  5,  1, T_SPIKE },
    { 87, 15,  8,  3, T_SOLID },
};
static const TermSpec L19_TERM[] = { { 3, 14, 0 }, { 41, 14, 0 }, { 60, 3, 0 } };
static const HintSpec L19_HINT[] = {
    {  3, 12, 0,       "EVERYTHING AT ONCE", "three terminals, far apart" },
    { 41, 12, 0,       "SHAFT",              0 },
    { 62,  2, 0,       "THEN DOWN",          0 },
    { 90, 12, 0,       "GOODBYE.TXT",        "one line, unfinished" },
    { 93, 13, 0,       "DRIVE SLOT",         0 },
};

/* ══════════════════════════════════════════════════════════════════════
 *  20 — ME.IMG   [어려움]   ★ 클라이맥스
 *
 *  **디스크가 9칸을 먹는다.** 12칸 중 3칸만 남으므로 담는 순간
 *  들 수 있는 건 boot.exe(3칸) 하나뿐이다 — 처음 시작할 때 갖고 있던 그것.
 *
 *  그래서 레벨이 두 부분으로 나뉜다.
 *    x1~54   전 능력을 쓰는 구간 (6칸 구덩이 · 4칸 턱)
 *    x55~88  **boot 하나로만 가는 길** (4칸 구덩이 · 3칸 턱)
 *
 *  담을지 말지가 곧 "지금까지 모은 걸 다 버릴지"다. 규칙과 이야기가 여기서 만난다.
 *
 *  ⚠️ 그래서 이 스테이지만 "디스크는 출구 8칸 이내" 규칙에서 빠진다.
 *    그 규칙은 선택 회수물을 위한 것이고, 여기 디스크는 목표 그 자체다.
 * ══════════════════════════════════════════════════════════════════════ */
static const Box L20_BOX[] = {
    {  0,  0, 96,  1, T_SOLID }, {  0,  0,  1, 18, T_SOLID }, { 95,  0,  1, 18, T_SOLID },
    {  1, 15, 16,  3, T_SOLID },
    { 17, 17,  8,  1, T_SPIKE },      /* 8칸 → dash (boot 는 6.3칸까지) */
    { 25, 15,  8,  3, T_SOLID },
    { 33, 17, 12,  1, T_SPIKE },      /* 12칸 구덩이 */
    { 36, 10,  7,  1, T_ONEWAY },     /* ★ 뜬 선반 5칸 위 → boot+jump2 (s09 와 같은 관문) */
    { 45, 11, 26,  7, T_SOLID },      /* x45~70 표면 row11 — 여기 터미널과 디스크가 있다 */
    { 71, 17,  4,  1, T_SPIKE },      /* 4칸 */
    { 75, 15, 20,  3, T_SOLID },
    { 79, 12,  4,  3, T_SOLID },      /* 3칸 턱 → boot 가 반드시 필요한 유일한 지점 */
};
static const TermSpec L20_TERM[] = { { 3, 14, 0 }, { 28, 14, 0 }, { 50, 10, 0 }, { 77, 14, 0 } };
static const HintSpec L20_HINT[] = {
    {  3, 12, 0,       "ME.IMG",            "the machine itself" },
    { 36,  7, 0,       "LAST HARD PART",    0 },
    { 50,  8, 0,       "DROP EVERYTHING",   "the image needs 9 slots" },
    { 58,  7, 0,       "9 SLOTS",           "3 left - exactly boot.exe" },
    /* ★ 여기 boot.exe 만 teaches 를 안 붙인다. 이건 사용법이 아니라 결말이다 */
    { 79, 10, "1",     "boot.exe",          "all you started with" },
    { 92, 12, 0,       "DRIVE SLOT",        "take it out" },
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
      72, 2, 15,  68, 12,  65, 12,  22.0f },

    { "s03", "SECTOR 03", "LETTER.TXT", "never sent", 4096,
      L03_BOX,  (int)(sizeof(L03_BOX) /sizeof(L03_BOX[0])),
      L03_TERM, (int)(sizeof(L03_TERM)/sizeof(L03_TERM[0])),
      L03_HINT, (int)(sizeof(L03_HINT)/sizeof(L03_HINT[0])),
      AB(AB_GLIDE), AB(AB_DASH),
      64, 2, 15,  61, 14,  58, 14,  30.0f },

    { "s04", "SECTOR 04", "SONG.MOD", "4 channels, 3:12", 61440,
      L04_BOX,  (int)(sizeof(L04_BOX) /sizeof(L04_BOX[0])),
      L04_TERM, (int)(sizeof(L04_TERM)/sizeof(L04_TERM[0])),
      L04_HINT, (int)(sizeof(L04_HINT)/sizeof(L04_HINT[0])),
      0, AB(AB_GLIDE),
      86, 2, 4,  82, 16,  79, 16,  26.0f },

    { "s05", "SECTOR 05", "PHOTO.PCX", "scanned from paper", 24576,
      L05_BOX,  (int)(sizeof(L05_BOX) /sizeof(L05_BOX[0])),
      L05_TERM, (int)(sizeof(L05_TERM)/sizeof(L05_TERM[0])),
      L05_HINT, (int)(sizeof(L05_HINT)/sizeof(L05_HINT[0])),
      AB(AB_GRIP), AB(AB_DASH) | AB(AB_GLIDE),
      84, 2, 8,  79, 12,  76, 12,  32.0f },

    { "s06", "SECTOR 06", "DIARY.TXT", "entries stop in march", 8192,
      L06_BOX,  (int)(sizeof(L06_BOX) /sizeof(L06_BOX[0])),
      L06_TERM, (int)(sizeof(L06_TERM)/sizeof(L06_TERM[0])),
      L06_HINT, (int)(sizeof(L06_HINT)/sizeof(L06_HINT[0])),
      0, AB(AB_GRIP),
      56, 2, 15,  53, 14,  50, 14,  28.0f },

    /* needs = 0 : 구간마다 길이 갈려서 **어느 한 능력도 필수가 아니다.**
     * (다만 구간2 평지 구덩이는 dash 가 사실상 유일한 답이다 — 아래 needs 에 넣어둔다) */
    { "s07", "SECTOR 07", "MAZE.DAT", "someone mapped this by hand", 12288,
      L07_BOX,  (int)(sizeof(L07_BOX) /sizeof(L07_BOX[0])),
      L07_TERM, (int)(sizeof(L07_TERM)/sizeof(L07_TERM[0])),
      L07_HINT, (int)(sizeof(L07_HINT)/sizeof(L07_HINT[0])),
      0, AB(AB_DASH),
      96, 2, 15,  93, 16,  90, 16,  36.0f },

    { "s08", "SECTOR 08", "THEME.MOD", "his favourite", 57344,
      L08_BOX,  (int)(sizeof(L08_BOX) /sizeof(L08_BOX[0])),
      L08_TERM, (int)(sizeof(L08_TERM)/sizeof(L08_TERM[0])),
      L08_HINT, (int)(sizeof(L08_HINT)/sizeof(L08_HINT[0])),
      AB(AB_JUMP2), AB(AB_BOOT) | AB(AB_GRIP),
      60, 2, 15,  54, 2,  51, 2,  40.0f },

    { "s09", "SECTOR 09", "SAVE02.DAT", "a later save, same game", 81920,
      L09_BOX,  (int)(sizeof(L09_BOX) /sizeof(L09_BOX[0])),
      L09_TERM, (int)(sizeof(L09_TERM)/sizeof(L09_TERM[0])),
      L09_HINT, (int)(sizeof(L09_HINT)/sizeof(L09_HINT[0])),
      0, AB(AB_BOOT) | AB(AB_JUMP2) | AB(AB_DASH),
      76, 2, 15,  73, 6,  71, 6,  30.0f },

    { "s10", "SECTOR 10", "README.1ST", "read me first", 2048,
      L10_BOX,  (int)(sizeof(L10_BOX) /sizeof(L10_BOX[0])),
      L10_TERM, (int)(sizeof(L10_TERM)/sizeof(L10_TERM[0])),
      L10_HINT, (int)(sizeof(L10_HINT)/sizeof(L10_HINT[0])),
      0, AB(AB_DASH),
      72, 2, 15,  69, 14,  66, 14,  44.0f },   /* 속도 스테이지지만 48 은 어려운 티어보다 빨랐다 */

    { "s11", "SECTOR 11", "NOTES.TXT", "half a sentence, then nothing", 3072,
      L11_BOX,  (int)(sizeof(L11_BOX) /sizeof(L11_BOX[0])),
      L11_TERM, (int)(sizeof(L11_TERM)/sizeof(L11_TERM[0])),
      L11_HINT, (int)(sizeof(L11_HINT)/sizeof(L11_HINT[0])),
      0, AB(AB_BOOT) | AB(AB_DASH),
      72, 2, 15,  69, 14,  66, 14,  34.0f },

    { "s12", "SECTOR 12", "TRACK02.MOD", "an unfinished loop", 45056,
      L12_BOX,  (int)(sizeof(L12_BOX) /sizeof(L12_BOX[0])),
      L12_TERM, (int)(sizeof(L12_TERM)/sizeof(L12_TERM[0])),
      L12_HINT, (int)(sizeof(L12_HINT)/sizeof(L12_HINT[0])),
      0, AB(AB_GLIDE),
      72, 2, 15,  69, 14,  66, 14,  38.0f },

    { "s13", "SECTOR 13", "SPRITE.PCX", "16 by 16, four colours", 6144,
      L13_BOX,  (int)(sizeof(L13_BOX) /sizeof(L13_BOX[0])),
      L13_TERM, (int)(sizeof(L13_TERM)/sizeof(L13_TERM[0])),
      L13_HINT, (int)(sizeof(L13_HINT)/sizeof(L13_HINT[0])),
      0, AB(AB_BOOT),
      72, 2, 15,  69, 14,  66, 14,  36.0f },

    { "s14", "SECTOR 14", "BACKUP.ZIP", "everything, once", 131072,
      L14_BOX,  (int)(sizeof(L14_BOX) /sizeof(L14_BOX[0])),
      L14_TERM, (int)(sizeof(L14_TERM)/sizeof(L14_TERM[0])),
      L14_HINT, (int)(sizeof(L14_HINT)/sizeof(L14_HINT[0])),
      0, AB(AB_BOOT) | AB(AB_DASH),
      96, 2, 15,  93, 14,  90, 14,  46.0f },

    { "s15", "SECTOR 15", "INVOICE.TXT", "unpaid", 2560,
      L15_BOX,  (int)(sizeof(L15_BOX) /sizeof(L15_BOX[0])),
      L15_TERM, (int)(sizeof(L15_TERM)/sizeof(L15_TERM[0])),
      L15_HINT, (int)(sizeof(L15_HINT)/sizeof(L15_HINT[0])),
      0, AB(AB_DASH),
      88, 2, 15,  84, 14,  78, 4,  40.0f },

    { "s16", "SECTOR 16", "CRASH.LOG", "the last thing it wrote", 16384,
      L16_BOX,  (int)(sizeof(L16_BOX) /sizeof(L16_BOX[0])),
      L16_TERM, (int)(sizeof(L16_TERM)/sizeof(L16_TERM[0])),
      L16_HINT, (int)(sizeof(L16_HINT)/sizeof(L16_HINT[0])),
      0, AB(AB_DASH) | AB(AB_GRIP),
      88, 2, 15,  84, 8,  81, 8,  50.0f },

    { "s17", "SECTOR 17", "KERNEL.SYS", "the part that never crashed", 40960,
      L17_BOX,  (int)(sizeof(L17_BOX) /sizeof(L17_BOX[0])),
      L17_TERM, (int)(sizeof(L17_TERM)/sizeof(L17_TERM[0])),
      L17_HINT, (int)(sizeof(L17_HINT)/sizeof(L17_HINT[0])),
      0, AB(AB_GRIP),
      80, 2, 15,  77, 14,  74, 14,  44.0f },

    { "s18", "SECTOR 18", "LASTMAIL.TXT", "sent, no reply", 5120,
      L18_BOX,  (int)(sizeof(L18_BOX) /sizeof(L18_BOX[0])),
      L18_TERM, (int)(sizeof(L18_TERM)/sizeof(L18_TERM[0])),
      L18_HINT, (int)(sizeof(L18_HINT)/sizeof(L18_HINT[0])),
      0, AB(AB_DASH) | AB(AB_GRIP) | AB(AB_GLIDE),
      96, 2, 15,  93, 14,  90, 14,  48.0f },

    { "s19", "SECTOR 19", "GOODBYE.TXT", "one line, unfinished", 1024,
      L19_BOX,  (int)(sizeof(L19_BOX) /sizeof(L19_BOX[0])),
      L19_TERM, (int)(sizeof(L19_TERM)/sizeof(L19_TERM[0])),
      L19_HINT, (int)(sizeof(L19_HINT)/sizeof(L19_HINT[0])),
      0, AB(AB_DASH) | AB(AB_GRIP) | AB(AB_GLIDE),
      96, 2, 15,  93, 14,  90, 14,  52.0f },

    /* ★ 마지막 필드가 diskCost. 이 스테이지만 디스크가 9칸을 먹는다. */
    { "s20", "SECTOR 20", "ME.IMG", "the machine itself", 1474560,
      L20_BOX,  (int)(sizeof(L20_BOX) /sizeof(L20_BOX[0])),
      L20_TERM, (int)(sizeof(L20_TERM)/sizeof(L20_TERM[0])),
      L20_HINT, (int)(sizeof(L20_HINT)/sizeof(L20_HINT[0])),
      0, AB(AB_BOOT) | AB(AB_DASH) | AB(AB_JUMP2),
      96, 2, 15,  92, 14,  58, 10,  40.0f,  9 },
};

const int LEVEL_COUNT = (int)(sizeof(LEVELS) / sizeof(LEVELS[0]));

/* 능력을 보상으로 준 스테이지의 **다음**이 그 능력의 교실이다.
 * boot 는 아무도 안 주므로(처음부터 갖고 있다) s01 이 교실이 된다. */
int hint_school_level(unsigned ability)
{
    if (ability & AB(AB_BOOT)) return 0;
    for (int i = 0; i < LEVEL_COUNT; i++)
        if (LEVELS[i].reward & ability)
            return (i + 1 < LEVEL_COUNT) ? i + 1 : i;
    return -1;
}
