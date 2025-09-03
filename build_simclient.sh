#!/bin/bash

# Simple build and test script for SimClient
# This script demonstrates how to build and run the SimClient

echo "DarkflameServer SimClient Build and Test Script"
echo "==============================================="

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: Please run this script from the DarkflameServer root directory"
    exit 1
fi

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build

# Generate build files
echo "Generating build files..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build SimClient
echo "Building SimClient..."
if command -v make >/dev/null 2>&1; then
    make SimClient -j$(nproc)
else
    # For systems without make, try cmake --build
    cmake --build . --target SimClient --config Release
fi

if [ $? -ne 0 ]; then
    echo "Error: Build failed"
    exit 1
fi

echo "Build completed successfully!"
echo ""
echo "SimClient executable location:"
find . -name "SimClient*" -type f -executable 2>/dev/null | head -1

echo ""
echo "To run SimClient:"
echo "  ./SimClient --help                    # Show help"
echo "  ./SimClient -t basic -v               # Basic test with verbose logging"
echo "  ./SimClient -n 5 -t load              # Load test with 5 clients"
echo "  ./SimClient -a 127.0.0.1:1001 -n 3    # Connect to specific auth server"
echo ""
echo "Make sure your DarkflameServer is running before testing!"
