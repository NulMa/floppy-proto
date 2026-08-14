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

/* 플레이어가 x0..x1 (타일) 구간에 들어오면 한 번 표시하고 끝 */
typedef struct { short x0, x1; const char *l1, *l2; } MsgSpec;

typedef struct {
    const char *id;          /* "s00" */
    const char *sector;      /* 화면에 뜨는 섹터 표기 */
    const char *file;        /* 회수 대상 파일명 */
    const char *meta;        /* 그 파일의 한 줄 메타 — 이 게임의 감정이 여기 있다 */
    long        fileBytes;

    const Box      *boxes;  int boxCount;
    const TermSpec *terms;  int termCount;
    const MsgSpec  *msgs;   int msgCount;

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
