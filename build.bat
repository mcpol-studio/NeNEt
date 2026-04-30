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

set "PRESET=default"
set "BUILD_DIR=build"
set "DO_CLEAN="

:parse_args
if "%~1"=="" goto run_build
if /i "%~1"=="clean" goto arg_clean
if /i "%~1"=="release" goto arg_release
echo [ERROR] Unknown arg: %~1
exit /b 1

:arg_clean
set "DO_CLEAN=1"
shift
goto parse_args

:arg_release
set "PRESET=release"
set "BUILD_DIR=build-release"
shift
goto parse_args

:run_build
if defined DO_CLEAN (
    echo === Cleaning %BUILD_DIR% ===
    if exist %BUILD_DIR% rmdir /s /q %BUILD_DIR%
)

echo === CMake configure (preset=%PRESET%) ===
cmake --preset %PRESET% || exit /b 1

echo === CMake build ===
cmake --build %BUILD_DIR% --config Release || exit /b 1

echo.
echo =========================================
echo  Build OK. Run:  .\%BUILD_DIR%\NeNEt.exe
echo =========================================
endlocal
