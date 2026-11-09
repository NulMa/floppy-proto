/* levels.h — 레벨 데이터의 형식.
 *
 * 레벨은 96자짜리 문자열이 아니라 **사각형 목록**으로 짓는다. 손으로 96자를 못 맞춘다.
 * 터미널은 박스가 아니라 별도 목록이다 — 각자 "여기서 꺼낼 수 있는 유틸리티"가 다르기 때문. */
#ifndef LEVELS_H
#define LEVELS_H


typedef struct { short x, y, w, h; unsigned char t; } Box;

/* avail: 이 터미널에서 장착 가능한 능력 비트마스크. 0 이면 전부 가능.
 * 튜토리얼처럼 "이 섹터엔 이 유틸리티만 남아 있다"를 표현할 때 쓴다. */
typedef struct { short x, y; unsigned short avail; } TermSpec;

/* 힌트 — **월드 좌표에 붙어서 항상 떠 있는다.** 박스 없이 텍스트만.
 *
 * 화면 중앙에 한 번 띄우고 끝내면 읽을 타이밍을 플레이어가 못 고른다.
 * 근처에서만 뜨게 해도 놓치기 쉽다. 화면에 들어오면 그냥 보이는 게 낫다.
 * (화면 밖은 카메라가 알아서 잘라내므로 별도 범위 판정이 필요 없다)
 *
 * keys: 키 표기. 공백 구분. NULL 이면 없음. 예) "< >"  "SPACE"  "1"
 *       렌더에서 [ ] 로 감싸 텍스트로 그린다
 *
 * teaches: 이 힌트가 **가르치는** 능력 비트. 0 이면 항상 뜬다(지형 경고·서사).
 *
 *   ★ 능력 사용법은 **해금 직후 한 번만** 알려준다.
 *
 *     예전엔 모든 스테이지의 모든 관문 위에 "여기선 dash 를 써라"가 떠 있었다.
 *     그러면 플레이어가 전략을 짜는 게 아니라 **지시를 수행**하게 된다.
 *     이 게임은 "무엇을 들고 갈까"가 전부인데, 답을 벽에 써 붙여두면
 *     고를 것이 남지 않는다.
 *
 *     그래서 teaches 가 붙은 힌트는 그 능력을 **막 얻은 스테이지에서만** 뜬다.
 *     어느 스테이지인지는 LEVELS 의 reward 에서 자동으로 나온다 —
 *     능력을 준 스테이지의 바로 다음이다. 손으로 적지 않는다.
 *
 * ★★ teaches 가 0 인 힌트에도 규칙이 있다 (2026-08-19).
 *
 *   위 규칙은 teaches 가 붙은 힌트만 걸렀다. 그런데 힌트 118줄 중 teaches 는
 *   20줄뿐이었고, **나머지 98줄이 무조건 상시 표시**였다 — 스테이지당 5개.
 *   그 98개 안에 "NOW DROP RIGHT" "THEN DOWN" "AGAIN" 같은 **행동 지시**가
 *   26개 있었다. 태그가 안 붙어 있었다는 이유만으로 필터를 통과한 것이고,
 *   성질은 위에서 지우기로 한 것과 똑같다. 규칙이 한 층만 덮고 있었다.
 *
 *   그래서 teaches 가 0 인 힌트는 **아래 셋 중 하나여야 한다.** 아니면 안 넣는다.
 *
 *     1. 디스크 파일명 — 스테이지당 하나. 회수 대상이 뭔지, 그리고 서사
 *     2. 기믹 첫 등장 — 그 타일이 처음 나오는 스테이지에서 한 번만
 *          교대 가시 s03 · 상승 기류 s04 · 무너지는 발판 s07 · 원웨이 s09
 *          (점프패드·가시는 s01 튜토리얼에서)
 *     3. s01 튜토리얼 — 조작·파동·터미널. 여기만 예외적으로 많다
 *
 *   금지: 방향 지시("DROP RIGHT"), 답 적어두기("grip.exe OR boot.exe"),
 *         이름표 반복(DRIVE SLOT 을 19판에 다 붙이는 것), 전(轉) 예고.
 *
 *   118줄 -> 54줄. 화면에 뜨는 건 스테이지당 5.0개 -> 약 1.8개.
 */
typedef struct {
    short x, y;
    const char *keys;
    const char *l1, *l2;   /* l1 = 굵은 줄(무엇) · l2 = 설명(어떻게). l2 는 없어도 된다 */
    unsigned teaches;
} HintSpec;

/* 힌트 한 줄을 지금 언어로 고른다. 없으면 0 이 그대로 나온다. */

/* 그 능력을 처음 쓰는 스테이지 인덱스. 없으면 -1.
 * boot 는 처음부터 갖고 있으므로 0 (s01). */
int hint_school_level(unsigned ability);

typedef struct {
    const char *id;          /* "s00" */
    const char *sector;      /* 화면에 뜨는 섹터 표기 */
    const char *file;        /* 회수 대상 파일명 */
    /* 그 파일의 한 줄 설명 — **이 게임의 감정이 여기 있다.**
     *
     * ★ **번역하지 않는다.** 파일명과 같은 층이다 — 이 화면은 기계의 출력이고
     *   그 파일에 원래 그렇게 적혀 있던 말이다. `AUTOEXEC.BAT` 을 안 옮기면서
     *   `the file this machine read first` 만 옮기면 둘이 따로 논다.
     *   (2026-08-19 결정. 한때 한국어를 나란히 뒀다가 되돌렸다) */
    const char *meta;
    long        fileBytes;

    const Box      *boxes;  int boxCount;
    const TermSpec *terms;  int termCount;
    const HintSpec *hints;  int hintCount;

    /* 이 스테이지를 클리어하면 영구 해금되는 능력 (비트마스크).
     * 회수한 디스크 안에 그 유틸리티가 들어 있었다는 설정이다. */
    unsigned reward;

    /* 이 스테이지를 깨는 데 **반드시 필요한** 능력 (비트마스크).
     *
     * 보상형 진행에서 제일 위험한 버그는 "그 시점 능력으로 못 깨는 스테이지"다.
     * 지형을 자동으로 분석하는 건 (특히 활공 하강) 정확도가 안 나오므로,
     * 만든 사람이 선언하고 test.bat 이 해금 곡선과 대조한다. */
    unsigned needs;

    short width;             /* 이 레벨이 실제로 쓰는 폭 (타일). 카메라 클램프에 쓴다 */
    short spawnCol, spawnRow;

    /* 클리어는 **출구**로 한다. 디스크는 그 근처에 놓인 선택 회수물이다.
     * 디스크 없이 통과만 하는 스테이지도 있어야 하므로 둘을 분리했다.
     * 디스크가 없는 레벨은 diskCol 을 -1 로 둔다. */
    short exitCol, exitRow;
    short diskCol, diskRow;

    float formatSpeed;       /* 스테이지별 추격 압력 */

    /* 이 디스크가 먹는 용량(칸). 0 이면 공짜 — 대부분의 스테이지가 그렇다.
     *
     * ★ 마지막에 붙여둔다. 중간에 끼우면 기존 레벨의 위치 초기화가 통째로 밀린다.
     *   (실제로 한 번 그럴 뻔했다 — formatSpeed 가 diskCost 자리로 들어갔다)
     *
     * 마지막 스테이지에서만 쓴다. 담으려면 능력을 버려야 하므로
     * **회수한 뒤의 길은 남는 칸으로 갈 수 있어야 한다.** test.bat 이 검사한다. */
    short diskCost;
} Level;

extern const Level LEVELS[];
extern const int   LEVEL_COUNT;

#endif
