@echo off
setlocal enabledelayedexpansion

echo ================================================
echo ToDoWell - Build Script
echo ================================================
echo.

where cl.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] MSVC compiler not detected
    echo Please run vcvarsall.bat or open "Developer Command Prompt for VS"
    echo.
    echo Example path:
    echo   "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
    echo.
    pause
    exit /b 1
)

echo [1/4] Cleaning old files...
if exist *.obj del /q *.obj
if exist *.pdb del /q *.pdb
if exist *.ilk del /q *.ilk
if exist ToDoWell.exe del /q ToDoWell.exe

echo [2/4] Compiling source files...

set CFLAGS=/EHsc /std:c++17 /O2 /Zi /W3 /D_UNICODE /DUNICODE /MT /nologo
set SOURCES=main.cpp app.cpp gfx.cpp storage.cpp json.cpp

cl.exe %CFLAGS% /c %SOURCES%
if %errorlevel% neq 0 (
    echo [ERROR] Compilation failed
    pause
    exit /b 1
)

echo [3/4] Linking executable...

set LIBS=d2d1.lib dwrite.lib ole32.lib shell32.lib user32.lib gdi32.lib
set OBJS=main.obj app.obj gfx.obj storage.obj json.obj

link.exe /OUT:ToDoWell.exe /SUBSYSTEM:WINDOWS %OBJS% %LIBS% /NOLOGO
if %errorlevel% neq 0 (
    echo [ERROR] Linking failed
    pause
    exit /b 1
)

echo [4/4] Cleaning temporary files...
del /q *.obj

echo.
echo ================================================
echo Build successful!
echo Output: ToDoWell.exe
echo ================================================
echo.

set /p run="Run program now? (Y/N): "
if /i "%run%"=="Y" (
    echo.
    echo Starting ToDoWell...
    start ToDoWell.exe
) else (
    echo.
    echo Build complete. You can run ToDoWell.exe manually.
)

pause
