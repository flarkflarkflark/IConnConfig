# IConnConfig - Windows Build Guide

## Prerequisites

1. **Install Qt 5**
   - Download from: https://www.qt.io/download-qt-installer
   - Choose Qt 5.15.x with MinGW or MSVC compiler
   - Make sure qmake is in your PATH

2. **Install Git**
   - Download from: https://git-scm.com/download/win

3. **Install iConnectivity Drivers**
   - Download from iConnectivity website for your device
   - Required for device communication

## Build Methods

### Method 1: Using Build Scripts (Recommended)

**Option A: Batch Script**
```cmd
build-windows.bat
```

**Option B: PowerShell Script**
```powershell
.\build-windows.ps1
```

### Method 2: Qt Creator (GUI)
1. Open `iConnConfig.pro` in Qt Creator
2. Select MinGW or MSVC kit
3. Build → Build Project

### Method 3: Command Line
```cmd
REM Clone dependency
git clone https://codeberg.org/dehnhardt/PKControls.git ..\PKControls

REM Build
qmake iConnConfig.pro
make
```

## Output
- Executable will be in `debug/` or `release/` folder
- Copy all Qt DLLs alongside the executable for distribution

## Supported Devices
- MIO 4
- MIO 10  
- iConnect Audio 4+

## Notes
- Requires iConnectivity drivers installed
- Only supports command version 1.0 devices
- For newer devices, use mioctl instead