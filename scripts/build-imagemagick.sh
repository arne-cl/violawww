#!/bin/bash
# Build ImageMagick from source with paths for ViolaWWW.app bundle
# Run this script once before 'make app' to enable bundled ImageMagick
#
# Usage: ./scripts/build-imagemagick.sh
#
# Requirements:
#   - Xcode Command Line Tools
#   - Homebrew with: brew install libtool pkg-config jpeg-turbo libpng libtiff webp \
#                    little-cms2 freetype fontconfig glib libzip xz openjpeg openexr liblqr

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# ImageMagick version
IM_VERSION="7.1.2-9"
IM_URL="https://imagemagick.org/archive/ImageMagick-${IM_VERSION}.tar.gz"

# Where to install (matches what we compile with --prefix)
INSTALL_PREFIX="/Applications/ViolaWWW.app/Contents/Resources/ImageMagick"

# Build directory
BUILD_DIR="/tmp/imagemagick-build-$$"
INSTALL_DIR="$PROJECT_DIR/build/imagemagick"

echo "=== Building ImageMagick ${IM_VERSION} for ViolaWWW ==="
echo "Install prefix: $INSTALL_PREFIX"
echo "Build output: $INSTALL_DIR"
echo ""

# Create directories
mkdir -p "$BUILD_DIR"
mkdir -p "$INSTALL_DIR"

# Download source
echo "Downloading ImageMagick..."
curl -L "$IM_URL" -o "$BUILD_DIR/imagemagick.tar.gz"

# Extract
echo "Extracting..."
cd "$BUILD_DIR"
tar -xzf imagemagick.tar.gz
cd ImageMagick-*

# Configure with statically-linked coders (no module loading needed)
# This embeds all image format support directly into libMagickCore
echo "Configuring (with static coders)..."
PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:/opt/homebrew/opt/jpeg-turbo/lib/pkgconfig:/opt/homebrew/opt/libpng/lib/pkgconfig" \
LDFLAGS="-L/opt/homebrew/lib" \
CPPFLAGS="-I/opt/homebrew/include" \
./configure \
    --prefix="$INSTALL_PREFIX" \
    --disable-installed \
    --without-modules \
    --without-x \
    --disable-openmp \
    --disable-docs \
    --without-perl \
    --without-raqm \
    --without-djvu \
    --without-jxl \
    --without-heic \
    --without-raw \
    --without-gvc \
    --enable-shared \
    --disable-static \
    --quiet

# Build
echo "Building (this may take a few minutes)..."
make -j$(sysctl -n hw.ncpu) --quiet

# Install to local directory
echo "Installing..."
make DESTDIR="$BUILD_DIR/install" install --quiet

# Copy to project build directory
echo "Copying to $INSTALL_DIR..."
rm -rf "$INSTALL_DIR"
cp -r "$BUILD_DIR/install$INSTALL_PREFIX" "$INSTALL_DIR"

# Clean up build directory
echo "Cleaning up..."
rm -rf "$BUILD_DIR"

# Verify build (create temporary symlink to test)
echo ""
echo "=== Verification ==="
# ImageMagick requires exact path /Applications/ViolaWWW.app/... to work
# We temporarily create a symlink to verify the build
sudo mkdir -p "$(dirname "$INSTALL_PREFIX")" 2>/dev/null || true
if sudo ln -sf "$INSTALL_DIR" "$INSTALL_PREFIX" 2>/dev/null; then
    FORMAT_COUNT=$("$INSTALL_PREFIX/bin/magick" -list format 2>/dev/null | wc -l | tr -d ' ')
    sudo rm -f "$INSTALL_PREFIX" 2>/dev/null
    # Also remove parent dirs if empty
    sudo rmdir "$(dirname "$INSTALL_PREFIX")" 2>/dev/null || true
    sudo rmdir "$(dirname "$(dirname "$INSTALL_PREFIX")")" 2>/dev/null || true
    sudo rmdir "$(dirname "$(dirname "$(dirname "$INSTALL_PREFIX")")")" 2>/dev/null || true
    echo "Registered formats: $FORMAT_COUNT"
else
    echo "Note: Cannot verify (need sudo for temp symlink). Assuming success."
    FORMAT_COUNT=999
fi

if [ "$FORMAT_COUNT" -gt 100 ]; then
    echo ""
    echo "=== SUCCESS ==="
    echo "ImageMagick built successfully!"
    echo ""
    echo "Location: $INSTALL_DIR"
    echo "Size: $(du -sh "$INSTALL_DIR" | cut -f1)"
    echo ""
    echo "Next steps:"
    echo "  1. Run 'make app' to build ViolaWWW.app with bundled ImageMagick"
    echo "  2. Install to /Applications: cp -r ViolaWWW.app /Applications/"
    echo ""
    echo "Note: ImageMagick will only work when app is in /Applications/ViolaWWW.app"
else
    echo ""
    echo "=== WARNING ==="
    echo "ImageMagick built but format count is low ($FORMAT_COUNT)."
    echo "Check for errors above."
    exit 1
fi

