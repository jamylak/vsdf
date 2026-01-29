param(
  [string]$Ref = "n8.0.1",
  [string]$Dest = (Join-Path (Get-Location) "external/ffmpeg-src")
)

if (Test-Path (Join-Path $Dest ".git")) {
  Write-Host "FFmpeg source already exists at: $Dest"
  Write-Host "Delete it first if you want to re-fetch."
  exit 0
}

git clone --depth 1 --branch $Ref https://github.com/FFmpeg/FFmpeg.git $Dest
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Fetched FFmpeg ($Ref) to: $Dest"
