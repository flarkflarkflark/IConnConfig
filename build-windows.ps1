# Build IConnConfig for Windows
Write-Host "Building IConnConfig for Windows..." -ForegroundColor Green

# Check if PKControls exists
if (-not (Test-Path "..\PKControls")) {
    Write-Host "Cloning PKControls dependency..." -ForegroundColor Yellow
    git clone https://codeberg.org/dehnhardt/PKControls.git ..\PKControls
}

# Build with qmake
Write-Host "Running qmake..." -ForegroundColor Yellow
qmake iConnConfig.pro

# Build with make/nmake
Write-Host "Building..." -ForegroundColor Yellow
if (Test-Path "Makefile") {
    make
} elseif (Test-Path "Makefile.Debug") {
    nmake debug
} else {
    Write-Host "Error: No Makefile found" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host "Build complete!" -ForegroundColor Green
Write-Host "Executable should be in debug/ or release/ folder" -ForegroundColor Cyan
Read-Host "Press Enter to exit"