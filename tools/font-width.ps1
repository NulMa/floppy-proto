# font-width — 화면에 나가는 문자열이 **몇 픽셀인지** 실제 폰트로 잰다.
#
# 왜 필요한가: 화면이 512px 뿐이라 한 줄이 넘치면 그냥 잘린다. 그런데 한글은
# 전각 16px, 영문은 반각 8px 이라 글자 수로는 감이 안 온다 —
# "[방향키] 이동" 은 6글자인데 104px 이고 "[ENTER] load" 는 12글자인데 96px 이다.
#
# 사용:
#   powershell -File tools\font-width.ps1                  # strings.h 전수 검사
#   powershell -File tools\font-width.ps1 -Text "아무 문장"
#
# 넘치는 줄은 [OVER] 로 표시한다. 기준은 화면 폭 512px 에서 좌우 여백 12px 씩 뺀 488px.

param([string]$Text = "", [int]$Limit = 488)

Add-Type -AssemblyName System.Drawing
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$pfc  = New-Object Drawing.Text.PrivateFontCollection
$pfc.AddFontFile((Join-Path $root "art\font\neodgm.ttf"))
$font = New-Object Drawing.Font($pfc.Families[0], 16, [Drawing.FontStyle]::Regular, [Drawing.GraphicsUnit]::Pixel)
$bmp  = New-Object Drawing.Bitmap -ArgumentList 8, 8, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g    = [Drawing.Graphics]::FromImage($bmp)
$sf   = [Drawing.StringFormat]::GenericTypographic

function Measure-Str([string]$s) {
    # 자간이 반각 8 / 전각 16 으로 딱 떨어져서 글자별로 더하면 된다.
    # (MeasureString 을 통짜로 부르면 커닝이 끼어 1~2px 어긋난다)
    $w = 0
    foreach ($ch in $s.ToCharArray()) {
        $m = $g.MeasureString([string]$ch, $font, [Drawing.PointF]::new(0,0), $sf).Width
        if ($m -le 0) { $m = 8 }        # 공백
        $w += [int][math]::Round($m)
    }
    return $w
}

if ($Text -ne "") {
    "{0,5} px   {1}" -f (Measure-Str $Text), $Text
    exit 0
}

$sh = Join-Path $root "src\strings.h"
$raw = [IO.File]::ReadAllText($sh, [Text.Encoding]::UTF8)
$over = 0
# strings.h 는 `#define KEY "..."` 형태다. (한때 두 언어였다 — 2026-08-19 에 영문으로 되돌렸다)
# 넘치는 줄은 화면에서 그냥 잘리므로 도구가 봐야 한다.
$q = [char]34
$sh = Join-Path $root "src\strings.h"
$raw = [IO.File]::ReadAllText($sh, [Text.Encoding]::UTF8)
$raw = $raw -replace "\\s*\r?\n\s*", " "      # 여러 줄로 접힌 매크로를 한 줄로
$over = 0
$rows = 0
$pat = "#define\s+([SF]_\w+)\s+(?:TR\(\s*)?" + $q + "([^" + $q + "]*)" + $q + "(?:\s*,\s*" + $q + "([^" + $q + "]*)" + $q + "\s*\))?"
foreach ($m in [regex]::Matches($raw, $pat)) {
    $name = $m.Groups[1].Value
    foreach ($pair in @(@("en", $m.Groups[2].Value), @("ko", $m.Groups[3].Value))) {
        if ($pair[1] -eq "") { continue }
        $w = Measure-Str $pair[1]
        $rows++
        $flag = "      "
        if ($w -gt $Limit) { $flag = "[OVER]"; $over++ }
        "{0} {1,5} px  {2,-16} {3}" -f $flag, $w, $name, $pair[1]
    }
}
""
"  {0}줄 검사 · 한계 {1} px (화면 512 - 여백 12x2) · 넘치는 줄 {2}개" -f $rows, $Limit, $over
