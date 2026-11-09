#include "sfx.h"
#include "raylib.h"
#include <math.h>
#include <stdlib.h>

/* 22050Hz 모노 16bit. 이 게임 소리에 그 이상은 낭비다. */
#define SR 22050

enum { W_SQUARE = 0, W_SAW, W_NOISE, W_TRI };

/* sfxr 을 최소한으로 줄인 것. 이 몇 개 값이 소리 하나다. */
typedef struct {
    float f0, f1;    /* 시작 / 끝 주파수 (Hz) */
    float dur;       /* 길이 (초) */
    int   wave;
    float duty;      /* 사각파 듀티 (0~1) */
    float decay;     /* 진폭 감쇠 지수. 클수록 빨리 죽는다 */
    float vol;
} SfxSpec;

/* 값들은 귀로 맞춘 게 아니라 규칙으로 잡았다:
 *   상승 = 좋은 일 / 하강 = 나쁜 일 / 노이즈 = 물리적 충격 */
static const SfxSpec SPEC[SFX_COUNT] = {
    /*             f0     f1    dur   wave      duty  decay  vol  */
    [SFX_JUMP]  = { 320,  620, 0.11f, W_SQUARE, 0.50f,  7.0f, 0.30f },
    [SFX_JUMP2] = { 480,  900, 0.10f, W_SQUARE, 0.25f,  8.0f, 0.28f },
    [SFX_DASH]  = { 900,  260, 0.13f, W_NOISE,  0.50f,  9.0f, 0.22f },
    [SFX_WALL]  = { 260,  520, 0.09f, W_SAW,    0.50f,  9.0f, 0.26f },
    [SFX_LAND]  = { 180,   90, 0.06f, W_NOISE,  0.50f, 16.0f, 0.16f },
    [SFX_PAD]   = { 300, 1100, 0.20f, W_TRI,    0.50f,  4.0f, 0.34f },
    [SFX_HURT]  = { 420,   70, 0.28f, W_SAW,    0.50f,  5.0f, 0.34f },
    [SFX_ERASE] = { 220,   40, 0.40f, W_NOISE,  0.50f,  3.0f, 0.32f },
    [SFX_DISK]  = { 700, 1400, 0.24f, W_SQUARE, 0.50f,  4.0f, 0.28f },
    [SFX_EQUIP] = { 520,  780, 0.08f, W_SQUARE, 0.35f, 10.0f, 0.24f },
    [SFX_DENY]  = { 200,  150, 0.12f, W_SQUARE, 0.50f,  9.0f, 0.24f },
    [SFX_CHECK] = { 440,  660, 0.26f, W_TRI,    0.50f,  4.0f, 0.30f },
    [SFX_EXIT]  = { 520, 1040, 0.45f, W_TRI,    0.50f,  2.5f, 0.34f },
};

static Sound g_snd[SFX_COUNT];
static int   g_ready = 0;
static float g_vol   = 1.0f;   /* 마스터 볼륨. SPEC 의 vol 위에 곱해진다 */

static unsigned g_rng = 0x13579BDFu;
static float noise(void)
{
    g_rng = g_rng * 1664525u + 1013904223u;
    return ((float)((g_rng >> 9) & 0x7FFF) / 16383.5f) - 1.0f;
}

static Sound synth(const SfxSpec *s)
{
    int n = (int)(s->dur * SR);
    if (n < 8) n = 8;

    short *pcm = (short *)MemAlloc((unsigned)n * sizeof(short));
    float phase = 0.0f;

    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;                 /* 0 → 1 */
        float f = s->f0 + (s->f1 - s->f0) * t;         /* 주파수 스윕 */
        phase += f / (float)SR;
        if (phase >= 1.0f) phase -= 1.0f;

        float v;
        switch (s->wave) {
            case W_SAW:   v = phase * 2.0f - 1.0f; break;
            case W_NOISE: v = noise(); break;
            case W_TRI:   v = (phase < 0.5f) ? (phase * 4.0f - 1.0f) : (3.0f - phase * 4.0f); break;
            default:      v = (phase < s->duty) ? 1.0f : -1.0f; break;
        }

        float env = expf(-s->decay * t);
        /* 끝을 짧게 페이드 — 안 하면 뚝 끊기며 딱 소리가 난다 */
        if (t > 0.92f) env *= (1.0f - t) / 0.08f;

        float a = v * env * s->vol;
        if (a >  1.0f) a =  1.0f;
        if (a < -1.0f) a = -1.0f;
        pcm[i] = (short)(a * 32000.0f);
    }

    Wave w = { 0 };
    w.frameCount = (unsigned)n;
    w.sampleRate = SR;
    w.sampleSize = 16;
    w.channels   = 1;
    w.data       = pcm;

    Sound out = LoadSoundFromWave(w);
    MemFree(pcm);
    return out;
}

void sfx_init(void)
{
    if (g_ready) return;
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;    /* 사운드 장치가 없어도 게임은 돌아야 한다 */
    for (int i = 0; i < SFX_COUNT; i++) g_snd[i] = synth(&SPEC[i]);
    g_ready = 1;
    sfx_set_volume(g_vol);    /* init 전에 세이브에서 읽은 값이 있으면 여기서 반영된다 */
}

void sfx_set_volume(float v)
{
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    g_vol = v;
    if (!g_ready) return;     /* 값만 남긴다 — sfx_init 이 적용한다 */
    for (int i = 0; i < SFX_COUNT; i++) SetSoundVolume(g_snd[i], g_vol);
}

float sfx_volume(void) { return g_vol; }

void sfx_play(SfxId id)
{
    if (!g_ready || id < 0 || id >= SFX_COUNT) return;
    PlaySound(g_snd[id]);
}

void sfx_shutdown(void)
{
    if (!g_ready) return;
    for (int i = 0; i < SFX_COUNT; i++) UnloadSound(g_snd[i]);
    CloseAudioDevice();
    g_ready = 0;
}
