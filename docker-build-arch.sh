#!/usr/bin/env bash
# Build PKG on Arch Linux in Docker. Mount project root as /build.
# Usage: ./docker-build-arch.sh /absolute/path/to/iconnconfig

set -euo pipefail
if [ "$#" -ne 1 ]; then
  echo "Usage: $0 /absolute/path/to/iconnconfig"
  exit 1
fi
HOST_DIR="$1"
CONTAINER_DIR=/build

docker run --rm -it \
  -v "$HOST_DIR":$CONTAINER_DIR \
  -w $CONTAINER_DIR \
  archlinux:latest /bin/bash -lc '
  pacman -Sy --noconfirm --needed base-devel git qt5-base qt5-tools alsa-lib libusb
  cd "$CONTAINER_DIR"
  # Ensure PKControls is present as sibling if required by project
  if [ ! -d ../PKControls ]; then
    echo "WARNING: ../PKControls not found. Please place PKControls next to iconnconfig root."
  fi
  makepkg -si --noconfirm
'
