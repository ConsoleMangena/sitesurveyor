@echo off
setlocal enabledelayedexpansion

REM Configure Qt path before running (edit as needed)
if "%QT_DIR%"=="" (
  echo Set QT_DIR to your Qt 6 MSVC path, e.g. C:\Qt\6.7.2\msvc2022_64
  exit /b 1
)
set PATH=%QT_DIR%\bin;%PATH%

cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=%QT_DIR%
if errorlevel 1 exit /b 1
cmake --build build --config Release
if errorlevel 1 exit /b 1

"%QT_DIR%\bin\windeployqt.exe" --release --compiler-runtime build\SiteSurveyor.exe
if errorlevel 1 exit /b 1

echo Done. Output in build\