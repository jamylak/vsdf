#!/bin/bash
# Build Linux binary on Mac using Docker and test it
set -e

# Set to "linux/amd64" for x86_64, or "" for native (arm64 on M1 Mac)
# PLATFORM="linux/amd64"
PLATFORM=""
BUILD_DISTRO="ubuntu:latest"

# Build platform flag for docker
PLATFORM_FLAG=""
if [ -n "$PLATFORM" ]; then
  PLATFORM_FLAG="--platform $PLATFORM"
  echo "🔧 Building for: $PLATFORM on $BUILD_DISTRO"
else
  echo "🔧 Building for: native architecture on $BUILD_DISTRO"
fi

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 1: Building Linux binary in Docker..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

docker run --rm $PLATFORM_FLAG -v $(pwd):/work -w /work $BUILD_DISTRO bash -c "
  export DEBIAN_FRONTEND=noninteractive && \
  export TZ=Etc/UTC && \
  apt-get update -qq && \
  apt-get install -y --no-install-recommends build-essential cmake ninja-build \
    libspdlog-dev libglfw3-dev libvulkan-dev glslang-tools glslang-dev \
    libglm-dev pkg-config libavcodec-dev libavformat-dev libavutil-dev \
    libswscale-dev patchelf git && \
  echo '✓ Dependencies installed' && \
  cmake -B build-linux -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS='-Wno-error=array-bounds' -G Ninja . && \
  cmake --build build-linux && \
  echo '✓ Build complete' && \
  echo && \
  echo '📋 Libraries linked BEFORE bundling/patching:' && \
  ldd /work/build-linux/vsdf && \
  echo && \
  echo '━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━' && \
  echo 'Bundling dependencies (inside build container)...' && \
  echo '━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━' && \
  mkdir -p /work/release/linux/libs && \
  cp /work/build-linux/vsdf /work/release/linux/ && \
  chmod +x /work/release/linux/vsdf && \
  echo 'Finding and copying non-system libraries...' && \
  ldd /work/build-linux/vsdf | grep -v 'linux-vdso\|libc\.so\|libm\.so\|libpthread\|libdl\|librt\|libstdc++\|libgcc\|/lib/ld-' | grep '=>' | awk '{print \$3}' | grep '^/' > /tmp/libs_to_copy.txt && \
  cat /tmp/libs_to_copy.txt | while read lib; do
    if [ -f \"\$lib\" ]; then
      libname=\$(basename \"\$lib\")
      echo \"  → Copying \$libname\"
      cp \"\$lib\" /work/release/linux/libs/ 2>/dev/null || true
    fi
  done && \
  BUNDLED_COUNT=\$(ls /work/release/linux/libs/*.so* 2>/dev/null | wc -l) && \
  echo \"Total libraries bundled: \$BUNDLED_COUNT\" && \
  echo 'Setting RPATH...' && \
  patchelf --set-rpath '\$ORIGIN/libs' /work/release/linux/vsdf && \
  for lib in /work/release/linux/libs/*.so*; do
    [ -f \"\$lib\" ] && patchelf --set-rpath '\$ORIGIN' \"\$lib\" 2>/dev/null || true
  done && \
  echo '✓ Bundling complete'
"

cp -r shaders release/linux/

echo
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 2: Examining binary..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

docker run --rm $PLATFORM_FLAG -v $(pwd)/release/linux:/test ubuntu:latest bash -c "
  apt-get update -qq && apt-get install -y patchelf file > /dev/null 2>&1 && \
  
  echo '📦 Binary info:' && \
  file /test/vsdf && \
  ls -lh /test/vsdf && \
  echo && \
  
  echo '🔗 RPATH configuration:' && \
  patchelf --print-rpath /test/vsdf && \
  echo && \
  
  echo '📚 Bundled libraries:' && \
  ls -lh /test/libs/ 2>/dev/null || echo '  (no libs bundled)' && \
  echo && \
  
  echo '🔍 All dependencies (ldd output):' && \
  ldd /test/vsdf && \
  echo && \
  
  echo '🎯 Non-system dependencies:' && \
  ldd /test/vsdf | grep -v 'linux-vdso\|libc\|libm\|libpthread\|libdl\|librt\|libstdc++\|libgcc' | grep '=>' && \
  echo && \
  
  echo '⚠️  Checking for missing dependencies:' && \
  if ldd /test/vsdf | grep 'not found'; then
    echo '❌ ERROR: Missing dependencies found!'
    exit 1
  else
    echo '✅ No missing dependencies'
  fi
"

echo
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 3: Testing binary on multiple distros..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Test on Ubuntu 24.04 (latest)
echo
echo "📦 Testing on Ubuntu 24.04..."
docker run --rm $PLATFORM_FLAG -v $(pwd)/release/linux:/test ubuntu:latest bash -c "
  apt-get update -qq && \
  apt-get install -y --no-install-recommends \
    xvfb mesa-vulkan-drivers \
    libglfw3 libvulkan1 \
    libavcodec60 libavformat60 libavutil58 libswscale7 > /dev/null 2>&1 && \
  
  cd /test && \
  echo '→ Testing --version...' && \
  ./vsdf --version && \
  echo '✓ --version works'
  
  # These don't seem to work because of Docker I guess
  # echo '→ Testing 1-frame headless render (requires Vulkan)...' && \
  # export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json && \
  # xvfb-run -s '-screen 0 1024x768x24' \
  #   ./vsdf --toy shaders/testtoyshader.frag --frames 1 --headless --log-level info && \
  # echo '✓ Headless render works' && \
  #
  # echo '→ Testing offline render (FFmpeg, requires Vulkan)...' && \
  # xvfb-run -s '-screen 0 1024x768x24' \
  #   ./vsdf --toy shaders/testtoyshader.frag --frames 10 --offline out-test.mp4 --log-level info && \
  # [ -f out-test.mp4 ] && \
  # echo '✓ Offline render works (created out-test.mp4)' && \
  # rm -f out-test.mp4
" && echo "✅ Ubuntu 24.04: PASSED" || echo "❌ Ubuntu 24.04: FAILED"

# Test on Debian 13 (Trixie)
echo
echo "📦 Testing on Debian 13 (Trixie)..."
docker run --rm $PLATFORM_FLAG -v $(pwd)/release/linux:/test debian:trixie bash -c "
  apt-get update -qq && \
  apt-get install -y --no-install-recommends \
    xvfb xauth mesa-vulkan-drivers \
    libglfw3 libvulkan1 \
    libavcodec61 libavformat61 libavutil59 libswscale8 \
    > /dev/null 2>&1 && \
  
  cd /test && \
  echo '→ Testing --version...' && \
  ./vsdf --version && \
  echo '✓ --version works'
  
  # These don't seem to work because of Docker I guess
  # echo '→ Testing 1-frame headless render...' && \
  # export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json && \
  # xvfb-run -s '-screen 0 1024x768x24' \
  #   ./vsdf --toy shaders/testtoyshader.frag --frames 1 --headless --log-level info && \
  # echo '✓ Headless render works'
" && echo "✅ Debian 12: PASSED" || echo "❌ Debian 12: FAILED"
