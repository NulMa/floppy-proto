/* bgm.h — 8bit 배경음악. **파일을 쓰지 않는다.**
 *
 * 곡을 파형으로 실시간 합성한다. XM 파일을 넣으면 디코더 63KB + 곡 20~80KB 인데,
 * 이쪽은 코드 몇 KB 로 끝나고 라이선스 출처 관리도 필요 없다.
 *
 * 3채널 — 리드(사각파) · 베이스(사각파) · 퍼커션(노이즈).
 * 패턴 시퀀서로 Am - F - C - G 를 돈다. */
#ifndef BGM_H
#define BGM_H

void bgm_init(void);       /* sfx_init 다음에 부른다 (오디오 장치 공유) */
void bgm_update(void);     /* 매 프레임. 버퍼가 비면 다음 구간을 합성해 채운다 */
void bgm_toggle(void);
int  bgm_enabled(void);
void bgm_shutdown(void);

/* 마스터 볼륨 (0~1). 1.0 이 원래 믹스 크기다.
 * bgm_init 전에 불러도 값은 기억했다가 초기화 때 적용한다. */
void  bgm_set_volume(float v);
float bgm_volume(void);

#endif
