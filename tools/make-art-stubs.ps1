# make-art-stubs — art/ 에 빈 작업 파일을 만든다.
#
# 각 파일은 그 에셋이 가질 **최대 크기**의 검은 사각형이다.
# 캔버스 크기를 먼저 못 박아두려는 것 — 다 그린 뒤에 크기가 틀린 걸 알면 다시 그려야 한다.
#
# 여러 프레임을 쓰는 에셋은 가로로 이어붙인 크기다. (프레임수 x 칸너비) x 칸높이
# 예) floppy.png = 13프레임 x 16 = 208 x 16
#
# 이미 있는 파일은 건드리지 않는다. 작업 중인 그림을 덮어쓰면 안 되므로.
# 다시 만들려면 -Force.

param([switch]$Force)

Add-Type -AssemblyName System.Drawing

$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$dir  = Join-Path $root "art"
if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }

# 이름, 가로, 세로, 칸너비, 칸높이, 프레임수, 설명
$assets = @(
    @("floppy",      208,  16, 16, 16, 13, "플레이어 - 대기2 달리기3 상승 하강 대시 활공2 벽붙기 터미널대기2"),
    @("ground_top",   48,  16, 16, 16,  3, "지형 윗면 - 왼끝 가운데 오른끝"),
    @("ground_fill",  16,  16, 16, 16,  1, "지형 속 - 타일링"),
    @("spike",        32,  16, 16, 16,  2, "가시 - 켜짐 꺼짐. 아래 8px 만 쓰고 위는 투명"),
    @("crumble",      32,  16, 16, 16,  2, "무너지는 발판 - 금감 무너지는중"),
    @("oneway",       16,  16, 16, 16,  1, "원웨이 선반 - 위 6px 만. 벽으로 보이면 안 된다"),
    @("pad",          32,  16, 16, 16,  2, "점프패드 - 눌림 튐"),
    @("bg_tile",      16,  16, 16, 16,  1, "배경 격자 - 타일링"),
    @("terminal",     32,  32, 16, 32,  2, "터미널 - 꺼짐 켜짐. 세로 2타일"),
    @("disk",         16,  16, 16, 16,  1, "디스크 - 둥둥 뜨는 건 코드가 한다"),
    @("exit",         32,  16, 16, 16,  2, "드라이브 슬롯 - 닫힘 빛남"),
    @("icons",        40,   8,  8,  8,  5, "능력 아이콘 - boot dash glide grip jump2"),
    @("logo",        240,  56,240, 56,  1, "타이틀 로고 LAST WRITE"),
    @("ending",      512, 288,512,288,  1, "엔딩 일러스트 - 화면 전체")
)

$made = 0; $kept = 0
foreach ($a in $assets) {
    $path = Join-Path $dir ($a[0] + ".png")
    if ((Test-Path -LiteralPath $path) -and -not $Force) {
        Write-Host ("  keep   {0,-14} (이미 있음)" -f ($a[0] + ".png"))
        $kept++
        continue
    }

    $bmp = New-Object Drawing.Bitmap([int]$a[1], [int]$a[2], [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g   = [Drawing.Graphics]::FromImage($bmp)
    $g.Clear([Drawing.Color]::FromArgb(255, 0, 0, 0))
    $g.Dispose()
    $bmp.Save($path, [Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()

    Write-Host ("  create {0,-14} {1,4} x {2,-4}  {3,2} frame(s) of {4}x{5}   {6}" -f `
        ($a[0] + ".png"), $a[1], $a[2], $a[5], $a[3], $a[4], $a[6])
    $made++
}

Write-Host ""
Write-Host ("  art\  -  {0} created, {1} kept" -f $made, $kept)
