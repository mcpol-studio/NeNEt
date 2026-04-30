@echo off
setlocal EnableDelayedExpansion

set "VS_PATH=D:\soft\vssss"
set "VCVARS=%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%VCVARS%" (
    echo [ERROR] vcvars64.bat not found: %VCVARS%
    exit /b 1
)

echo === Loading x64 environment ===
call "%VCVARS%" >nul || exit /b 1

echo === Architecture check ===
where cl
cl 2>&1 | findstr /C:"x64" >nul
if errorlevel 1 (
    echo [ERROR] cl is not x64. Aborting.
    cl
    exit /b 1
)
echo cl: x64 OK

if /i "%1"=="clean" (
    echo === Cleaning build directory ===
    if exist build rmdir /s /q build
)

echo === CMake configure ===
cmake --preset default || exit /b 1

echo === CMake build ===
cmake --build build || exit /b 1

echo.
echo =========================================
echo  Build OK. Run:  .\build\NeNEt.exe
echo =========================================
endlocal
