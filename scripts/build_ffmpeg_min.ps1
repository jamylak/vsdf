param(
  [Parameter(Mandatory = $true)]
  [string]$FfmpegSrc,
  [string]$Prefix = (Join-Path (Get-Location) "external/ffmpeg-mini"),
  [string]$BuildDir = (Join-Path (Get-Location) "build/ffmpeg-mini"),
  [string]$X264Dir = $env:X264_DIR
)

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
  Write-Error "cl.exe not found. Run this from a Visual Studio Developer Prompt."
  exit 1
}

if (-not (Get-Command nmake.exe -ErrorAction SilentlyContinue)) {
  Write-Error "nmake.exe not found. Run this from a Visual Studio Developer Prompt."
  exit 1
}

if (-not (Get-Command nasm.exe -ErrorAction SilentlyContinue)) {
  Write-Error "nasm.exe not found. Install NASM and ensure it's on PATH."
  exit 1
}

New-Item -ItemType Directory -Force $Prefix | Out-Null
New-Item -ItemType Directory -Force $BuildDir | Out-Null

Push-Location $BuildDir

$extraCflags = "/MT"
$extraLdflags = ""
if ($X264Dir) {
  $extraCflags = "$extraCflags /I`"$X264Dir\include`""
  $extraLdflags = "/LIBPATH:`"$X264Dir\lib`""
}

& "$FfmpegSrc\configure" `
  --prefix="$Prefix" `
  --toolchain=msvc `
  --arch=x86_64 `
  --target-os=win64 `
  --disable-everything `
  --enable-avcodec `
  --enable-avformat `
  --enable-avutil `
  --enable-swscale `
  --enable-encoder=libx264 `
  --enable-muxer=mp4 `
  --enable-protocol=file `
  --enable-libx264 `
  --enable-gpl `
  --disable-programs `
  --disable-doc `
  --disable-network `
  --disable-avdevice `
  --disable-postproc `
  --disable-swresample `
  --enable-small `
  --enable-static `
  --disable-shared `
  --extra-cflags="$extraCflags" `
  --extra-ldflags="$extraLdflags"
if ($LASTEXITCODE -ne 0) { Pop-Location; exit $LASTEXITCODE }

nmake
if ($LASTEXITCODE -ne 0) { Pop-Location; exit $LASTEXITCODE }
nmake install
if ($LASTEXITCODE -ne 0) { Pop-Location; exit $LASTEXITCODE }

Pop-Location

Write-Host "Installed minimal FFmpeg to: $Prefix"
