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
 *       렌더에서 [ ] 로 감싸 텍스트로 그린다 */
typedef struct {
    short x, y;
    const char *keys;
    const char *l1, *l2;
} HintSpec;

typedef struct {
    const char *id;          /* "s00" */
    const char *sector;      /* 화면에 뜨는 섹터 표기 */
    const char *file;        /* 회수 대상 파일명 */
    const char *meta;        /* 그 파일의 한 줄 메타 — 이 게임의 감정이 여기 있다 */
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
} Level;

extern const Level LEVELS[];
extern const int   LEVEL_COUNT;

#endif
