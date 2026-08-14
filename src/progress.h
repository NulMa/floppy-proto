/* progress.h — 클리어 기록과 세이브.
 *
 * 파일 IO 라서 sim 에 두지 않는다. sim.c 는 raylib 도 파일도 모른다. */
#ifndef PROGRESS_H
#define PROGRESS_H

#define MAX_STAGES 32

typedef struct {
    unsigned      unlocked;              /* 해금된 능력 비트마스크 */
    int           reached;               /* 열린 최고 스테이지 인덱스 (0부터) */
    unsigned char cleared[MAX_STAGES];
    unsigned char diskGot[MAX_STAGES];
    float         best[MAX_STAGES];      /* 최고 기록(초). 0 = 기록 없음 */
    int           deaths;                /* 누적 재시도 */
} Progress;

void progress_reset(Progress *p, unsigned startingAbilities);
int  progress_load(Progress *p, const char *path);   /* 1 = 읽음 */
int  progress_save(const Progress *p, const char *path);

/* 클리어 결과를 반영한다. 더 빠른 기록만 갱신한다. */
void progress_record(Progress *p, int stage, int gotDisk, float time, int levelCount);

int  progress_total_cleared(const Progress *p, int levelCount);
int  progress_total_disks(const Progress *p, int levelCount);

#endif
