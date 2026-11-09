/* strings.h — 화면에 나가는 모든 문장.
 *
 * 왜 한 파일에 모으나: `tools/make-font-atlas.ps1` 이 여기를 읽어서
 * **실제로 쓰는 글자만** 굽는다. 문장이 코드에 흩어져 있으면 아틀라스에 뭐가
 * 들어가야 하는지 알 방법이 없다.
 *
 * ── 언어 ────────────────────────────────────────────────────────────
 *
 * ★ **영문 하나다.** 2026-08-19 에 한국어를 넣었다가 되돌렸다 —
 *   문구의 어감이 전체적으로 안 맞았고, 이 게임의 화면은 애초에
 *   **도스 화면**이라는 설정이다 (`세계관.md §4`). 파일명·용량 표기·쓰기 보호
 *   라벨이 전부 영문인데 안내문만 한국어면 두 세계가 섞인다.
 *
 *   되돌리면서 남긴 것 / 버린 것:
 *     남김   neoDGM 16px 폰트 (도스 비트맵 폰트라 이 게임에 맞는다)
 *            그에 맞춰 다시 잰 레이아웃 (`폰트_레이아웃.md` 에 기록)
 *     버림   `TR()` 매크로 · `lang.h` · `[F1]` 전환 · 한국어 문자열 전부
 *
 * ── 폭 ─────────────────────────────────────────────────────────────
 *
 * neoDGM 16px 은 반각 8px 로 딱 떨어진다. 화면이 512px 이므로
 * 한 줄 한계가 **64자**다. `tools/font-width.ps1` 이 전수 검사한다.
 */
#ifndef STRINGS_H
#define STRINGS_H

/* ── 타이틀 ── */
#define S_TITLE_FREE    "1,474,560 bytes free"
#define S_TITLE_ENTER   "[ENTER]  insert disk"
#define S_TITLE_VOLKEYS "[ [ ] ] BGM   [ - = ] SFX   [M] mute"
#define S_TITLE_TAG     "a floppy has one job left"

/* ── 섹터 선택 ── */
#define S_MENU_TITLE    "SELECT SECTOR"
#define S_MENU_LOCKED   "LOCKED"
#define S_MENU_NEW      "NEW"
#define S_MENU_KEYS     "[ARROWS] select   [ENTER] load   [ESC] title   [DEL] erase"
#define S_TIER_EASY     "EASY"
#define S_TIER_MEDIUM   "MEDIUM"
#define S_TIER_HARD     "HARD"

/* ── 기록 초기화 확인 ──
 *
 * ★ 되돌릴 수 없는 동작이라 **무엇이 지워지는지 나열한다.** "정말입니까?"만
 *   띄우면 사람은 무엇을 잃는지 모르는 채로 누른다. */
#define S_ERASE_TITLE   "ERASE ALL PROGRESS?"
#define S_ERASE_WHAT    "cleared sectors  ·  best times  ·  unlocked utilities"
#define S_ERASE_WARN    "this cannot be undone"
#define S_ERASE_KEYS    "[DEL] erase        [ESC] cancel"
#define S_TOAST_ERASED  "progress erased"

/* ── 사망 ── */
#define S_DEATH_SPIKE1  "SECTOR DAMAGED"
#define S_DEATH_SPIKE2  "you touched the surface"
#define S_DEATH_FMT1    "OVERWRITTEN"
#define S_DEATH_FMT2    "the format caught up"
#define S_DEATH_FALL1   "LOST"
#define S_DEATH_FALL2   "nothing down there"
#define S_DEATH_ANYKEY  "press any key"

/* ── 인게임 HUD ── */
#define S_HUD_REQUIRED  "REQUIRED"
#define S_HUD_KEYS      "[R] retry"
#define S_HUD_HALTED    "format halted at this sector"
#define S_HUD_ABSENT    "absent"

/* 거절 사유 — 사유마다 플레이어가 할 일이 다르다. 뭉뚱그리면 안 된다. */
#define S_DENY_ABSENT   "not on this sector"
#define S_DENY_NOSPACE  "no space"
#define S_DENY_LOCKED   "recover the disk first"
#define S_DENY_TERMINAL "terminals only"

/* ── 클리어 ── */
#define S_CLEAR_TITLE   "SECTOR CLEARED"
#define S_CLEAR_HELD    "the disk contained a utility"
#define S_CLEAR_MISSED  "it held a utility you did not get"
#define S_CLEAR_BACK    "[R]  go back for it"
#define S_CLEAR_NODISK  "no disk on this sector"
#define S_CLEAR_NEXT    "[ENTER]  next sector"

/* ── 엔딩 ── */
#define S_END_EJECTED   "disk ejected."
#define S_END_BACK      "[ENTER]  back to sectors"

/* ── 포맷 문자열 ── */
#define F_HUD_TIME      "%.1fs  RETRIES %d"
#define F_HUD_GOT       "%s  recovered"
#define F_MENU_STATS    "%d/%d cleared   %d disks   %d retries"
#define F_CLEAR_GOT     "RECOVERED  %s   %s bytes"
#define F_CLEAR_ABILITY "[%d]  + %s     %d slots"
#define F_CLEAR_LEFT    "%s  LEFT BEHIND"
#define F_CLEAR_TIME    "%.1f seconds     %d retries"
#define F_END_FILES     "%d of %d files recovered"
#define F_END_BYTES     "%s bytes carried out"
#define F_END_TIME      "%.1f seconds"
#define F_END_RETRY     "%d retries"
#define F_DENY_SLOTS    "%s needs %d slots  -  drop utilities"

#endif
