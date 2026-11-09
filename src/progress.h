/* progress.h — 클리어 기록과 세이브.
 *
 * 파일 IO 라서 sim 에 두지 않는다. sim.c 는 raylib 도 파일도 모른다. */
#ifndef PROGRESS_H
#define PROGRESS_H


#define MAX_STAGES 32

/* 처음 켰을 때 볼륨. 1.0 은 원래 믹스 그대로라 실제로 시끄럽다 */
#define VOL_DEFAULT 0.30f


typedef struct {
    unsigned      unlocked;              /* 해금된 능력 비트마스크 */
    int           reached;               /* 열린 최고 스테이지 인덱스 (0부터) */
    unsigned char cleared[MAX_STAGES];
    unsigned char diskGot[MAX_STAGES];
    float         best[MAX_STAGES];      /* 최고 기록(초). 0 = 기록 없음 */
    int           deaths;                /* 누적 재시도 */

    /* ── 설정 ── 진행이 아니라 설정이지만, 저장 파일이 이거 하나뿐이라 같이 싣는다 */
    float         bgmVol;                /* 0~1 */
    float         sfxVol;                /* 0~1 */
} Progress;

void progress_reset(Progress *p, unsigned startingAbilities);
int  progress_load(Progress *p, const char *path);   /* 1 = 읽음 */
int  progress_save(const Progress *p, const char *path);

/* 클리어 결과를 반영한다. 더 빠른 기록만 갱신한다. */
void progress_record(Progress *p, int stage, int gotDisk, float time, int levelCount);

int  progress_total_cleared(const Progress *p, int levelCount);
int  progress_total_disks(const Progress *p, int levelCount);

#endif
