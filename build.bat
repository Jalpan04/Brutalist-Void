@echo off
if not exist "bin" mkdir bin

echo Compiling Brutalist Void...
g++ main.cpp -o bin/brutalist_void.exe -I./include -L./lib -lraylib -lopengl32 -lgdi32 -lwinmm -std=c++17

if %errorlevel% neq 0 (
    echo Compilation Failed!
    pause
    exit /b %errorlevel%
)

echo Compilation Successful!
echo Running...
bin\brutalist_void.exe
