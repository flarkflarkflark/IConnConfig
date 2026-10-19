#!/bin/bash
set -e

echo "Rebuilding IConnConfig with light theme..."

# Backup and replace main.cpp
if [ ! -f src/main-original.cpp ]; then
    cp src/main.cpp src/main-original.cpp
fi
cp src/main-light.cpp src/main.cpp

# Clean
make clean 2>/dev/null || true
rm -f Makefile *.o moc_* ui_*

# Build
qmake-qt5 iConnConfig.pro PREFIX=/usr
make -j$(nproc)

echo ""
echo "✓ Build complete with light theme!"
echo "✓ Larger window size (1200x800)"
echo "✓ Better visibility and contrast"
echo ""
echo "Run: ./iconnconfig"