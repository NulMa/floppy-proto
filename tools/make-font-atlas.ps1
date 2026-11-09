# make-font-atlas — neoDGM 에서 **실제로 쓰는 글자만** 골라 아틀라스로 굽는다.
#
# 왜 TTF 를 안 넣나:
#   neodgm.ttf 가 651,044 B 인데 예산 여유가 444,416 B 다. 애초에 안 들어간다.
#   게다가 raylib 의 TTF 경로는 stb_truetype 를 끌어와서 고정비가 +38,912 B 다.
#   (실측 2026-08-19 — `게임스펙.md §9`)
#
# 왜 구워도 되나:
#   neoDGM 은 16x16 비트맵 폰트(둥근모꼴)를 TTF 로 옮긴 것이라 **16px 로 찍으면
#   픽셀 격자에 정확히 떨어진다.** 알파값이 0 과 255 두 개뿐이다 — 안티에일리어싱이 없다.
#   즉 구운 결과가 원본 비트맵과 같다. 힌팅으로 잃을 게 없다.
#
# 자간도 딱 떨어진다: **ASCII 8px · 한글 16px · 높이 전부 16px** (반각/전각).
#   그래서 레이아웃 폭이 `8*영문 + 16*한글` 로 계산된다 — MeasureText 없이도 잰다.
#
# ★ 라이선스: neoDGM 은 SIL OFL 1.1 이고 **예약 폰트 이름**이 있다.
#   구운 결과물을 `Neo둥근모`/`NeoDunggeunmo` 로 부르면 안 된다. 고지는 크레딧에 넣는다.
#   자세한 건 `art/font/README.md`.
#
# 사용:
#   powershell -File tools\make-font-atlas.ps1
#
# 입력: src\strings.h 와 src\levels.c 에 든 문자열 리터럴 전부 (UTF-8 로 읽는다)
# 출력: art\font16.png  ·  src\art\font16_png.h  ·  src\art\font16_map.h

param([switch]$Quiet)

Add-Type -AssemblyName System.Drawing

$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$ttf  = Join-Path $root "art\font\neodgm.ttf"
if (-not (Test-Path -LiteralPath $ttf)) {
    Write-Host "[font] neodgm.ttf 가 없다: $ttf"
    exit 1
}

# ── 1. 어떤 글자가 필요한가 ───────────────────────────────────────────
#
# ★ **ASCII 0x20~0x7E 와 몇몇 문장부호뿐이다.** 화면 글자는 전부 영문이다
#   (2026-08-19 에 한국어를 넣었다가 되돌렸다 — `strings.h` 머리 참고).
#   95자에 글리프당 8px 이라 아껴봐야 의미도 없다.
#
# ★ 그래서 **소스를 훑어 한글을 모으지 않는다.** 예전엔 문자열 리터럴을 긁었는데,
#   주석 안의 따옴표까지 딸려 들어와서 화면에 절대 안 나오는 한글 122자가
#   아틀라스에 박혀 있었다 (4.8 KB). 대신 아래에서 **경고만** 한다.
$need = [Collections.Generic.SortedSet[int]]::new()
for ($c = 0x20; $c -le 0x7E; $c++) { [void]$need.Add($c) }
[void]$need.Add(0x00B7)      # · 가운뎃점 — 초기화 확인창이 쓴다

# strings.h 에 비ASCII 가 들어오면 화면에서 두부(?)로 나온다. 굽기 전에 잡는다.
$sh = Join-Path $root "src\strings.h"
if (Test-Path -LiteralPath $sh) {
    $q = [char]34
    $raw = [IO.File]::ReadAllText($sh, [Text.Encoding]::UTF8)
    $body = ($raw -split "`n" | Where-Object { $_ -notmatch "^\s*\*" -and $_ -notmatch "^\s*/\*" }) -join "`n"
    $lits = [regex]::Matches($body, $q + "[^" + $q + [char]92 + "r" + [char]92 + "n]*" + $q)
    $bad = [Collections.Generic.SortedSet[int]]::new()
    foreach ($m in $lits) {
        foreach ($ch in $m.Value.ToCharArray()) {
            $cp = [int]$ch
            if ($cp -gt 0x7E -and -not $need.Contains($cp)) { [void]$bad.Add($cp) }
        }
    }
    if ($bad.Count -gt 0) {
        $chars = -join (@($bad) | ForEach-Object { [char]$_ })
        Write-Host ("  [경고] strings.h 에 아틀라스에 없는 글자가 있다: {0}" -f $chars)
        Write-Host  "         화면에서 ? 로 나온다. 영문으로 바꾸거나 이 목록에 추가할 것."
        foreach ($cp in @($bad)) { [void]$need.Add($cp) }
    }
}
$cps = @($need)
$n   = $cps.Count

# ── 2. 굽는다 ────────────────────────────────────────────────────────
#
# 32칸씩 줄바꿈. 칸은 전각 기준 16x16 이고 반각도 같은 칸에 **왼쪽 정렬**로 넣는다.
# 칸 폭을 글자마다 다르게 하면 좌표 계산이 런타임으로 넘어간다 — 격자가 싸다.
[int]$cols = 32
[int]$rows = [math]::Ceiling($n / $cols)
[int]$cell = 16
[int]$W = $cols * $cell
[int]$H = $rows * $cell

$pfc = New-Object Drawing.Text.PrivateFontCollection
$pfc.AddFontFile($ttf)
$font = New-Object Drawing.Font($pfc.Families[0], 16, [Drawing.FontStyle]::Regular, [Drawing.GraphicsUnit]::Pixel)

$bmp = New-Object Drawing.Bitmap -ArgumentList $W, $H, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g   = [Drawing.Graphics]::FromImage($bmp)
$g.Clear([Drawing.Color]::Transparent)
# ★ 이 한 줄이 전부다. 안 걸면 안티에일리어싱이 붙어서 픽셀아트가 아니게 된다.
$g.TextRenderingHint = [Drawing.Text.TextRenderingHint]::SingleBitPerPixelGridFit
$sf = [Drawing.StringFormat]::GenericTypographic

$adv = New-Object int[] $n
for ($i = 0; $i -lt $n; $i++) {
    $s = [char]::ConvertFromUtf32($cps[$i])
    [single]$x = ($i % $cols) * $cell
    [single]$y = [math]::Floor($i / $cols) * $cell
    $g.DrawString($s, $font, [Drawing.Brushes]::White, $x, $y, $sf)
    $adv[$i] = [int][math]::Round($g.MeasureString($s, $font, [Drawing.PointF]::new(0,0), $sf).Width)
    if ($adv[$i] -le 0) { $adv[$i] = 8 }   # 공백은 0 으로 나온다
}
$g.Dispose()

$pngPath = Join-Path $root "art\font16.png"
$bmp.Save($pngPath, [Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

# ── 3. 코드포인트·자간 표를 헤더로 ──────────────────────────────────
$mapPath = Join-Path $root "src\art\font16_map.h"
$sb = [Text.StringBuilder]::new()
[void]$sb.AppendLine("/* font16_map.h — tools\make-font-atlas.ps1 이 만든다. 손으로 고치지 말 것. */")
[void]$sb.AppendLine("#ifndef FONT16_MAP_H")
[void]$sb.AppendLine("#define FONT16_MAP_H")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("#define FONT16_COUNT $n")
[void]$sb.AppendLine("#define FONT16_COLS  $cols")
[void]$sb.AppendLine("#define FONT16_CELL  $cell")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("static const int FONT16_CP[FONT16_COUNT] = {")
for ($i = 0; $i -lt $n; $i += 12) {
    $slice = @()
    for ($k = $i; $k -lt [math]::Min($i + 12, $n); $k++) { $slice += ("{0,6}" -f $cps[$k]) }
    [void]$sb.AppendLine("    " + ($slice -join ",") + ",")
}
[void]$sb.AppendLine("};")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("static const unsigned char FONT16_ADV[FONT16_COUNT] = {")
for ($i = 0; $i -lt $n; $i += 24) {
    $slice = @()
    for ($k = $i; $k -lt [math]::Min($i + 24, $n); $k++) { $slice += ("{0,3}" -f $adv[$k]) }
    [void]$sb.AppendLine("    " + ($slice -join ",") + ",")
}
[void]$sb.AppendLine("};")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("#endif")
[IO.File]::WriteAllText($mapPath, $sb.ToString(), (New-Object Text.UTF8Encoding $false))

# ── 4. png 를 C 배열로 ───────────────────────────────────────────────
& (Join-Path $PSScriptRoot "png2c.ps1") "art\font16.png" "src\art\font16_png.h" "FONT16_PNG"

$pngSize = (Get-Item $pngPath).Length
$hangul  = ($cps | Where-Object { $_ -ge 0xAC00 -and $_ -le 0xD7A3 }).Count
if (-not $Quiet) {
    Write-Host ""
    Write-Host ("  글리프 {0}개 (한글 {1} · ASCII 95 · 기타 {2})" -f $n, $hangul, ($n - $hangul - 95))
    Write-Host ("  아틀라스 {0}x{1}  ->  art\font16.png  {2:N0} B" -f $W, $H, $pngSize)
    Write-Host ("  글리프당 {0:N1} B" -f ($pngSize / $n))
}
