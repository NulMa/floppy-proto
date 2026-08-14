#include "bgm.h"
#include "raylib.h"
#include <math.h>
#include <string.h>

#define SR        22050
#define BUFN      2048          /* 한 번에 채우는 샘플 수 */
#define BPM       100
#define STEPS     16            /* 패턴 하나가 16스텝 (16분음표) */
#define PATTERNS  8

/* 스텝 하나의 길이(샘플). 16분음표 = 60 / BPM / 4 초 */
#define STEP_SAMPLES (SR * 60 / (BPM * 4))

/* ── 곡 ──────────────────────────────────────────────────────────────
 *
 * A 단조. i - VI - III - VII (Am - F - C - G) 를 두 번 돈다.
 * 숫자는 MIDI 노트, -1 은 쉼표. 죽어가는 기계에 어울리게 단조로 깐다.
 *
 * ★ 패턴 4개(9.6초)만 돌리면 20스테이지를 하는 동안 지겹다.
 *   그래서 A(0~3) / B(4~7) 두 부분으로 나눴다 — 화성은 같고 리드의
 *   음역과 리듬이 다르다. 한 바퀴 19.2초.
 *
 *   A: 코드 톤을 느리게 오르내린다. 조용한 쪽.
 *   B: 한 옥타브 위에서 8분음표로 몰아친다. 쫓기는 느낌 쪽. */
static const signed char LEAD[PATTERNS][STEPS] = {
    /* ── A ── */
    { 69, -1, -1, 72,  -1, 76, -1, -1,   72, -1, 69, -1,  -1, -1, 67, -1 },  /* Am */
    { 65, -1, -1, 69,  -1, 72, -1, -1,   69, -1, 65, -1,  -1, -1, 64, -1 },  /* F  */
    { 72, -1, -1, 76,  -1, 79, -1, -1,   76, -1, 72, -1,  -1, -1, 71, -1 },  /* C  */
    { 67, -1, -1, 71,  -1, 74, -1, -1,   71, -1, 67, -1,  -1, -1, 69, -1 },  /* G  */
    /* ── B ── 한 옥타브 위, 8분음표 */
    { 81, -1, 84, -1,  88, -1, 84, -1,   81, -1, 79, -1,  81, -1, -1, -1 },  /* Am */
    { 77, -1, 81, -1,  84, -1, 81, -1,   77, -1, 76, -1,  77, -1, -1, -1 },  /* F  */
    { 84, -1, 88, -1,  91, -1, 88, -1,   84, -1, 83, -1,  84, -1, -1, -1 },  /* C  */
    { 79, -1, 83, -1,  86, -1, 83, -1,   79, -1, 78, -1,  79, -1, 81, -1 },  /* G  */
};

/* 베이스는 근음만. 4분음표로 바닥을 깐다. B 에서는 8분음표로 조인다 */
static const signed char BASS[PATTERNS][STEPS] = {
    { 45, -1, -1, -1,  45, -1, -1, -1,   45, -1, -1, -1,  45, -1, -1, -1 },
    { 41, -1, -1, -1,  41, -1, -1, -1,   41, -1, -1, -1,  41, -1, -1, -1 },
    { 48, -1, -1, -1,  48, -1, -1, -1,   48, -1, -1, -1,  48, -1, -1, -1 },
    { 43, -1, -1, -1,  43, -1, -1, -1,   43, -1, -1, -1,  43, -1, -1, -1 },
    { 45, -1, 45, -1,  45, -1, 45, -1,   45, -1, 45, -1,  45, -1, 52, -1 },
    { 41, -1, 41, -1,  41, -1, 41, -1,   41, -1, 41, -1,  41, -1, 48, -1 },
    { 48, -1, 48, -1,  48, -1, 48, -1,   48, -1, 48, -1,  48, -1, 55, -1 },
    { 43, -1, 43, -1,  43, -1, 43, -1,   43, -1, 43, -1,  43, -1, 45, -1 },
};

/* 0 없음 / 1 킥 / 2 하이햇.  B 는 여덟째마다 킥을 더 넣어 밀어붙인다 */
static const unsigned char PERC[2][STEPS] = {
    { 1, 0, 2, 0,  0, 0, 2, 0,   1, 0, 2, 0,  0, 0, 2, 2 },   /* A */
    { 1, 0, 2, 0,  1, 0, 2, 0,   1, 0, 2, 0,  1, 0, 2, 2 },   /* B */
};

/* ── 상태 ───────────────────────────────────────────────────────── */
static AudioStream g_stream;
static short g_buf[BUFN];
static int   g_ready = 0, g_on = 1;

static int   g_pat = 0, g_step = 0, g_stepPos = 0;
static float g_leadPh, g_bassPh;
static float g_leadF, g_bassF;
static float g_leadEnv, g_bassEnv, g_percEnv;
static float g_percDec, g_percPh, g_percF;
static int   g_percKind;

static unsigned g_rng = 0x2468ACEu;
static float noise(void)
{
    g_rng = g_rng * 1664525u + 1013904223u;
    return ((float)((g_rng >> 9) & 0x7FFF) / 16383.5f) - 1.0f;
}

static float note_hz(int midi) { return 440.0f * powf(2.0f, (float)(midi - 69) / 12.0f); }

static void trigger_step(void)
{
    signed char ln = LEAD[g_pat][g_step];
    signed char bn = BASS[g_pat][g_step];
    unsigned char pc = PERC[g_pat >= 4][g_step];

    if (ln >= 0) { g_leadF = note_hz(ln); g_leadEnv = 1.0f; }
    if (bn >= 0) { g_bassF = note_hz(bn); g_bassEnv = 1.0f; }
    if (pc) {
        g_percEnv = 1.0f;
        g_percKind = pc;
        /* ★ 킥은 노이즈가 아니라 **급강하하는 삼각파**다.
         *   노이즈로 만들면 킥이 아니라 스네어처럼 들린다. */
        if (pc == 1) { g_percF = 150.0f; g_percPh = 0.0f; g_percDec = 0.99930f; }
        else         { g_percDec = 0.99845f; }   /* 하이햇은 아주 짧게 */
    }
}

static void fill(short *out, int n)
{
    /* 감쇠는 샘플당 계수로 미리 계산. exp 를 매 샘플 부르면 낭비다 */
    const float leadDec = 0.99986f;   /* 리드는 길게 끈다 */
    const float bassDec = 0.99990f;

    for (int i = 0; i < n; i++) {
        if (g_stepPos == 0) trigger_step();

        /* 리드 — 사각파, 듀티 25% 로 얇게 */
        g_leadPh += g_leadF / (float)SR;
        if (g_leadPh >= 1.0f) g_leadPh -= 1.0f;
        float lead = (g_leadPh < 0.25f ? 1.0f : -1.0f) * g_leadEnv;

        /* 베이스 — 사각파, 듀티 50% */
        g_bassPh += g_bassF / (float)SR;
        if (g_bassPh >= 1.0f) g_bassPh -= 1.0f;
        float bass = (g_bassPh < 0.5f ? 1.0f : -1.0f) * g_bassEnv;

        /* 퍼커션 — 킥은 150→45Hz 로 떨어지는 삼각파, 하이햇은 노이즈 */
        float perc;
        if (g_percKind == 1) {
            g_percF += (45.0f - g_percF) * 0.0016f;   /* 지수적으로 낮아진다 */
            g_percPh += g_percF / (float)SR;
            if (g_percPh >= 1.0f) g_percPh -= 1.0f;
            float tri = 4.0f * fabsf(g_percPh - 0.5f) - 1.0f;
            perc = tri * g_percEnv * 1.2f;
        } else {
            perc = noise() * g_percEnv * 0.5f;
        }

        g_leadEnv *= leadDec;
        g_bassEnv *= bassDec;
        g_percEnv *= g_percDec;

        float v = lead * 0.20f + bass * 0.22f + perc * 0.16f;
        if (v >  1.0f) v =  1.0f;
        if (v < -1.0f) v = -1.0f;
        out[i] = (short)(v * 26000.0f);

        if (++g_stepPos >= STEP_SAMPLES) {
            g_stepPos = 0;
            if (++g_step >= STEPS) {
                g_step = 0;
                if (++g_pat >= PATTERNS) g_pat = 0;   /* 무한 반복 */
            }
        }
    }
}

void bgm_init(void)
{
    if (g_ready) return;
    if (!IsAudioDeviceReady()) return;    /* sfx_init 이 장치를 못 열었으면 조용히 포기 */

    SetAudioStreamBufferSizeDefault(BUFN);
    g_stream = LoadAudioStream(SR, 16, 1);
    SetAudioStreamVolume(g_stream, 0.55f);

    g_leadF = note_hz(69);
    g_bassF = note_hz(45);
    g_percDec = 0.999f;
    PlayAudioStream(g_stream);
    g_ready = 1;
}

void bgm_update(void)
{
    if (!g_ready) return;
    /* 버퍼가 비는 만큼만 채운다. 한 프레임에 여러 번 빌 수도 있다 */
    while (IsAudioStreamProcessed(g_stream)) {
        if (g_on) fill(g_buf, BUFN);
        else      memset(g_buf, 0, sizeof(g_buf));
        UpdateAudioStream(g_stream, g_buf, BUFN);
    }
}

void bgm_toggle(void) { g_on = !g_on; }
int  bgm_enabled(void) { return g_ready && g_on; }

void bgm_shutdown(void)
{
    if (!g_ready) return;
    UnloadAudioStream(g_stream);
    g_ready = 0;
}
