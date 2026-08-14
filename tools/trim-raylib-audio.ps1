# raylib 의 안 쓰는 오디오 디코더를 끈다.
#
# 기본값은 WAV/OGG/MP3/QOA/XM/MOD 가 전부 켜져 있다.
# 우리는 효과음을 절차 생성하고(파일 없음) 음악은 XM 만 쓸 계획이라
# OGG/MP3/QOA/MOD 는 필요 없다.
#
# 실측: 음악(LoadMusicStream)을 쓰면 디코더가 전부 링크되어 +99KB 가 더 붙는다.
#       효과음만 쓸 때는 /OPT:REF 가 알아서 버리므로 차이가 10KB 정도다.
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
$text = $text -replace '(?m)^#define SUPPORT_FILEFORMAT_(OGG|MP3|QOA|MOD)', '//#define SUPPORT_FILEFORMAT_$1'

if ($text -ne $before) {
    [IO.File]::WriteAllText($ConfigPath, $text)
    Write-Host "[trim-raylib-audio] disabled OGG / MP3 / QOA / MOD"
} else {
    Write-Host "[trim-raylib-audio] already trimmed"
}
