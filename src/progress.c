#include "progress.h"
#include <stdio.h>
#include <string.h>

/* 파일 포맷 — 앞에 매직과 버전을 둔다.
 * 포맷이 바뀌면 버전을 올리고, 못 읽는 세이브는 조용히 버린다(새로 시작).
 * 공모전 제출물이라 마이그레이션까지 할 이유는 없다. */
#define SAVE_MAGIC   0x314C5746u   /* "FWL1" */
#define SAVE_VERSION 1

typedef struct {
    unsigned      magic;
    unsigned      version;
    unsigned      unlocked;
    int           reached;
    int           deaths;
    unsigned char cleared[MAX_STAGES];
    unsigned char diskGot[MAX_STAGES];
    float         best[MAX_STAGES];
} SaveBlob;

void progress_reset(Progress *p, unsigned startingAbilities)
{
    memset(p, 0, sizeof(*p));
    p->unlocked = startingAbilities;
    p->reached  = 0;
}

int progress_load(Progress *p, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    SaveBlob b;
    size_t n = fread(&b, 1, sizeof(b), f);
    fclose(f);
    if (n != sizeof(b) || b.magic != SAVE_MAGIC || b.version != SAVE_VERSION) return 0;

    p->unlocked = b.unlocked;
    p->reached  = (b.reached < 0) ? 0 : (b.reached >= MAX_STAGES ? MAX_STAGES - 1 : b.reached);
    p->deaths   = b.deaths;
    memcpy(p->cleared, b.cleared, sizeof(p->cleared));
    memcpy(p->diskGot, b.diskGot, sizeof(p->diskGot));
    memcpy(p->best,    b.best,    sizeof(p->best));
    return 1;
}

int progress_save(const Progress *p, const char *path)
{
    SaveBlob b;
    memset(&b, 0, sizeof(b));
    b.magic = SAVE_MAGIC;
    b.version = SAVE_VERSION;
    b.unlocked = p->unlocked;
    b.reached = p->reached;
    b.deaths = p->deaths;
    memcpy(b.cleared, p->cleared, sizeof(b.cleared));
    memcpy(b.diskGot, p->diskGot, sizeof(b.diskGot));
    memcpy(b.best,    p->best,    sizeof(b.best));

    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    size_t n = fwrite(&b, 1, sizeof(b), f);
    fclose(f);
    return n == sizeof(b);
}

void progress_record(Progress *p, int stage, int gotDisk, float time, int levelCount)
{
    if (stage < 0 || stage >= MAX_STAGES) return;

    p->cleared[stage] = 1;
    if (gotDisk) p->diskGot[stage] = 1;
    /* 더 빠른 기록만 갱신 */
    if (p->best[stage] <= 0.0f || time < p->best[stage]) p->best[stage] = time;

    /* 다음 스테이지를 연다 */
    if (stage + 1 < levelCount && stage + 1 > p->reached) p->reached = stage + 1;
}

int progress_total_cleared(const Progress *p, int levelCount)
{
    int n = 0;
    for (int i = 0; i < levelCount && i < MAX_STAGES; i++) if (p->cleared[i]) n++;
    return n;
}

int progress_total_disks(const Progress *p, int levelCount)
{
    int n = 0;
    for (int i = 0; i < levelCount && i < MAX_STAGES; i++) if (p->diskGot[i]) n++;
    return n;
}
