#!/bin/bash
set -e

echo "Building READABLE IConnConfig..."

# Backup and replace main.cpp
if [ ! -f src/main-original.cpp ]; then
    cp src/main.cpp src/main-original.cpp
fi
cp src/main-readable.cpp src/main.cpp

# Clean
make clean 2>/dev/null || true
rm -f Makefile *.o moc_* ui_*

# Build
qmake-qt5 iConnConfig.pro PREFIX=/usr
make -j$(nproc)

echo ""
echo "✓ READABLE build complete!"
echo "✓ Larger fonts (12-14pt)"
echo "✓ Horizontal text (no more vertical!)"
echo "✓ Bigger buttons and dropdowns"
echo "✓ Better spacing and padding"
echo ""
echo "Run: ./iconnconfig"