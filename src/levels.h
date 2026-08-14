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

/* 힌트 — **월드 좌표에 붙어서 가까이 있는 동안 계속 떠 있는다.**
 *
 * 화면 중앙에 한 번 띄우고 끝내면 읽을 타이밍을 플레이어가 못 고른다.
 * 파동에 쫓기는 중이면 더 그렇다. 위치에 고정해야 "필요할 때 거기 있는" 안내가 된다.
 *
 * keys: 키캡으로 그릴 것. 공백 구분. NULL 이면 없음. 예) "< >"  "SPACE"  "5"
 * range: 이 타일 거리 안에 들어오면 보이고, 가장자리에서 서서히 사라진다. */
typedef struct {
    short x, y;
    short range;
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

    short width;             /* 이 레벨이 실제로 쓰는 폭 (타일). 카메라 클램프에 쓴다 */
    short spawnCol, spawnRow;
    short goalCol, goalRow;
    float formatSpeed;       /* 스테이지별 추격 압력 */
} Level;

extern const Level LEVELS[];
extern const int   LEVEL_COUNT;

#endif
