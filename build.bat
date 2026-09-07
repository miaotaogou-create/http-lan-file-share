@echo off
setlocal
cd /d "%~dp0"

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1

set "QT_PREFIX=C:\Qt6_10\6.10.1\msvc2022_64"
set "PATH=%QT_PREFIX%\bin;%PATH%"

echo === Configure ===
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DCMAKE_PREFIX_PATH=%QT_PREFIX%
if errorlevel 1 exit /b 1

echo === Build Release ===
cmake --build build --config Release
if errorlevel 1 exit /b 1

echo === windeployqt ===
"%QT_PREFIX%\bin\windeployqt.exe" --release "build\Release\HttpLanFileShare.exe"

echo.
echo SUCCESS: build\Release\HttpLanFileShare.exe
endlocal
