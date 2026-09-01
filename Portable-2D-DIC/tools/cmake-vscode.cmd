@echo off
setlocal
chcp 65001 >nul
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "VSLANG=1033"
set "CUDACXX=D:\Dev\CUDA-13.3\bin\nvcc.exe"
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" %*
exit /b %errorlevel%
