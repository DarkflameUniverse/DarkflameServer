# Create the build directory, preserving it if it already exists
mkdir -p build
cd build

# Run cmake to generate make files
cmake ..

# Windows build
if [[ "$OSTYPE" =~ ^msys ]]; then

	cmake --build . --config Release
	
	# Move over binaries
	if test -f "Release/MasterServer.exe"; then
		mv Release/*.exe .
	fi
else

	# Run make to build the project. To build utilizing multiple cores, append `-j` and the amount of cores to utilize, for example `make -j8`
	make
fi

# Run migrations
./MasterServer -m
