# <img style="float: left; padding-right: 5px" height=35px width=35px src="logo.png"> Darkflame Universe

## Introduction
Darkflame Universe (DLU) is a server emulator for LEGO® Universe. Development started in 2013 and has gone through multiple iterations and is now able to present a near perfect emulation of the game server.

### LEGO® Universe
Developed by NetDevil and The LEGO Group, LEGO® Universe launched in October 2010 and ceased operation in January 2012.

## License
Darkflame Universe is licensed under AGPLv3, please read [LICENSE](LICENSE). Some important points:
* We are not liable for anything you do with the code
* The code comes without any warranty what so ever
* You must disclose any changes you make to the code when you distribute it
* Hosting a server for others counts as distribution

## Disclaimers
### Setup difficulty
Throughout the entire build and setup process a level of familiarity with the command line and preferably a Unix-like development environment is greatly advantageous.

### Hosting a server
We do not recommend hosting public servers. DLU is intended for small scale deployment, for example within a group of friends. It has not been tested for large scale deployment which comes with additional security risks.

### Supply of resource files
Darkflame Universe is a server emulator and does not distribute any LEGO® Universe files. A separate game client is required to setup this server emulator and play the game, which we cannot supply. Users are strongly suggested to refer to the safe checksums listed [here](#lego®-universe-11064) to see if a client will work.

### Steps to setup server
* [Clone this repository](#clone-the-repository)
* Install dependencies
    * [Python](#python)
    * [cmake](#cmake)

## Build
Development of the latest iteration of Darkflame Universe has been done primarily in a Unix-like environment and is where it has been tested and designed for deployment. It is therefore highly recommended that Darkflame Universe be built and deployed using a Unix-like environment for the most streamlined experience.

#### Clone the repository
```bash
git clone --recursive https://github.com/DarkflameUniverse/DarkflameServer
```
### Python
Some tools utilized to streamline the setup process require [Python 3](https://www.python.org/downloads/), make sure you have it installed.  

### CMake
This project uses CMake version 3.18 or higher and as such you will need to ensure you have this version installed.
You can check your CMake version by using the following command in a terminal.
```bash
cmake --version
```

If the above command returns an error or is below the supported version of CMake, download and install the latest version from [here](https://cmake.org/download/)

### Installing CMake on Ubuntu
If you are going to be using an ubuntu environment to run the server, you may need to get a more recent version of `cmake` than the packages available may provide.

The general approach to do so would be to obtain a copy of the signing key and then add the CMake repository to your apt
You can do so with the following commands
```bash
# Remove the old version of CMake
sudo apt purge --auto-remove cmake

# Obtain a copy of the signing key
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null

# Install the apt-add-repository package
sudo apt install software-properties-common

# Add the repository to your sources list.
# Depending on your Ubuntu install, the command may differ.
# For ubuntu 20.04, the command is the following
sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ focal main'

# Finally update and install
sudo apt update
sudo apt install cmake
```

### Choosing the right version for your client
DLU clients identify themselves using a higher version number than the regular live clients out there.
This was done make sure that older and incomplete clients wouldn't produce false positive bug reports for us, and because we made bug fixes and new content for the client. 

If you're using a DLU client you'll have to go into the "CMakeVariables.txt" file and change the NET_VERSION variable to 171023 to match the modified client's version number.

### Enabling testing
While it is highly recommended to enable testing, if you would like to save compilation time, you'll want to comment out the enable_testing variable in CMakeVariables.txt.
It is recommended that after building and if testing is enabled, to run `ctest` and make sure all the tests pass.

### Using Docker
Refer to [Docker.md](/Docker.md).

For Windows, refer to [Docker_Windows.md](/Docker_Windows.md).

### Linux builds
Make sure packages like `gcc`, and `zlib` are installed. Depending on the distribution, these packages might already be installed. Note that on systems like Ubuntu, you will need the `zlib1g-dev` package so that the header files are available. `libssl-dev` will also be required as well as `openssl`.

CMake must be version 3.18 or higher!

#### Build the repository

You can either run `build.sh` when in the root folder of the repository:

```bash
./build.sh
```

Or manually run the commands used in [build.sh](build.sh).

### MacOS builds
Ensure `cmake`, `zlib` and `open ssl` are installed as well as a compiler (e.g `clang` or `gcc`).

In the repository root folder run the following. Ensure -DOPENSSL_ROOT_DIR=/path/to/openssl points to your openssl install location
```bash
# Create the build directory, preserving it if it already exists
mkdir -p build
cd build

# Run CMake to generate build files
cmake .. -DOPENSSL_ROOT_DIR=/path/to/openssl

# Get cmake to build the project. If make files are being used then using make and appending `-j` and the amount of cores to utilize may be preferable, for example `make -j8`
cmake --build . --config Release
```

### Windows builds (native)
Ensure that you have either the [MSVC](https://visualstudio.microsoft.com/vs/) or the [Clang](https://github.com/llvm/llvm-project/releases/) (recommended) compiler installed. You will also need to install [CMake](https://cmake.org/download/). Currently on native Windows the server will only work in Release mode.

#### Build the repository
```batch
:: Create the build directory
mkdir build
cd build

:: Run CMake to generate make files
cmake ..

:: Run CMake with build flag to build
cmake --build . --config Release
```

#### Windows for ARM has not been tested but should build by doing the following
```batch
:: Create the build directory
mkdir build
cd build

:: Run CMake to generate make files
cmake .. -DMARIADB_BUILD_SOURCE=ON

:: Run CMake with build flag to build
cmake --build . --config Release
```

### Windows builds (WSL)
This section will go through how to install [WSL](https://docs.microsoft.com/en-us/windows/wsl/install) and building in a Linux environment under Windows. WSL requires Windows 10 version 2004 and higher (Build 19041 and higher) or Windows 11.

#### Open the Command Prompt application with Administrator permissions and run the following:
```bash
# Installing Windows Subsystem for Linux
wsl --install
```

#### Open the Ubuntu application and run the following:
```bash
# Make sure the install is up to date
sudo apt update && sudo apt upgrade

# Make sure the gcc, cmake, build-essentials, zlib1g-dev and libssl-dev packages are installed
sudo apt install gcc cmake build-essential zlib1g-dev libssl-dev
```

Then, [**Follow the Linux instructions**](#linux-builds)

### ARM builds
AArch64 builds should work on linux and MacOS using their respective build steps. Windows ARM should build but it has not been tested

### Updating your build
To update your server to the latest version navigate to your cloned directory
```bash
cd /path/to/DarkflameServer
```
run the following commands to update to the latest changes
```bash
git pull
git submodule update --init --recursive
```
now follow the build section for your system

## Setting up the environment

### Resources

#### LEGO® Universe 1.10.64

This repository does not distribute any LEGO® Universe files. A full install of LEGO® Universe version 1.10.64 (latest) is required to finish setting up Darkflame Universe.

Known good SHA256 checksums of the client:
- `8f6c7e84eca3bab93232132a88c4ae6f8367227d7eafeaa0ef9c40e86c14edf5` (packed client, rar compressed)
- `c1531bf9401426042e8bab2de04ba1b723042dc01d9907c2635033d417de9e05` (packed client, includes extra locales, rar compressed)
- `0d862f71eedcadc4494c4358261669721b40b2131101cbd6ef476c5a6ec6775b` (unpacked client, includes extra locales, rar compressed)

Known good *SHA1* checksum of the DLU client:
- `91498e09b83ce69f46baf9e521d48f23fe502985` (packed client, zip compressed) 

How to generate a SHA256 checksum:
```bash
# Replace <file> with the file path to the zipped client

# If on Linux or MacOS
shasum -a 256 <file>

# If on Windows using the Command Prompt
certutil -hashfile <file> SHA256
```
#### Setting up client dependencies
* Darkflame Universe can run with either a packed or an unpacked client.
* Navigate to `sharedconfig.ini` and fill in the `client_path` field with the location of your client.

#### Client database
* On the initial sever start, the server will do the following

- Extract the file `cdclient.fdb` from your packed client (if you have an unpacked client this step is skipped.)
- `cdclient.fdb` will be converted to `CDServer.sqlite`.

* Optionally, you can also convert the database manually using `fdb_to_sqlite.py` using lcdr's utilities.  Just make sure to rename the file to `CDServer.sqlite` instead of `cdclient.sqlite`.  
* Migrations to the database are automatically run on server start.  Migrations address issues with data in the database that would normally cause bugs.  When migrations are needed to be ran, the server may take a bit longer to start.

### Database
Darkflame Universe utilizes a MySQL/MariaDB database for account and character information.

Initial setup can vary drastically based on which operating system or distribution you are running; there are instructions out there for most setups, follow those and come back here when you have a database up and running.

* Note that all that you need to do is create a database to connect to.  As long as the server can connect to the database, the schema will always be kept up to date when you start the server.

#### Configuration

After the server has been built there should be five `ini` files in the build director: `sharedconfig.ini`, `authconfig.ini`, `chatconfig.ini`, `masterconfig.ini`, and `worldconfig.ini`.  
* All options in `shardconfig.ini` should have something filled in.  
* `worldconfig.ini` contains several options to turn on QOL improvements should you want them.  If you would like the most vanilla experience possible, you will need to turn some of these settings off.
* `masterconfig.ini` contains options related to permissions you want to run your servers with.
* `authconfig.ini` contains an option to enable or disable play keys on your server.  Do not change the default port for auth.
* `chatconfig.ini` contains a port option.

#### Migrations

* Both databases are automatically setup and migrated to what they should look like for the latest commit whenever you start the server.

#### Verify

Your build directory should now look like this:
* AuthServer
* ChatServer
* MasterServer
* WorldServer
* authconfig.ini
* chatconfig.ini
* masterconfig.ini
* sharedconfig.ini
* worldconfig.ini
* ...

Your `sharedconfig.ini` file should have:
* The `client_path` option filled in with the location of your packed client.
* A MySQL/MariaDB database is setup with the credentials filled in

## Running the server
* If everything has been configured correctly you should now be able to run the `MasterServer` binary. Darkflame Universe utilizes port numbers under 1024, so under Linux you either have to give the binary network permissions or run it under sudo.

### First admin user
* Run `MasterServer -a` to get prompted to create an admin account. This method is only intended for the system administrator as a means to get started, do NOT use this method to create accounts for other users!

### Nexus Dashboard

* Follow the instructions [here](https://github.com/DarkflameUniverse/NexusDashboard) to setup the DLU Nexus Dashboard web application. This is the intended way for users to create accounts.

### Admin levels

* The admin level, or Game Master level (hereafter referred to as gmlevel), is specified in the `accounts.gm_level` column in the MySQL database. Normal players should have this set to `0`, which comes with no special privileges. The system administrator will have this set to `9`, which comes will all privileges. gmlevel `8` should be used to give a player a majority of privileges without the safety critical once.

While a character has a gmlevel of anything but `0`, some gameplay behavior will change. When testing gameplay, you should always use a character with a gmlevel of `0`.

## User guide
* A few modifications have to be made to the client.

### Client configuration
To connect to a server follow these steps:
* In the client directory, locate `boot.cfg`
* Open it in a text editor and locate where it says `AUTHSERVERIP=0:`
* Replace the contents after to `:` and the following `,` with what you configured as the server's public facing IP. For example `AUTHSERVERIP=0:localhost` for locally hosted servers
* Launch `legouniverse.exe`, through `wine` if on a Unix-like operating system
* Note that if you are on WSL2, you will need to configure the public IP in the server and client to be the IP of the WSL2 instance and not localhost, which can be found by running `ifconfig` in the terminal. Windows defaults to WSL1, so this will not apply to most users.

### Brick-By-Brick building

* Brick-By-Brick building requires `PATCHSERVERIP=0:` in the `boot.cfg` to point to a HTTP server which always returns `HTTP 404 - Not Found` for all requests. This can be most easily achieved by pointing it to `localhost` while having `sudo python -m http.server 80` running in the background.  On Unix based systems, you may need to run `python3` instead of `python`.
* You may also need to set `UGCSERVERIP=0:` to localhost as well.

### In-game commands
* A list of all in-game commands can be found [here](./docs/Commands.md).

# Credits
## Active Contributors
* [EmosewaMC](https://github.com/EmosewaMC)
* [Jettford](https://github.com/Jettford)
* [Aaron K.](https://github.com/aronwk-aaron)

## DLU Team
* [DarwinAnim8or](https://github.com/DarwinAnim8or)
* [Wincent01](https://github.com/Wincent01)
* [Mick](https://github.com/MickVermeulen)
* [averysumner](https://github.com/codeshaunted)
* [Jon002](https://github.com/jaller200)
* [Jonny](https://github.com/cuzitsjonny)

### Research and tools
* [lcdr](https://github.com/lcdr)
* [Xiphoseer](https://github.com/Xiphoseer)

### Community management
* [Neal](https://github.com/NealSpellman)

### Former contributors
* TheMachine
* Matthew
* [Raine](https://github.com/Rainebannister)
* Bricknave

### Logo
* Cole Peterson (BlasterBuilder)

## Special thanks
* humanoid24
* pwjones1969
* [Simon](https://github.com/SimonNitzsche)
* ALL OF THE NETDEVIL AND LEGO TEAMS!
