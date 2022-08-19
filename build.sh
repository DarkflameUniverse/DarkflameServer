# Create the build directory, preserving it if it already exists
mkdir -p build
cd build

# Run cmake to generate make files
cmake ..

# To build utilizing multiple cores, append `-j` and the amount of cores to utilize, for example `cmake --build . --config Release -j8'
cmake --build . --config Release

# Run migrations
./MasterServer -m
