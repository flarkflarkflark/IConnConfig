#!/bin/bash
set -e

echo "Building CLEAN iConnConfig..."

# Restore original and use clean version
if [ -f src/main-original.cpp ]; then
    cp src/main-original.cpp src/main.cpp
fi
cp src/main-clean.cpp src/main.cpp

# Clean
make clean 2>/dev/null || true
rm -f Makefile *.o moc_* ui_*

# Build
qmake-qt5 iConnConfig.pro PREFIX=/usr
make -j$(nproc)

echo ""
echo "✓ CLEAN build complete!"
echo "✓ Just bigger fonts (12pt)"
echo "✓ Uses your system theme"
echo ""
echo "Run: ./iconnconfig"