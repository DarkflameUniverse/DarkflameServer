# Git Pull the recent changes from the repository
git pull

# check if there is a submodule update
git submodule update --init --recursive

# cd into the directory build
cd build

# run the cmake command
cmake ..

# run the make command
make

# run the migrations
./MasterServer -m
