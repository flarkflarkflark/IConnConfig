#!/bin/bash
set -e

echo "Building improved IConnConfig with KDE/GNOME integration..."

# Backup original main.cpp
if [ ! -f src/main-original.cpp ]; then
    cp src/main.cpp src/main-original.cpp
fi

# Use improved main.cpp
cp src/main-improved.cpp src/main.cpp

# Clean and build
make clean 2>/dev/null || true
rm -f Makefile

# Build
qmake-qt5 iConnConfig.pro PREFIX=/usr
make -j$(nproc)

echo "Improved build complete!"
echo "Features added:"
echo "- Auto-detects KDE Plasma vs GNOME"
echo "- Better styling and visibility"
echo "- Larger default window size"
echo "- High-DPI scaling support"
echo ""
echo "Usage: ./iconnconfig --scale 1.5  (for high-DPI displays)"