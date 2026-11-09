# png2c — png 파일을 C 바이트 배열 헤더로 바꾼다.
#
# 왜: 이 게임은 파일을 안 쓴다. 효과음도 음악도 코드가 만들고, 아트도 exe 안에 넣는다.
#     규정이 단일 파일을 요구하진 않는다(옆에 둬도 되지만 **그 파일도 용량에 들어간다**).
#     exe 에 넣는 이유는 ① 옆에 두면 사용자가 지울 수 있고 ② 압축된 png 를 그대로
#     박는 게 raw 로 푸는 것보다 훨씬 작기 때문이다.
#
# ★ png 를 **압축된 채로** 박는다. raw RGBA 로 풀면 256x256 = 256KB 지만
#   16색 인덱스 png 는 3~8KB 다. 게임 안에서 LoadImageFromMemory 가 푼다.
#
#   const unsigned char ATLAS_PNG[] = { ... };
#   Image img = LoadImageFromMemory(".png", ATLAS_PNG, sizeof(ATLAS_PNG));
#
# 사용:
#   powershell -File tools\png2c.ps1 art\atlas.png src\atlas_png.h ATLAS_PNG

param(
    [Parameter(Mandatory = $true)][string]$In,
    [Parameter(Mandatory = $true)][string]$Out,
    [string]$Name = "ATLAS_PNG"
)

# .NET 호출은 Set-Location 을 무시한다. 반드시 절대 경로로 바꿔서 넘긴다.
$inPath  = [IO.Path]::GetFullPath((Join-Path (Get-Location) $In))
$outPath = [IO.Path]::GetFullPath((Join-Path (Get-Location) $Out))

if (-not (Test-Path -LiteralPath $inPath)) {
    Write-Host "[png2c] not found: $inPath"
    exit 1
}

$bytes = [IO.File]::ReadAllBytes($inPath)

# png 인지 확인한다. 다른 걸 넣으면 게임이 런타임에 조용히 빈 텍스처를 쓴다.
$sig = 137, 80, 78, 71, 13, 10, 26, 10
for ($i = 0; $i -lt 8; $i++) {
    if ($bytes[$i] -ne $sig[$i]) {
        Write-Host "[png2c] not a png: $inPath"
        exit 1
    }
}

$sb = New-Object Text.StringBuilder
[void]$sb.AppendLine("/* $Name - $(Split-Path -Leaf $inPath) 를 그대로 담은 png 바이트.")
[void]$sb.AppendLine(" *")
[void]$sb.AppendLine(" * tools\png2c.ps1 이 만든다. 직접 고치지 말 것.")
[void]$sb.AppendLine(" * 원본을 고쳤으면 스크립트를 다시 돌린다.")
[void]$sb.AppendLine(" *")
[void]$sb.AppendLine(' *   Image img = LoadImageFromMemory(".png", ' + $Name + ', sizeof(' + $Name + '));')
[void]$sb.AppendLine(" */")
[void]$sb.AppendLine("static const unsigned char $Name[] = {")

for ($i = 0; $i -lt $bytes.Length; $i += 16) {
    $end = [Math]::Min($i + 16, $bytes.Length)
    $row = for ($k = $i; $k -lt $end; $k++) { "0x{0:x2}" -f $bytes[$k] }
    [void]$sb.AppendLine("    " + ($row -join ", ") + ",")
}

[void]$sb.AppendLine("};")

[IO.File]::WriteAllText($outPath, $sb.ToString(), (New-Object Text.UTF8Encoding $false))

$kb = [Math]::Round($bytes.Length / 1024.0, 1)
Write-Host "[png2c] $Name  $($bytes.Length) bytes ($kb KB)  ->  $outPath"
