#!/usr/bin/env bash
set -euo pipefail

DEFAULT_REF="n8.0.1"
REF=${1:-"$DEFAULT_REF"}
DEST=${2:-"$(pwd)/external/ffmpeg-src"}

if [[ -d "$DEST/.git" ]]; then
  echo "FFmpeg source already exists at: $DEST"
  echo "Delete it first if you want to re-fetch."
  exit 0
fi

git clone --depth 1 --branch "$REF" https://github.com/FFmpeg/FFmpeg.git "$DEST"
echo "Fetched FFmpeg ($REF) to: $DEST"
