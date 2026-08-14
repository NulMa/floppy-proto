/* bgm-preview 전용 raylib 껍데기.
 *
 * bgm.c 를 게임/raylib 없이 컴파일해서 곡만 wav 로 뽑기 위한 것이다.
 * cl 의 /I 는 앞에 준 경로를 먼저 뒤지므로, 이 파일이 진짜 raylib.h 를 가린다.
 * 여기 선언된 함수의 몸통은 bgm-preview.c 에 있다. */
#ifndef RAYLIB_STUB_H
#define RAYLIB_STUB_H

typedef struct AudioStream { unsigned sampleRate, sampleSize, channels; } AudioStream;

int  IsAudioDeviceReady(void);
void SetAudioStreamBufferSizeDefault(int size);
AudioStream LoadAudioStream(unsigned sampleRate, unsigned sampleSize, unsigned channels);
void SetAudioStreamVolume(AudioStream s, float volume);
void PlayAudioStream(AudioStream s);
int  IsAudioStreamProcessed(AudioStream s);
void UpdateAudioStream(AudioStream s, const void *data, int frameCount);
void UnloadAudioStream(AudioStream s);

#endif
