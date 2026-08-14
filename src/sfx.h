/* sfx.h — 효과음. **파일을 쓰지 않는다.**
 *
 * 전부 시작할 때 파형으로 합성한다. 이유 둘:
 *   ① 용량 — wav 를 넣으면 개당 수십 KB 다. 파라미터는 개당 수십 바이트
 *   ② 라이선스 — 외부 에셋을 안 쓰면 출처 관리가 필요 없다
 *
 * 오디오 링크 비용 실측 (2026-08-15):
 *   오디오 없음        752,640 B
 *   + SFX (이 파일)    930,304 B   (+174 KB — miniaudio 백엔드가 대부분)
 *   + XM 음악          994,816 B   (+63 KB — jar_xm 디코더)
 */
#ifndef SFX_H
#define SFX_H

typedef enum {
    SFX_JUMP = 0,
    SFX_JUMP2,      /* 2단 점프 — 한 옥타브 위 */
    SFX_DASH,
    SFX_WALL,       /* 벽 점프 */
    SFX_LAND,
    SFX_PAD,        /* 점프패드 */
    SFX_HURT,       /* 가시 */
    SFX_ERASE,      /* 파동에 지워짐 */
    SFX_DISK,       /* 디스크 회수 */
    SFX_EQUIP,      /* 유틸리티 장착 */
    SFX_DENY,
    SFX_CHECK,      /* 터미널 도달 */
    SFX_EXIT,       /* 클리어 */
    SFX_COUNT
} SfxId;

void sfx_init(void);
void sfx_play(SfxId id);
void sfx_shutdown(void);

#endif
