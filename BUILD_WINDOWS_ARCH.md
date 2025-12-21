# Arch Linux and Windows build instructions

## Arch Linux (PKGBUILD)

A `PKGBUILD` template is provided at the project root (`PKGBUILD`). Basic steps to build on Arch:

```bash
cd /path/to/iconnconfig
# ensure PKControls is checked out next to this repo
makepkg -si
```

If you prefer to build from source without `makepkg`:

```bash
qmake-qt5 iConnConfig.pro
make
```

## Windows (MSVC or MinGW)

General guidance to build on Windows with Qt:

- Install Qt (Qt 5) and either MSVC or MinGW toolchain.
- Open the `iConnConfig.pro` in Qt Creator and select the appropriate kit.
- Ensure the `PKControls` dependency is available in the parent directory as described in BUILD.md.
- Build the project in Qt Creator (it will run qmake and build with the chosen toolchain).

Notes:
- The codebase contains platform guards for POSIX headers; Windows-specific libraries are already enabled in the `.pro`.
- If you prefer a command-line build with MinGW/MSYS2, run:

```bash
# for MSYS2/MinGW
qmake-qt5 iConnConfig.pro
make
```
