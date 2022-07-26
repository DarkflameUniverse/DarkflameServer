# Create the build directory, preserving it if it already exists
mkdir -p build
cd build

# Run cmake to generate make files
cmake ..

# Run make to build the project. To build utilizing multiple cores, append `-j` and the amount of cores to utilize, for example `make -j8`
make

# Run migrations
./MasterServer -m
