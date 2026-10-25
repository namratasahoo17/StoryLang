@echo off
rem ============================================================================
rem  build_windows.bat  -  builds StoryLang.exe (one file, no DLLs, runs from a pendrive)
rem
rem  You need ONE thing installed: a C++ compiler for Windows.
rem  Easiest: w64devkit (no installer, just unzip) from
rem     https://github.com/skeeto/w64devkit/releases   (download the .7z.exe / .zip, extract, 
rem     then open "w64devkit.exe" and run this file from inside it)
rem  Or MSYS2 (https://www.msys2.org) with:  pacman -S mingw-w64-ucrt-x86_64-gcc
rem  and run from its "UCRT64" terminal.
rem ============================================================================
setlocal
cd /d "%~dp0"

where g++ >nul 2>nul
if errorlevel 1 (
    echo.
    echo [!] g++ was not found. Install w64devkit or MSYS2 - see the notes at the top of this file.
    goto :end
)

if not exist build mkdir build

if not exist third_party\sqlite3\sqlite3.c (
    echo Downloading SQLite ^(one-time, ~2 MB^)...
    powershell -NoProfile -Command "try { [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -UseBasicParsing 'https://www.sqlite.org/2024/sqlite-amalgamation-3450100.zip' -OutFile 'build\sqlite.zip'; Expand-Archive -Force 'build\sqlite.zip' 'build\sqlite_tmp'; Copy-Item 'build\sqlite_tmp\*\sqlite3.c','build\sqlite_tmp\*\sqlite3.h' 'third_party\sqlite3\' } catch { exit 1 }"
)
if not exist third_party\sqlite3\sqlite3.c (
    echo.
    echo [!] Could not download SQLite automatically. Do it by hand:
    echo     1. Open https://www.sqlite.org/download.html
    echo     2. Under "Source Code" download  sqlite-amalgamation-XXXXXXX.zip
    echo     3. Copy  sqlite3.c  and  sqlite3.h  into the folder  third_party\sqlite3\
    echo     4. Run this file again.
    goto :end
)

echo Compiling SQLite...
gcc -O2 -DSQLITE_THREADSAFE=0 -c third_party\sqlite3\sqlite3.c -o build\sqlite3.o
if errorlevel 1 goto :fail

echo Compiling StoryLang...
g++ -std=c++17 -O2 -Iinclude -Ithird_party\sqlite3 -static -static-libgcc -static-libstdc++ -o StoryLang.exe ^
    src\main.cpp ^
    src\model\Story.cpp src\model\SampleStories.cpp ^
    src\engine\Player.cpp ^
    src\analysis\Validator.cpp src\analysis\PathFinder.cpp src\analysis\StoryMap.cpp ^
    src\storage\Database.cpp src\storage\StoryRepository.cpp ^
    src\ui\InputHelper.cpp src\ui\Editor.cpp src\ui\Ui.cpp ^
    build\sqlite3.o
if errorlevel 1 goto :fail

echo.
echo ==========================================================
echo   SUCCESS:  StoryLang.exe  is ready in this folder.
echo   Copy StoryLang.exe to your pendrive and double-click it.
echo   (It creates a "data" folder next to itself for saved stories.)
echo ==========================================================
goto :end

:fail
echo.
echo [!] Build failed - scroll up for the first error message.

:end
echo.
pause
