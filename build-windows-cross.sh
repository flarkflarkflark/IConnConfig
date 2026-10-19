#!/bin/bash
set -e

echo "Cross-compiling IConnConfig for Windows..."

# Clean previous build
make clean 2>/dev/null || true
rm -f Makefile

# Use MinGW qmake
/usr/x86_64-w64-mingw32/bin/qmake iConnConfig.pro

# Cross-compile
make -j$(nproc)

echo "Windows build complete!"
echo "Executable: iconnconfig.exe"