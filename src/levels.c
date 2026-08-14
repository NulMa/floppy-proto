#include "levels.h"
#include "sim.h"

#define AB(x) (1u << (x))

/* ══════════════════════════════════════════════════════════════════════
 *  SECTOR 00 — AUTOEXEC.BAT  (튜토리얼)
 *
 *  가르치는 것은 넷뿐이다. 한 번에 하나씩, 텍스트는 최소로.
 *    ① 이동·점프        평지 + 1칸 계단 + 2칸 구덩이 (전부 맨몸으로 됨)
 *    ② 포맷 파동        느리게 쫓아온다. "오른쪽으로 계속 가라"
 *    ③ 터미널           밟으면 파동이 멈춘다 = 안전. 여기서만 유틸리티를 로드
 *    ④ 능력으로 통과    3칸 턱(맨몸 2.4칸으로 불가) → boot(3.4칸) 필요
 *
 *  ★ 여기서 쓸 수 있는 능력은 boot 하나뿐이다.
 *    나머지는 스테이지 클리어로 하나씩 해금된다. 아직 안 얻은 건 목록에 뜨지도 않는다.
 *    그래서 튜토리얼은 "고르는 법"이 아니라 "쓰는 법"만 가르친다.
 *
 *  ★ 두 번째 터미널을 턱 **바로 앞**에 둔다.
 *    못 넘고 되돌아가야 하는데 파동이 쫓아오면 첫 경험이 최악이 된다.
 * ══════════════════════════════════════════════════════════════════════ */
static const Box S00_BOXES[] = {
    {  0,  0, 46,  1, T_SOLID },      /* 천장 */
    {  0,  0,  1, 18, T_SOLID },      /* 왼쪽 벽 */
    { 45,  0,  1, 18, T_SOLID },      /* 오른쪽 벽 */

    {  1, 15, 14,  3, T_SOLID },      /* 바닥 A  x1~14 */
    { 10, 14,  2,  1, T_SOLID },      /* 1칸 계단 — 맨몸으로 넘어진다 */
    /* 구덩이 x15~16 (2칸 = 32px) — 맨몸 점프(75px)로 여유 있게 건넌다 */
    { 17, 15, 28,  3, T_SOLID },      /* 바닥 B  x17~44 */
    { 19, 12,  4,  6, T_SOLID },      /* 3칸 턱 (48px) — boot 없이는 못 넘는다 */
};

static const TermSpec S00_TERMS[] = {
    {  5, 14, 0 },      /* 첫 소개용 */
    { 17, 14, 0 },      /* 턱 바로 앞 — 여기서 boot 를 켜고 넘는다 */
};

static const MsgSpec S00_MSGS[] = {
    {  1,  4, "ARROWS TO MOVE", "SPACE TO JUMP" },
    {  7,  9, "FORMAT IN PROGRESS", "KEEP MOVING RIGHT" },
    { 17, 18, "THAT LEDGE IS TOO HIGH", "LOAD boot.exe HERE  [5]" },
    { 38, 40, "AUTOEXEC.BAT", "the file this machine read first" },
};

/* ══════════════════════════════════════════════════════════════════════
 *  SECTOR 01 — SAVE01.DAT
 *
 *  여기서 "전부 못 들고 간다"를 가르친다. 능력 5종 20칸 / 용량 12칸.
 *  구간마다 **서로 다른 능력으로 가는 두 갈래 길**이 있다.
 *
 *    구간1 (x1~31)   세로 장벽 9칸    : grip 직등 / boot·jump2 로 3칸 발판
 *    구간2 (x32~61)  구덩이 6칸 x2    : dash / jump2
 *    구간3 (x62~94)  저천장 통로      : dash·glide
 *                    또는 위층        : grip 으로 9칸 벽면 등반
 *
 *  ★ 불변식: 단독으로 전 구간을 여는 능력이 없어야 한다. test.bat 이 검사한다.
 *    jump2 는 높이(4.4칸)도 거리(8.9칸)도 1위라, 구간3 저천장(도달 2.2칸)이
 *    유일한 차단 수단이다. 이 구조를 깨면 로드아웃 선택이 무의미해진다.
 * ══════════════════════════════════════════════════════════════════════ */
static const Box S01_BOXES[] = {
    {  0,  0, 96,  1, T_SOLID },
    {  0,  0,  1, 18, T_SOLID },
    { 95,  0,  1, 18, T_SOLID },

    /* 구간 1 */
    {  1, 15, 31,  3, T_SOLID },
    { 13, 12,  2,  1, T_SOLID },      /* 발판 (3칸) */
    { 16,  9,  2,  1, T_SOLID },      /* 발판 (3칸) */
    { 19,  6,  3,  9, T_SOLID },      /* 장벽 — 꼭대기는 바닥에서 9칸 */

    /* 구간 2 */
    { 32, 15,  7,  3, T_SOLID },
    { 45, 15,  6,  3, T_SOLID },      /* 구덩이 x39~44 */
    { 57, 15,  5,  3, T_SOLID },      /* 구덩이 x51~56 */

    /* 구간 3 — 위층 아랫면이 row12 여야 맨몸 점프로 벽면을 잡을 수 있다.
     * row10 까지만 내려오면 grip 루트가 통째로 막힌다 (실제로 한 번 그렇게 만들었다) */
    { 62,  6, 29,  7, T_SOLID },      /* 위층 x62~90, row6~12 */
    { 62, 15,  6,  3, T_SOLID },      /* 통로 바닥 — 머리 위 여유 2칸 */
    { 72, 17, 23,  1, T_SOLID },      /* 4칸 구덩이 건넌 뒤 (2칸 아래) */
    { 72, 14, 19,  1, T_SOLID },      /* 낮은 천장 */
};

static const TermSpec S01_TERMS[] = {
    {  2, 14, 0 },     /* 0 = 전부 가능 */
    { 29, 14, 0 },
    { 58, 14, 0 },
};

static const MsgSpec S01_MSGS[] = {
    {  1,  4, "TWO UTILITIES NOW", "SWAP THEM AT TERMINALS" },
    { 27, 30, "BOOT GETS YOU UP", "DASH GETS YOU ACROSS" },
    { 60, 63, "LOW CEILING BELOW", "JUMPING WILL NOT HELP HERE" },
};

/* ══════════════════════════════════════════════════════════════════════ */

/* 해금 곡선 — 능력 합계가 12칸을 넘는 시점부터 "무엇을 두고 갈까"가 시작된다.
 *
 *   s00 시작   boot                      3칸   여유 9
 *   s00 클리어 +dash                     7칸   여유 5
 *   s01 클리어 +glide                   11칸   여유 1
 *   s02 클리어 +grip                    15칸   ← 여기서 처음 조인다
 *   s03 클리어 +jump2                   20칸   ← 8칸을 버려야 한다
 */
const Level LEVELS[] = {
    {
        "s00", "SECTOR 00", "AUTOEXEC.BAT", "the file this machine read first", 512,
        S00_BOXES, (int)(sizeof(S00_BOXES) / sizeof(S00_BOXES[0])),
        S00_TERMS, (int)(sizeof(S00_TERMS) / sizeof(S00_TERMS[0])),
        S00_MSGS,  (int)(sizeof(S00_MSGS)  / sizeof(S00_MSGS[0])),
        AB(AB_DASH),
        46, 2, 15, 42, 14, 22.0f,
    },
    {
        "s01", "SECTOR 01", "SAVE01.DAT", "last saved 1998-04-11", 73728,
        S01_BOXES, (int)(sizeof(S01_BOXES) / sizeof(S01_BOXES[0])),
        S01_TERMS, (int)(sizeof(S01_TERMS) / sizeof(S01_TERMS[0])),
        S01_MSGS,  (int)(sizeof(S01_MSGS)  / sizeof(S01_MSGS[0])),
        AB(AB_GLIDE),
        96, 2, 15, 93, 16, 34.0f,
    },
};

const int LEVEL_COUNT = (int)(sizeof(LEVELS) / sizeof(LEVELS[0]));
