@echo off
echo Compiling click.exe using g++...
g++ -O3 -std=c++17 main.cpp -o click.exe -static -municode -lshlwapi -luser32
if %ERRORLEVEL% EQU 0 (
    echo Compilation successful!
    echo click.exe has been created.
) else (
    echo Compilation failed!
)
pause
