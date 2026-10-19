#!/bin/bash
set -e

echo "Building LARGE TEXT iConnConfig..."

# Backup and replace main.cpp
if [ ! -f src/main-original.cpp ]; then
    cp src/main.cpp src/main-original.cpp
fi
cp src/main-large.cpp src/main.cpp

# Clean
make clean 2>/dev/null || true
rm -f Makefile *.o moc_* ui_*

# Build
qmake-qt5 iConnConfig.pro PREFIX=/usr
make -j$(nproc)

echo ""
echo "✓ LARGE TEXT build complete!"
echo "✓ 16-20pt fonts (was 8-10pt)"
echo "✓ Bigger buttons and dropdowns"
echo "✓ Better space usage"
echo "✓ USB Device section now readable!"
echo ""
echo "Run: ./iconnconfig"