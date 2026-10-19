#!/bin/bash
set -e

echo "Rebuilding IConnConfig with system theme (Klassy/KDE)..."

# Backup and replace main.cpp
if [ ! -f src/main-original.cpp ]; then
    cp src/main.cpp src/main-original.cpp
fi
cp src/main-system.cpp src/main.cpp

# Clean
make clean 2>/dev/null || true
rm -f Makefile *.o moc_* ui_*

# Build
qmake-qt5 iConnConfig.pro PREFIX=/usr
make -j$(nproc)

echo ""
echo "✓ Build complete with system theme!"
echo "✓ Uses your Klassy/KDE colors"
echo "✓ Larger window size (1200x800)"
echo "✓ Better fonts and spacing"
echo ""
echo "Run: ./iconnconfig"