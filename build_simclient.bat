@echo off
REM Simple build and test script for SimClient (Windows)
REM This script demonstrates how to build and run the SimClient

echo DarkflameServer SimClient Build and Test Script
echo ===============================================

REM Check if we're in the right directory
if not exist CMakeLists.txt (
    echo Error: Please run this script from the DarkflameServer root directory
    pause
    exit /b 1
)

REM Create build directory if it doesn't exist
if not exist build (
    echo Creating build directory...
    mkdir build
)

cd build

REM Generate build files
echo Generating build files...
cmake .. -DCMAKE_BUILD_TYPE=Release

if errorlevel 1 (
    echo Error: CMake configuration failed
    pause
    exit /b 1
)

REM Build SimClient
echo Building SimClient...
cmake --build . --target SimClient --config Release

if errorlevel 1 (
    echo Error: Build failed
    pause
    exit /b 1
)

echo Build completed successfully!
echo.
echo SimClient executable location:
dir /s /b SimClient.exe 2>nul

echo.
echo To run SimClient:
echo   SimClient.exe --help                    # Show help
echo   SimClient.exe -t basic -v               # Basic test with verbose logging  
echo   SimClient.exe -n 5 -t load              # Load test with 5 clients
echo   SimClient.exe -a 127.0.0.1:1001 -n 3    # Connect to specific auth server
echo.
echo Make sure your DarkflameServer is running before testing!
pause
