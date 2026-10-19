#!/usr/bin/env bash
set -euo pipefail

# Install required packages inside the Arch container
pacman -Sy --noconfirm --needed base-devel git qt5-base qt5-tools alsa-lib libusb

cd /build

# Ensure PKControls is present as sibling if required by project
if [ ! -d ../PKControls ]; then
  git clone https://codeberg.org/dehnhardt/PKControls.git ../PKControls || true
fi

# Run qmake and make (single-threaded to avoid $(nproc) quoting issues)
qmake -qt=5 -o Makefile iConnConfig.pro || qmake -o Makefile iConnConfig.pro
make -j1
