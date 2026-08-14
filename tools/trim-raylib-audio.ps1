# raylib 의 안 쓰는 오디오 디코더를 끈다.
#
# 기본값은 WAV/OGG/MP3/QOA/XM/MOD 가 전부 켜져 있다.
# 우리는 **오디오 파일을 하나도 안 읽는다** — 효과음은 sfx.c 가 파형을 합성하고
# 음악은 bgm.c 가 AudioStream 에 직접 밀어넣는다. 그래서 디코더가 전부 필요 없다.
# (XM 도 껐다. 한때 음악을 XM 으로 넣을 계획이었으나 폐기했다 → D-20)
#
# 실측: 음악(LoadMusicStream)을 쓰면 디코더가 전부 링크되어 +99KB 가 더 붙는다.
#       우리처럼 안 쓰면 /OPT:REF 가 알아서 버린다 — XM 을 추가로 꺼봤지만
#       exe 크기가 936,960 B 그대로였다 (2026-08-15 실측, 차이 0 B).
#       그래도 유지한다. 링커의 판단에 기대는 것보다 애초에 컴파일을 안 하는 쪽이
#       확실하고, 나중에 실수로 LoadMusicStream 을 부르면 여기서 바로 막힌다.
#
# vendor 는 gitignore 라서 fetch-deps 를 다시 돌리면 원복된다.
# 그래서 build-raylib.bat 이 매번 이 스크립트를 부른다 (멱등).

param([string]$ConfigPath)

if (-not (Test-Path -LiteralPath $ConfigPath)) {
    Write-Host "[trim-raylib-audio] config.h not found: $ConfigPath"
    exit 1
}

$text = [IO.File]::ReadAllText($ConfigPath)
$before = $text
$text = $text -replace '(?m)^#define SUPPORT_FILEFORMAT_(OGG|MP3|QOA|MOD|XM)', '//#define SUPPORT_FILEFORMAT_$1'

if ($text -ne $before) {
    [IO.File]::WriteAllText($ConfigPath, $text)
    Write-Host "[trim-raylib-audio] disabled OGG / MP3 / QOA / MOD / XM"
} else {
    Write-Host "[trim-raylib-audio] already trimmed"
}
