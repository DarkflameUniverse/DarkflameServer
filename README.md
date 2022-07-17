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
Darkflame Universe is a server emulator and does not distribute any LEGO® Universe files. A separate game client is required to setup this server emulator and play the game, which we cannot supply. Users are strongly suggested to refer to the safe checksums listed in the resources tab below when checking if a client will work.

## Build
Development of the latest iteration of Darkflame Universe has been done primarily in a Unix-like environment and is where it has been tested and designed for deployment. It is therefore highly recommended that Darkflame Universe be built and deployed using a Unix-like environment for the most streamlined experience.

### Prerequisites
**Clone the repository**
```bash
git clone --recursive https://github.com/DarkflameUniverse/DarkflameServer
```
**Python**

Some tools utilized to streamline the setup process require Python 3, make sure you have it installed.


### Choosing the right version for your client
DLU clients identify themselves using a higher version number than the regular live clients out there.
This was done make sure that older and incomplete clients wouldn't produce false positive bug reports for us, and because we made bug fixes and new content for the client. 

If you're using a DLU client you'll have to go into the "CMakeVariables.txt" file and change the NET_VERSION variable to 171023 to match the modified client's version number.

### Linux builds
Make sure packages like `gcc`, `cmake`, and `zlib` are installed. Depending on the distribution, these packages might already be installed. Note that on systems like Ubuntu, you will need the `zlib1g-dev` package so that the header files are available. `libssl-dev` will also be required as well as `openssl`.

CMake must be version 3.14 or higher!

**Build the repository**

You can either run `build.sh` when in the root folder of the repository:

```bash
./build.sh
```

Or manually run the commands used in `build.sh`:

```bash
# Create the build directory, preserving it if it already exists
mkdir -p build
cd build

# Run CMake to generate make files
cmake ..

# Run make to build the project. To build utilizing multiple cores, append `-j` and the amount of cores to utilize, for example `make -j8`
make
```

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

**Build the repository**
```batch
:: Create the build directory
mkdir build
cd build

:: Run CMake to generate make files
cmake ..

:: Run CMake with build flag to build
cmake --build . --config Release
```
**Windows for ARM** has not been tested but should build by doing the following
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

**Open the Command Prompt application with Administrator permissions and run the following:**
```bash
# Installing Windows Subsystem for Linux
wsl --install
```

**Open the Ubuntu application and run the following:**
```bash
# Make sure the install is up to date
apt update && apt upgrade

# Make sure the gcc, cmake, and build-essentials are installed
sudo apt install gcc
sudo apt install cmake
sudo apt install build-essential
```

[**Follow the Linux instructions**](#linux-builds)

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

**LEGO® Universe 1.10.64**

This repository does not distribute any LEGO® Universe files. A full install of LEGO® Universe version 1.10.64 (latest) is required to finish setting up Darkflame Universe.

Known good SHA256 checksums of the client:
- `8f6c7e84eca3bab93232132a88c4ae6f8367227d7eafeaa0ef9c40e86c14edf5` (packed client, rar compressed)
- `c1531bf9401426042e8bab2de04ba1b723042dc01d9907c2635033d417de9e05` (packed client, includes extra locales, rar compressed)
- `0d862f71eedcadc4494c4358261669721b40b2131101cbd6ef476c5a6ec6775b` (unpacked client, includes extra locales, rar compressed)

Known good *SHA1* checksum of the DLU client:
- `91498e09b83ce69f46baf9e521d48f23fe502985` (packed client, zip compressed) 

How to generate a SHA256 checksum:
```bash
# Replace <file> with the file path to the client

# If on Linux or MacOS
shasum -a 256 <file>

# If on Windows
certutil -hashfile <file> SHA256
```

**Unpacking the client**
* Clone lcdr's utilities repository [here](https://github.com/lcdr/utils)
* Use `pkextractor.pyw` to unpack the client files if they are not already unpacked

**Setup resource directory**
* In the `build` directory create a `res` directory if it does not already exist.
* Copy over or create symlinks from `macros`, `BrickModels`, `chatplus_en_us.txt`, `names`, and `maps` in your client `res` directory to the server `build/res` directory
* Unzip the navmeshes [here](./resources/navmeshes.zip) and place them in `build/res/maps/navmeshes`

**Setup locale**
* In the `build` directory create a `locale` directory if it does not already exist
* Copy over or create symlinks from `locale.xml` in your client `locale` directory to the `build/locale` directory

**Client database**
* Use `fdb_to_sqlite.py` in lcdr's utilities on `res/cdclient.fdb` in the unpacked client to convert the client database to `cdclient.sqlite`
* Move and rename `cdclient.sqlite` into `build/res/CDServer.sqlite`
* Run each SQL file in the order at which they appear [here](migrations/cdserver/) on the SQLite database

### Database
Darkflame Universe utilizes a MySQL/MariaDB database for account and character information.

Initial setup can vary drastically based on which operating system or distribution you are running; there are instructions out there for most setups, follow those and come back here when you have a database up and running.
* Create a database for Darkflame Universe to use
* Use the command `./MasterServer -m` to automatically run them.

**Configuration**

After the server has been built there should be four `ini` files in the build director: `authconfig.ini`, `chatconfig.ini`, `masterconfig.ini`, and `worldconfig.ini`. Go through them and fill in the database credentials and configure other settings if necessary.

**Verify**

Your build directory should now look like this:
* AuthServer
* ChatServer
* MasterServer
* WorldServer
* authconfig.ini
* chatconfig.ini
* masterconfig.ini
* worldconfig.ini
* **locale/**
  * locale.xml
* **res/**
  * CDServer.sqlite
  * chatplus_en_us.txt
  * **macros/**
    * ...
  * **BrickModels/**
    * ...
  * **maps/**
    * **navmeshes/**
      * ...
    * ...
* ...

## Running the server
If everything has been configured correctly you should now be able to run the `MasterServer` binary. Darkflame Universe utilizes port numbers under 1024, so under Linux you either have to give the binary network permissions or run it under sudo.

### First admin user
Run `MasterServer -a` to get prompted to create an admin account. This method is only intended for the system administrator as a means to get started, do NOT use this method to create accounts for other users!

### Account Manager

Follow the instructions [here](https://github.com/DarkflameUniverse/AccountManager) to setup the DLU account management Python web application. This is the intended way for users to create accounts.

### Admin levels

The admin level, or game master level, is specified in the `accounts.gm_level` column in the MySQL database. Normal players should have this set to `0`, which comes with no special privileges. The system administrator will have this set to `9`, which comes will all privileges. Admin level `8` should be used to give a player a majority of privileges without the safety critical once.

While a character has a gmlevel of anything but 0, some gameplay behavior will change. When testing gameplay, you should always use a character with a gmlevel of 0.

## User guide
A few modifications have to be made to the client.

### Client configuration
To connect to a server follow these steps:
* In the client directory, locate `boot.cfg`
* Open it in a text editor and locate where it says `AUTHSERVERIP=0:`
* Replace the contents after to `:` and the following `,` with what you configured as the server's public facing IP. For example `AUTHSERVERIP=0:localhost` for locally hosted servers
* Launch `legouniverse.exe`, through `wine` if on a Unix-like operating system
* Note that if you are on WSL2, you will need to configure the public IP in the server and client to be the IP of the WSL2 instance and not localhost, which can be found by running `ifconfig` in the terminal. Windows defaults to WSL1, so this will not apply to most users.

### Survival

The client script for the survival minigame has a bug in it which can cause the minigame to not load. To fix this, follow these instructions:
* Open `res/scripts/ai/minigame/survival/l_zone_survival_client.lua`
* Navigate to line `617`
* Change `PlayerReady(self)` to `onPlayerReady(self)`
* Save the file, overriding readonly mode if required

If you still experience the bug, try deleting/renaming `res/pack/scripts.pk`.

### Brick-By-Brick building

Brick-By-Brick building requires `PATCHSERVERIP=0:` in the `boot.cfg` to point to a HTTP server which always returns `HTTP 404 - Not Found` for all requests. This can be achieved by pointing it to `localhost` while having `sudo python -m http.server 80` running in the background.

### In-game commands
Here is a summary of the commands available in-game. All commands are prefixed by `/` and typed in the in-game chat window. Some commands requires admin privileges. Operands within `<>` are required, operands within `()` are not. For the full list of in-game commands, please checkout [the source file](./dGame/dUtilities/SlashCommandHandler.cpp).

<table>
<thead>
  <th>
    Command
  </th>
  <th>
    Usage
  </th>
  <th>
    Description
  </th>
  <th>
    Admin Level Requirement
  </th>
</thead>
<tbody>
  <tr>
    <td>
      info
    </td>
    <td>
      /info
    </td>
    <td>
      Displays server info to the user, including where to find the server's source code.
    </td>
    <td>
    </td>
  </tr>
  <tr>
    <td>
      credits
    </td>
    <td>
      /credits
    </td>
    <td>
      Displays the names of the people behind Darkflame Universe.
    </td>
    <td>
    </td>
  </tr>
  <tr>
    <td>
      instanceinfo
    </td>
    <td>
      /instanceinfo
    </td>
    <td>
      Displays in the chat the current zone, clone, and instance id. 
    </td>
    <td>
    </td>
  </tr>
  <tr>
    <td>
      gmlevel
    </td>
    <td>
      /gmlevel &#60;level&#62;
    </td>
    <td>
      Within the authorized range of levels for the current account, changes the character's game master level to the specified value. This is required to use certain commands.
    </td>
    <td>
    </td>
  </tr>
  <tr>
    <td>
      testmap
    </td>
    <td>
      /testmap &#60;zone&#62; (clone-id)
    </td>
    <td>
      Transfers you to the given zone by id and clone id.
    </td>
    <td>
      1
    </td>
  </tr>
  <tr>
    <td>
      ban
    </td>
    <td>
      /ban &#60;username&#62;
    </td>
    <td>
      Bans a user from the server.
    </td>
    <td>
      4
    </td>
  </tr>
  <tr>
    <td>
      gmadditem
    </td>
    <td>
      /gmadditem &#60;id&#62; (count)
    </td>
    <td>
      Adds the given item to your inventory by id.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      spawn
    </td>
    <td>
      /spawn &#60;id&#62;
    </td>
    <td>
      Spawns an object at your location by id.
    </td>
    <td>
      8
    </td>
  </tr>
  <tr>
    <td>
      metrics
    </td>
    <td>
      /metrics
    </td>
    <td>
      Prints some information about the server's performance.
    </td>
    <td>
      8
    </td>
  </tr>
</tbody>
</table>

## Credits
## Active Contributors
* [EmosewaMC](https://github.com/EmosewaMC)
* [Jettford](https://github.com/Jettford)

## DLU Team
* [DarwinAnim8or](https://github.com/DarwinAnim8or)
* [Wincent01](https://github.com/Wincent01)
* [Mick](https://github.com/MickVermeulen)
* [averysumner](https://github.com/codeshaunted)
* [Jon002](https://github.com/jaller200)
* [Jonny](https://github.com/cuzitsjonny)
* TheMachine
* Matthew
* [Raine](https://github.com/Rainebannister)
* Bricknave

### Research and tools
* [lcdr](https://github.com/lcdr)
* [Xiphoseer](https://github.com/Xiphoseer)

### Community management
* [Neal](https://github.com/NealSpellman)

### Former contributors
* TheMachine
* Matthew
* Raine
* Bricknave

### Special thanks
* humanoid24
* pwjones1969
* BlasterBuilder for the logo
* ALL OF THE NETDEVIL AND LEGO TEAMS!
