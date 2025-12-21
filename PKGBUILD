#! /bin/bash
# Maintainer: you <you@example.com>
pkgname=iconnconfig
pkgver=1.0.0
pkgrel=1
pkgdesc="iConnConfig - device configuration GUI"
arch=('x86_64')
url="https://codeberg.org/dehnhardt/iConnConfig"
license=('GPL')
depends=(qt5-base alsa-lib libusb)
makedepends=(qt5-base qt5-tools pkgconfig base-devel git)
source=(.)
# For development builds we skip checksums; replace with real checksums for AUR
sha256sums=('SKIP')

# Derive version from git when available
pkgver() {
  if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git describe --tags --dirty --always 2>/dev/null || echo "1.0.0"
  else
    echo "1.0.0"
  fi
}

build() {
  cd "$srcdir"
  if command -v qmake-qt5 >/dev/null 2>&1; then
    QMAKE=qmake-qt5
  elif command -v qmake >/dev/null 2>&1; then
    QMAKE=qmake
  else
    echo "qmake not found"
    return 1
  fi
  "$QMAKE" iConnConfig.pro PREFIX=/usr
  make -j$(nproc)
}

package() {
  cd "$srcdir"
  install -Dm755 iconnconfig "$pkgdir/usr/bin/iconnconfig" || true
  install -Dm644 resources/iconnconfig.desktop "$pkgdir/usr/share/applications/iconnconfig.desktop" || true
  install -Dm644 resources/iconnconfig.png "$pkgdir/usr/share/icons/hicolor/48x48/apps/iconnconfig.png" || true
  # translations (if built)
  install -Dm644 tr/iconnconfig_en_EN.qm "$pkgdir/usr/share/iconnconfig/iconnconfig_en_EN.qm" || true
}
