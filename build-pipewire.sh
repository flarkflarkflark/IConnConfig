#!/bin/bash
set -e

echo "Building IConnConfig with PipeWire compatibility..."

# Check if PipeWire is running
if pgrep -x "pipewire" > /dev/null; then
    echo "✓ PipeWire detected"
    
    # Check for ALSA compatibility layer
    if ! pacman -Qs pipewire-alsa > /dev/null; then
        echo "⚠ Installing pipewire-alsa for compatibility..."
        echo "Run: sudo pacman -S pipewire-alsa"
        echo "This replaces alsa-lib with PipeWire's ALSA compatibility layer"
    fi
else
    echo "PipeWire not detected, continuing with standard build..."
fi

# Backup and replace main.cpp
if [ ! -f src/main-original.cpp ]; then
    cp src/main.cpp src/main-original.cpp
fi
cp src/main-system.cpp src/main.cpp

# Clean
make clean 2>/dev/null || true
rm -f Makefile *.o moc_* ui_*

# Build with PipeWire-friendly flags
echo "Building with PipeWire compatibility..."
qmake-qt5 iConnConfig.pro PREFIX=/usr \
    "DEFINES+=PIPEWIRE_COMPAT" \
    "LIBS+=-lpipewire-0.3"

make -j$(nproc)

echo ""
echo "✓ Build complete with PipeWire support!"
echo "✓ Uses system theme colors"
echo "✓ PipeWire ALSA compatibility"
echo ""
echo "If audio doesn't work, try:"
echo "  systemctl --user restart pipewire"
echo "  ./iconnconfig"