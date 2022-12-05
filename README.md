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
We do not recommend hosting public servers. Darkflame Universe is intended for small scale deployment, for example within a group of friends. It has not been tested for large scale deployment which comes with additional security risks.

### Supply of resource files
Darkflame Universe is a server emulator and does not distribute any LEGO® Universe files. A separate game client is required to setup this server emulator and play the game, which we cannot supply. Users are strongly suggested to refer to the safe checksums listed [here](#verifying-your-client-files) to see if a client will work.

## Steps to setup server
* [Clone this repository](#clone-the-repository)
* [Install dependencies](#install-dependencies)
* [Database setup](#database-setup)
* [Build the server](#build-the-server)
* [Configuring your server](#configuring-your-server)
	* [Required Configuration](#required-configuration)
	* [Optional Configuration](#optional-configuration)
* [Verify your setup](#verify-your-setup)
* [Running the server](#running-the-server)
* [User Guide](#user-guide)

## Clone the repository
If you are on Windows, you will need to download and install git from [here](https://git-scm.com/download/win)

Then run the following command
```bash
git clone --recursive https://github.com/DarkflameUniverse/DarkflameServer
```

## Install dependencies

### Windows packages
Ensure that you have either the [MSVC](https://visualstudio.microsoft.com/vs/) (recommended) or the [Clang](https://github.com/llvm/llvm-project/releases/) compiler installed.
You'll also need to download and install [CMake](https://cmake.org/download/) (version <font size="4">**CMake version 3.18**</font> or later!).

### MacOS packages
Ensure you have [brew](https://brew.sh) installed.
You will need to install the following packages
```bash
brew install cmake gcc mariadb openssl zlib
```

### Linux packages
Make sure packages like `gcc`, and `zlib` are installed. Depending on the distribution, these packages might already be installed. Note that on systems like Ubuntu, you will need the `zlib1g-dev` package so that the header files are available. `libssl-dev` will also be required as well as `openssl`. You will also need a MySQL database solution to use. We recommend using `mariadb-server`.

For Ubuntu, you would run the following commands. On other systems, the package install command will differ.

```bash
sudo apt update && sudo apt upgrade

# Install packages
sudo apt install build-essential gcc zlib1g-dev libssl-dev openssl mariadb-server cmake
```

#### Required CMake version
This project uses <font size="4">**CMake version 3.18**</font> or higher and as such you will need to ensure you have this version installed.
You can check your CMake version by using the following command in a terminal.
```bash
cmake --version
```

If you are going to be using an ubuntu environment to run the server, you may need to get a more recent version of `cmake` than the packages available may provide.

The general approach to do so would be to obtain a copy of the signing key and then add the CMake repository to your apt.
You can do so with the following commands.

[Source of the below commands](https://askubuntu.com/questions/355565/how-do-i-install-the-latest-version-of-cmake-from-the-command-line)

```bash
# Remove the old version of CMake
sudo apt purge --auto-remove cmake

# Prepare for installation
sudo apt update && sudo apt install -y software-properties-common lsb-release && sudo apt clean all

# Obtain a copy of the signing key
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null

# Add the repository to your sources list.
sudo apt-add-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main"

# Next you'll want to ensure that Kitware's keyring stays up to date
sudo apt update
sudo apt install kitware-archive-keyring
sudo rm /etc/apt/trusted.gpg.d/kitware.gpg

# If sudo apt update above returned an error, copy the public key at the end of the error message and run the following command
# if the error message was "The following signatures couldn't be verified because the public key is not available: NO_PUBKEY 6AF7F09730B3F0A4"
# then the below command would be "sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 6AF7F09730B3F0A4"
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys <TheCopiedPublicKey>

# Finally update and install
sudo apt update
sudo apt install cmake
```

## Database setup
First you'll need to start MariaDB.

For Windows the service is always running by default.

For MacOS, run the following command
```bash
brew services start mariadb
```

For Linux, run the following command
```bash
sudo systemctl start mysql
# If systemctl is not a known command on your distribution, try the following instead
sudo service mysql start
```

<font size="4">**You will need to run this command every time you restart your environment**</font>

Once MariaDB is started, you'll need to create a user and an empty database for Darkflame Universe to use.

First, login to the MariaDB instance.

To do this on Ubuntu/Linux, MacOS, or another Unix like operating system, run the following command in a terminal
```bash
# Logs you into the MariaDB instance as root
sudo mysql
```

For Windows, run the following command in the `Command Prompt (MariaDB xx.xx)` terminal
```bash
# Logs you into the mysql instance
mysql -u root -p
# You will then be prompted for the password you set for root during installation of MariaDB
```

Now that you are logged in, run the following commands.

```bash
# Creates a user for this computer which uses a password and grant said user all privileges.
# Change mydarkflameuser to a custom username and password to a custom password. 
GRANT ALL ON *.* TO 'mydarkflameuser'@'localhost' IDENTIFIED BY 'password' WITH GRANT OPTION;
FLUSH PRIVILEGES;

# Then create a database for Darkflame Universe to use.
CREATE DATABASE darkflame;
```

## Build the server
You can either run `build.sh` when in the root folder of the repository:

```bash
./build.sh
```

Or manually run the commands used in [build.sh](build.sh).

### Notes
Depending on your operating system, you may need to adjust some pre-processor defines in [CMakeVariables.txt](./CMakeVariables.txt) before building:
* If you are on MacOS, ensure OPENSSL_ROOT_DIR is pointing to the openssl root directory.
* If you are using a Darkflame Universe client, ensure NET_VERSION is changed to 171023.

## Configuring your server
This server has a few steps that need to be taken to configure the server for your use case.

### Required Configuration
Darkflame Universe can run with either a packed or an unpacked client.
Navigate to `build/sharedconfig.ini` and fill in the following fields:
* `mysql_host` (This is the IP address or hostname of your MariaDB server.  This is highly likely `localhost`)
* `mysql_database` (This is the database you created for the server)
* `mysql_username` (This is the user you created for the server)
* `mysql_password` (This is the password for the user you created for the server)
* `client_location` (This is the location of the client files.  This should be the folder path of a packed or unpacked client)

### Optional Configuration
* After the server has been built there should be five `ini` files in the build directory: `sharedconfig.ini`, `authconfig.ini`, `chatconfig.ini`, `masterconfig.ini`, and `worldconfig.ini`.
* `authconfig.ini` contains an option to enable or disable play keys on your server. Do not change the default port for auth.
* `chatconfig.ini` contains a port option.
* `masterconfig.ini` contains options related to permissions you want to run your servers with.
* `sharedconfig.ini` contains several options that are shared across all servers
* `worldconfig.ini` contains several options to turn on QOL improvements should you want them. If you would like the most vanilla experience possible, you will need to turn some of these settings off.

## Verify your setup
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

## Running the server
If everything has been configured correctly you should now be able to run the `MasterServer` binary which is located in the `build` directory. Darkflame Universe utilizes port numbers under 1024, so under Linux you either have to give the `AuthServer` binary network permissions or run it under sudo.
To give `AuthServer` network permissions and not require sudo, run the following command
```bash
sudo setcap 'cap_net_bind_service=+ep' AuthServer
```
and then go to `build/masterconfig.ini` and change `use_sudo_auth` to 0.

### First admin user
Run `MasterServer -a` to get prompted to create an admin account. This method is only intended for the system administrator as a means to get started, do NOT use this method to create accounts for other users!

### Account management tool (Nexus Dashboard)
**If you are just using this server for yourself, you can skip setting up Nexus Dashboard**

Follow the instructions [here](https://github.com/DarkflameUniverse/NexusDashboard) to setup the DLU Nexus Dashboard web application. This is the intended way for users to create accounts.

### Admin levels
The admin level, or Game Master level (hereafter referred to as gmlevel), is specified in the `accounts.gm_level` column in the MySQL database. Normal players should have this set to `0`, which comes with no special privileges. The system administrator will have this set to `9`, which comes will all privileges. gmlevel `8` should be used to give a player a majority of privileges without the safety critical once.

While a character has a gmlevel of anything but `0`, some gameplay behavior will change. When testing gameplay, you should always use a character with a gmlevel of `0`.

# User guide
Some changes to the client `boot.cfg` file are needed to play on your server.

## Updating your server
To update your server to the latest version navigate to your cloned directory
```bash
cd /path/to/DarkflameServer
```
Run the following commands to update to the latest changes
```bash
git pull
git submodule update --init --recursive
```
Now follow the [build](#build-the-server) section for your system and your server is up to date.

## Allowing a user to connect to your server
To connect to a server follow these steps:
* In the client directory, locate `boot.cfg`
* Open it in a text editor and locate where it says `AUTHSERVERIP=0:`
* Replace the contents after to `:` and the following `,` with what you configured as the server's public facing IP. For example `AUTHSERVERIP=0:localhost` for locally hosted servers
* Launch `legouniverse.exe`, through `wine` if on a Unix-like operating system
* Note that if you are on WSL2, you will need to configure the public IP in the server and client to be the IP of the WSL2 instance and not localhost, which can be found by running `ifconfig` in the terminal. Windows defaults to WSL1, so this will not apply to most users.

## Brick-By-Brick building
Should you choose to do any brick building, you will want to have some form of a http server that returns a 404 error since we are unable to emulate live User Generated Content at the moment. If you attempt to do any brick building without a 404 server running properly, you will be unable to load into your game. Python is the easiest way to do this, but any thing that returns a 404 should work fine.
* Note: the client hard codes this request on port 80.

<font size="4">**If you do not plan on doing any Brick Building, then you can skip this step.**</font>

The easiest way to do this is to install [python](https://www.python.org/downloads/).

### Allowing a user to build in Brick-by-Brick mode
Brick-By-Brick building requires `PATCHSERVERIP=0:` and `UGCSERVERIP=0:` in the `boot.cfg` to point to a HTTP server which always returns `HTTP 404 - Not Found` for all requests. This can be most easily achieved by pointing both of those variables to `localhost` while having running in the background.
Each client must have their own 404 server running if they are using a locally hosted 404 server.
```bash
# If on linux run this command. Because this is run on a port below 1024, binary network permissions are needed.
sudo python3 -m http.server 80

# If on windows one of the following will work when run through Powershell or Command Prompt assuming python is installed
python3 -m http.server 80
python http.server 80
py -m http.server 80
```

## In-game commands
* A list of all in-game commands can be found [here](./docs/Commands.md).

## Verifying your client files

### LEGO® Universe 1.10.64
To verify that you are indeed using a LEGO® Universe 1.10.64 client, make sure you have the full client compressed **in a rar file** and run the following command.
```bash
# Replace <file> with the file path to the zipped client

# If on Linux or MacOS
shasum -a 256 <file>

# If on Windows using the Command Prompt
certutil -hashfile <file> SHA256
```

Below are known good SHA256 checksums of the client:
* `8f6c7e84eca3bab93232132a88c4ae6f8367227d7eafeaa0ef9c40e86c14edf5` (packed client, rar compressed)
* `c1531bf9401426042e8bab2de04ba1b723042dc01d9907c2635033d417de9e05` (packed client, includes extra locales, rar compressed)
* `0d862f71eedcadc4494c4358261669721b40b2131101cbd6ef476c5a6ec6775b` (unpacked client, includes extra locales, rar compressed)

If the returned hash matches one of the lines above then you can continue with setting up the server. If you are using a fully downloaded and complete client from live, then it will work, but the hash above may not match. Otherwise you must obtain a full install of LEGO® Universe 1.10.64.

### Darkflame Universe Client
Darkflame Universe clients identify themselves using a higher version number than the regular live clients out there.
This was done make sure that older and incomplete clients wouldn't produce false positive bug reports for us, and because we made bug fixes and new content for the client.

To verify that you are indeed using a Darkflame Universe client, make sure you have the full client compressed **in a zip file** and run the following command.

```bash
# Replace <file> with the file path to the zipped client

# If on Linux or MacOS
shasum -a 1 <file>

# If on Windows using the Command Prompt
certutil -hashfile <file> SHA1
```

Known good *SHA1* checksum of the Darkflame Universe client:
- `91498e09b83ce69f46baf9e521d48f23fe502985` (packed client, zip compressed)


# Credits
## Active Contributors
* [EmosewaMC](https://github.com/EmosewaMC)
* [Jettford](https://github.com/Jettford)
* [Aaron K.](https://github.com/aronwk-aaron)

## Darkflame Universe Team
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
