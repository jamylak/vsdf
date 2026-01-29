#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $0 /path/to/ffmpeg-src [prefix] [build-dir]" >&2
  exit 1
fi

FFMPEG_SRC=$1
PREFIX=${2:-"$(pwd)/external/ffmpeg-mini"}
BUILD_DIR=${3:-"$(pwd)/build/ffmpeg-mini"}

mkdir -p "$PREFIX" "$BUILD_DIR"

cd "$BUILD_DIR"

"$FFMPEG_SRC/configure" \
  --prefix="$PREFIX" \
  --disable-everything \
  --enable-avcodec \
  --enable-avformat \
  --enable-avutil \
  --enable-swscale \
  --enable-encoder=libx264 \
  --enable-muxer=mp4 \
  --enable-protocol=file \
  --enable-libx264 \
  --enable-gpl \
  --disable-programs \
  --disable-doc \
  --disable-network \
  --disable-avdevice \
  --disable-postproc \
  --disable-swresample \
  --enable-small

make -j"$(getconf _NPROCESSORS_ONLN)"
make install

echo "Installed minimal ffmpeg to: $PREFIX"
echo "If you need .mov output, add: --enable-muxer=mov"
echo "If you want shared libs instead of static, add: --enable-shared --disable-static"
