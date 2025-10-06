# Darkflame Universe Server Development Instructions

Darkflame Universe (DLU) is a LEGO Universe server emulator written in C++ with a multi-server architecture (AuthServer, ChatServer, MasterServer, WorldServer). Always reference these instructions first and fallback to search or bash commands only when you encounter unexpected information that does not match the info here.

## Working Effectively

### Bootstrap, Build, and Test - REQUIRED STEPS
Execute these commands in order for ANY development work. NEVER CANCEL builds - they take time but work reliably:

```bash
# 1. Install system dependencies (Ubuntu/Debian)
sudo apt update && sudo apt install -y build-essential gcc zlib1g-dev libssl-dev openssl mariadb-server cmake

# 2. Initialize git submodules (CRITICAL - project won't build without this)
git submodule update --init --recursive

# 3. Build using the provided script
./build.sh -j2
```
- **Build time: ~6 minutes. NEVER CANCEL. Set timeout to 720+ seconds (12+ minutes).**
- **Uses CMake 3.25-3.31 (confirmed working with 3.31.6)**
- **Requires g++11+ (confirmed working with g++ 13.3.0)**

### Alternative Build Methods
```bash
# Using CMake presets (CI-style)
cmake --workflow --preset ci-ubuntu-22.04

# Manual CMake (for custom configurations)
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE="Release" ..
cmake --build . --config Release -j2
```
- **Same timing: ~6 minutes. NEVER CANCEL. Set timeout to 720+ seconds.**

### Run Tests
```bash
cd build
ctest --output-on-failure
```
- **Test time: <4 seconds. Set timeout to 30+ seconds.**
- **91 tests run, all should pass**
- **Tests are built automatically when ENABLE_TESTING=1 in CMakeVariables.txt**

### Database Setup (for runtime testing)
```bash
# Start MariaDB
sudo systemctl start mysql

# Create test database and user
sudo mysql -e "CREATE USER 'testuser'@'localhost' IDENTIFIED BY 'testpass'; GRANT ALL ON *.* TO 'testuser'@'localhost' WITH GRANT OPTION; FLUSH PRIVILEGES; CREATE DATABASE testdarkflame;"
```

## Validation

### Build Validation
- **ALWAYS run the bootstrapping steps first** before making any code changes
- **ALWAYS build and test your changes** before considering them complete
- Build output should include all server binaries: AuthServer, ChatServer, MasterServer, WorldServer
- Build directory contains required files: `*.ini` configs, `navmeshes/`, `migrations/`, `vanity/`, `blocklist.dcf`, `libmariadbcpp.so`

### Runtime Validation
The servers can be started for basic validation:
```bash
cd build
./MasterServer
```
- **Server will start but complain about missing client files (this is expected)**
- **Database connections work with proper configuration in sharedconfig.ini**
- **For full server testing, LEGO Universe client files are required (not available in this repository)**

### Code Validation
**ALWAYS validate your changes by**:
1. Building successfully with no new compilation errors
2. Running the test suite and confirming all tests pass
3. Starting MasterServer to verify basic functionality
4. **Use .editorconfig** - code style uses tabs (width=4), Unix line endings, trailing whitespace removal

## Common Tasks

### Project Structure
```
/home/runner/work/DarkflameServer/DarkflameServer/
├── dAuthServer/          # Authentication server code
├── dChatServer/          # Chat server code  
├── dMasterServer/        # Master server (main coordinator)
├── dWorldServer/         # World/game server code
├── dCommon/              # Shared common utilities
├── dDatabase/            # Database abstraction layer
├── dGame/                # Core game logic, components, behaviors
├── dScripts/             # Game scripts (NPCs, quests, etc.)
├── dNet/                 # Network utilities
├── dPhysics/             # Physics integration
├── tests/                # Unit tests (GoogleTest)
├── migrations/           # Database schema migrations
├── thirdparty/           # External dependencies
├── build.sh              # Main build script
├── CMakeVariables.txt    # Build configuration variables
└── CMakePresets.json     # CMake preset configurations
```

### Key Files to Know
- **CMakeVariables.txt**: Build configuration (testing enabled, MariaDB jobs, etc.)
- **build/sharedconfig.ini**: Database connection, client location, server settings
- **build/masterconfig.ini**: Master server port and startup configuration
- **CONTRIBUTING.md**: Code style guidelines and commit message format
- **docs/Commands.md**: Complete list of in-game server commands

### Build Configuration
Located in `CMakeVariables.txt`:
- `ENABLE_TESTING=1` - Unit tests enabled (keep enabled)
- `MARIADB_CONNECTOR_COMPILE_JOBS=1` - Parallel compilation jobs for MariaDB connector
- `CDCLIENT_CACHE_ALL=0` - Database caching strategy

### Common Commands Reference
```bash
# Build from clean state
rm -rf build && ./build.sh -j2

# Run specific test
cd build && ctest -R "TestName" --output-on-failure

# Check which servers were built
cd build && ls -la *Server

# View build configuration
cat CMakeVariables.txt

# Check git submodules status
git submodule status
```

### Important Notes
- **Client files are NOT included** - this is only the server emulator
- **Database can use SQLite or MariaDB** - SQLite recommended for development since it's lighter and doesn't require an external service
- **Multi-server architecture** requires all 4 servers to run a complete setup
- **Network ports**: Auth (1001), Chat (2005), Master (2000), World (3000+)
- **Development uses Debug builds**, production uses Release builds
- **GM level 0** = normal player, **GM level 8-9** = admin privileges

## Troubleshooting
- **"Asset bundle not found"**: Expected without LEGO Universe client files
- **"Submodule errors"**: Run `git submodule update --init --recursive`
- **"CMake version errors"**: Requires CMake 3.25-3.31
- **"MariaDB connection errors"**: Check database setup and sharedconfig.ini
- **"Permission denied on port"**: Run `sudo setcap 'cap_net_bind_service=+ep' AuthServer` for ports <1024

### CI Information
- **GitHub Actions** runs builds on Windows, Ubuntu, and macOS  
- **Build matrix** tests multiple configurations via CMake presets
- **All tests must pass** for CI to succeed
- **Build artifacts** are automatically generated and uploaded

**Remember: This is a complex game server requiring LEGO Universe client files for full functionality, but the server has the capability to mock everything that's needed to test without the client since cdclient can be mocked and the database can be mocked as well.**
