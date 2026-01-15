#!/bin/bash
set -e

# macOS bundling script - replicates the GitHub Actions workflow logic
# Usage: ./scripts/bundle_macos.sh [build_dir] [release_dir]

BUILD_DIR="${1:-build}"
RELEASE_DIR="${2:-release/macos}"

echo "📦 macOS Binary Bundling Script"
echo "================================"
echo "Build directory: $BUILD_DIR"
echo "Release directory: $RELEASE_DIR"
echo ""

# Check if binary exists
if [ ! -f "$BUILD_DIR/vsdf" ]; then
  echo "❌ Error: $BUILD_DIR/vsdf not found"
  echo "Please build the project first with: cmake -B build -DCMAKE_BUILD_TYPE=Release . && cmake --build build"
  exit 1
fi

# Create libs directory
mkdir -p "$RELEASE_DIR/libs"

# Copy the binary
cp "$BUILD_DIR/vsdf" "$RELEASE_DIR/"

echo "📋 Libraries linked BEFORE bundling:"
otool -L "$BUILD_DIR/vsdf"
echo ""

# Recursively find and copy all non-system dylibs
echo "🔍 Finding and bundling dynamic libraries..."

# Function to copy a library and its dependencies
copy_lib_recursive() {
  local lib="$1"
  local libname=$(basename "$lib")
  
  # Skip if already copied
  if [ -f "$RELEASE_DIR/libs/$libname" ]; then
    return
  fi
  
  # Skip system libraries
  if [[ "$lib" == /System/* ]] || [[ "$lib" == /usr/lib/* ]] || [[ "$lib" == /Library/Apple/* ]]; then
    return
  fi
  
  # Copy the library
  if [ -f "$lib" ]; then
    echo "  → Copying $libname"
    cp "$lib" "$RELEASE_DIR/libs/"
    
    # Recursively copy dependencies of this library
    otool -L "$lib" | tail -n +2 | awk '{print $1}' | while read -r dep; do
      if [ -f "$dep" ]; then
        copy_lib_recursive "$dep"
      fi
    done
  fi
}

# Get initial list of libraries from the binary
otool -L "$BUILD_DIR/vsdf" | tail -n +2 | awk '{print $1}' | while read -r lib; do
  if [ -f "$lib" ]; then
    copy_lib_recursive "$lib"
  fi
done

BUNDLED_COUNT=$(ls "$RELEASE_DIR/libs"/*.dylib 2>/dev/null | wc -l | xargs)
echo ""
echo "Total libraries bundled: $BUNDLED_COUNT"
echo ""

# Fix all library paths to use @rpath
echo "🔧 Fixing library paths..."

# Set rpath on the binary to look in libs/ folder next to the executable
# Only add if it doesn't already exist
if ! otool -l "$RELEASE_DIR/vsdf" | grep -q "@executable_path/libs"; then
  echo "  Adding @executable_path/libs to rpath"
  install_name_tool -add_rpath "@executable_path/libs" "$RELEASE_DIR/vsdf"
else
  echo "  @executable_path/libs already in rpath"
fi

# Fix paths in the main binary
for lib in "$RELEASE_DIR/libs"/*.dylib; do
  libname=$(basename "$lib")
  # Find the original path in the binary
  ORIG_PATH=$(otool -L "$RELEASE_DIR/vsdf" | grep "$libname" | awk '{print $1}')
  if [ ! -z "$ORIG_PATH" ]; then
    echo "  Fixing $libname in vsdf: $ORIG_PATH -> @rpath/$libname"
    install_name_tool -change "$ORIG_PATH" "@rpath/$libname" "$RELEASE_DIR/vsdf"
  fi
done

# Fix inter-library dependencies
for lib in "$RELEASE_DIR/libs"/*.dylib; do
  libname=$(basename "$lib")
  
  # Fix the library's own id
  install_name_tool -id "@rpath/$libname" "$lib"
  
  # Fix references to other bundled libraries
  for otherlib in "$RELEASE_DIR/libs"/*.dylib; do
    othername=$(basename "$otherlib")
    ORIG_PATH=$(otool -L "$lib" | grep "$othername" | awk '{print $1}')
    if [ ! -z "$ORIG_PATH" ] && [ "$ORIG_PATH" != "@rpath/$othername" ]; then
      echo "  Fixing $othername in $libname: $ORIG_PATH -> @rpath/$othername"
      install_name_tool -change "$ORIG_PATH" "@rpath/$othername" "$lib"
    fi
  done
done

echo "Ad-hoc signing binary and libraries..."
# Ad-hoc sign all libraries
for lib in release/macos/libs/*.dylib; do
  echo "Signing $(basename "$lib")"
  codesign --force --sign - "$lib"
done

# Ad-hoc sign the main binary
echo "Signing vsdf"
codesign --force --sign - release/macos/vsdf

echo "Verifying signatures..."
codesign -v release/macos/vsdf
for lib in release/macos/libs/*.dylib; do
  codesign -v "$lib"
done

echo ""
echo "✓ Bundling complete"
echo ""
echo "📦 Binary info:"
file "$RELEASE_DIR/vsdf"
ls -lh "$RELEASE_DIR/vsdf"
echo ""
echo "📚 Bundled libraries:"
ls -lh "$RELEASE_DIR/libs/"
echo ""
echo "🔗 Binary dependencies after fixing:"
otool -L "$RELEASE_DIR/vsdf"

echo ""
echo "🎉 Done! To package the release:"
echo "  cp -r shaders $RELEASE_DIR/"
echo "  cp README.md LICENSE $RELEASE_DIR/"
echo "  cd release && tar -czf vsdf-macos-$(uname -m).tar.gz macos/"
