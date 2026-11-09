# make-icon — art/floppy.png 의 0번 프레임(대기)으로 실행 파일 아이콘을 만든다.
#
# 왜 도구로 두는가: floppy.png 을 다시 그리면 아이콘도 같이 낡는다.
#   원본이 하나여야 어긋나지 않는다 — png2c.ps1 과 같은 이유다.
#
# 16x16 을 정수배로만 키운다 (x1 x2 x3 x4 x8 x16). 보간을 쓰면 도트가 뭉개진다.
# ico 안에는 png 를 그대로 넣는다 — Vista 이상이 png 엔트리를 읽는다.
#
# 사용:
#   powershell -File tools\make-icon.ps1

param(
    [string]$In    = "art\floppy.png",
    [string]$Out   = "art\icon.ico",
    [int]   $Frame = 0,
    [int]   $Cell  = 16
)

Add-Type -AssemblyName System.Drawing

# .NET 호출은 Set-Location 을 무시한다. 반드시 절대 경로로 바꿔서 넘긴다.
$inPath  = [IO.Path]::GetFullPath((Join-Path (Get-Location) $In))
$outPath = [IO.Path]::GetFullPath((Join-Path (Get-Location) $Out))

if (-not (Test-Path -LiteralPath $inPath)) {
    Write-Host "[make-icon] not found: $inPath"
    exit 1
}

$src = New-Object Drawing.Bitmap $inPath
if (($Frame + 1) * $Cell -gt $src.Width) {
    Write-Host "[make-icon] frame $Frame is out of range (width $($src.Width))"
    $src.Dispose(); exit 1
}

# 프레임 한 칸만 잘라낸다
$cellBmp = New-Object Drawing.Bitmap $Cell, $Cell
$g = [Drawing.Graphics]::FromImage($cellBmp)
$g.DrawImage($src, (New-Object Drawing.Rectangle 0, 0, $Cell, $Cell),
                   (New-Object Drawing.Rectangle ($Frame * $Cell), 0, $Cell, $Cell),
                   [Drawing.GraphicsUnit]::Pixel)
$g.Dispose()
$src.Dispose()

# 윈도우가 실제로 골라 쓰는 크기들. 전부 16 의 정수배라 도트가 안 깨진다
$sizes = 16, 32, 48, 64, 128, 256
$blobs = @()
foreach ($s in $sizes) {
    $big = New-Object Drawing.Bitmap $s, $s
    $gb  = [Drawing.Graphics]::FromImage($big)
    $gb.InterpolationMode = 'NearestNeighbor'
    $gb.PixelOffsetMode   = 'Half'
    $gb.DrawImage($cellBmp, 0, 0, $s, $s)
    $gb.Dispose()

    $ms = New-Object IO.MemoryStream
    $big.Save($ms, [Drawing.Imaging.ImageFormat]::Png)
    $blobs += ,($ms.ToArray())
    $ms.Dispose(); $big.Dispose()
}
$cellBmp.Dispose()

# ── ico 조립 ──
#   ICONDIR(6) + ICONDIRENTRY(16) * N + 이미지 데이터
$fs = [IO.File]::Create($outPath)
$bw = New-Object IO.BinaryWriter $fs

$bw.Write([UInt16]0)             # reserved
$bw.Write([UInt16]1)             # type 1 = icon
$bw.Write([UInt16]$sizes.Count)

$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]
    $dim = 0                     # 256 은 0 으로 적는다 (1바이트에 안 들어간다)
    if ($s -lt 256) { $dim = $s }
    $bw.Write([Byte]$dim)
    $bw.Write([Byte]$dim)
    $bw.Write([Byte]0)           # 팔레트 색 수 (0 = 팔레트 없음)
    $bw.Write([Byte]0)           # reserved
    $bw.Write([UInt16]1)         # planes
    $bw.Write([UInt16]32)        # bpp
    $bw.Write([UInt32]$blobs[$i].Length)
    $bw.Write([UInt32]$offset)
    $offset += $blobs[$i].Length
}
foreach ($b in $blobs) { $bw.Write($b) }

$bw.Flush(); $bw.Close(); $fs.Close()

$len = (Get-Item $outPath).Length
Write-Host "[make-icon] frame $Frame -> $($sizes -join ', ') px   $len bytes   ->  $outPath"
