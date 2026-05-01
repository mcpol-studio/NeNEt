@echo off
call "D:\soft\vssss\VC\Auxiliary\Build\vcvars64.bat" >nul
"D:\soft\vssss\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "C:\Users\Mcpol\Desktop\NeNEt\build" --target NeNEt
exit /b %ERRORLEVEL%
