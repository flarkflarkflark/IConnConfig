#!/bin/bash
set -e

echo "Building MODERN iConnConfig (iConnectivity style)..."

# Backup and replace main.cpp
if [ ! -f src/main-original.cpp ]; then
    cp src/main.cpp src/main-original.cpp
fi
cp src/main-modern.cpp src/main.cpp

# Clean
make clean 2>/dev/null || true
rm -f Makefile *.o moc_* ui_*

# Build
qmake-qt5 iConnConfig.pro PREFIX=/usr
make -j$(nproc)

echo ""
echo "✓ MODERN build complete!"
echo "✓ Dark theme like original iConnectivity software"
echo "✓ Clean navigation panel"
echo "✓ Modern blue accents (#0078d4)"
echo "✓ Professional look and feel"
echo ""
echo "Run: ./iconnconfig"