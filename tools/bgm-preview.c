/* bgm-preview — BGM 을 wav 로 뽑는다.
 *
 * 왜 필요한가: 곡이 마음에 드는지 판단하려고 게임을 켜고 스테이지를 도는 건
 * 낭비다. 여기서 뽑은 wav 를 바로 들어보고 bgm.c 의 패턴 표를 고치면 된다.
 * 이 wav 는 **빌드에 들어가지 않는다.** 게임은 여전히 런타임 합성이다.
 *
 *   tools\bgm-preview.bat [초]     기본 40초 → build\bgm-preview.wav
 *
 * bgm.c 의 합성부를 그대로 쓴다 (tools\stub\raylib.h 가 raylib 를 대신한다).
 * 곡을 두 벌 유지하면 반드시 어긋나므로 절대 복사해오지 않는다. */
#include <stdio.h>
#include <stdlib.h>
#include "raylib.h"   /* ← tools\stub 의 껍데기가 잡힌다 */
#include "bgm.h"

#define SR 22050

static FILE *g_out;
static long  g_frames, g_want;

/* ── raylib 껍데기 구현 ────────────────────────────────────────── */
int  IsAudioDeviceReady(void) { return 1; }
void SetAudioStreamBufferSizeDefault(int size) { (void)size; }
AudioStream LoadAudioStream(unsigned sr, unsigned ss, unsigned ch)
{ AudioStream s = { sr, ss, ch }; return s; }
void SetAudioStreamVolume(AudioStream s, float v) { (void)s; (void)v; }
void PlayAudioStream(AudioStream s) { (void)s; }
void UnloadAudioStream(AudioStream s) { (void)s; }

/* 아직 목표 길이를 못 채웠으면 "버퍼가 비었다"고 답한다 */
int IsAudioStreamProcessed(AudioStream s) { (void)s; return g_frames < g_want; }

void UpdateAudioStream(AudioStream s, const void *data, int frameCount)
{
    (void)s;
    fwrite(data, 2, (size_t)frameCount, g_out);
    g_frames += frameCount;
}

/* ── wav ──────────────────────────────────────────────────────── */
static void put32(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f);
                                         fputc((v >> 16) & 255, f); fputc((v >> 24) & 255, f); }
static void put16(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }

static void write_header(FILE *f, long dataBytes)
{
    fwrite("RIFF", 1, 4, f); put32(f, (unsigned)(36 + dataBytes)); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); put32(f, 16); put16(f, 1); put16(f, 1);
    put32(f, SR); put32(f, SR * 2); put16(f, 2); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, (unsigned)dataBytes);
}

int main(int argc, char **argv)
{
    const char *path = "build\\bgm-preview.wav";
    double secs = (argc > 1) ? atof(argv[1]) : 40.0;
    if (secs < 1.0)   secs = 1.0;
    if (secs > 600.0) secs = 600.0;
    g_want = (long)(secs * SR);

    g_out = fopen(path, "wb");
    if (!g_out) { printf("cannot write %s\n", path); return 1; }
    write_header(g_out, g_want * 2);   /* 자리만 잡아두고 끝에 다시 쓴다 */

    bgm_init();
    while (g_frames < g_want) bgm_update();
    bgm_shutdown();

    long dataBytes = g_frames * 2;
    fseek(g_out, 0, SEEK_SET);
    write_header(g_out, dataBytes);
    fclose(g_out);

    printf("  %s   %.1fs   %ld bytes\n", path, (double)g_frames / SR, dataBytes + 44);
    return 0;
}
