# DarkflameServer SimClient

A simulation client for testing and load testing the DarkflameServer.

## Overview

The SimClient is a headless client that can simulate multiple player connections to test server performance, stability, and functionality. It's designed for:

- **Load Testing**: Simulate many concurrent users
- **Performance Testing**: Measure server response under load  
- **Regression Testing**: Automated testing of server functionality
- **Development Testing**: Quick connectivity and feature testing

## Features

- **Multiple Client Simulation**: Spawn many simulated clients
- **Configurable Behaviors**: Movement, chat, random actions
- **Network Statistics**: Track packets, bytes, connection stats
- **Flexible Configuration**: File-based and command-line configuration
- **Different Test Modes**: Basic, load, stress, movement, chat tests
- **Real-time Monitoring**: Live statistics and status updates

## Building

The SimClient is built as part of the normal DarkflameServer build process:

```bash
mkdir build && cd build
cmake ..
make SimClient  # or build SimClient target in Visual Studio
```

## Usage

### Basic Usage

```bash
# Run with default settings (1 client)
./SimClient

# Run load test with 10 clients
./SimClient -n 10 -t load

# Run with verbose logging
./SimClient -v -n 5

# Connect to remote server
./SimClient -a 192.168.1.100:1001 -w 192.168.1.100:2000
```

### Command Line Options

- `-h, --help` - Show help message
- `-c, --config <file>` - Configuration file (default: simclient.ini)
- `-n, --clients <count>` - Number of clients to simulate  
- `-a, --auth <ip:port>` - Auth server address
- `-w, --world <ip:port>` - World server address
- `-v, --verbose` - Enable verbose logging
- `-t, --test <type>` - Test type (basic, load, stress, movement, chat)

### Test Types

- **basic** - Single client connectivity test
- **load** - Multiple client load test  
- **stress** - High-load stress test (2x client count)
- **movement** - Movement simulation test
- **chat** - Chat functionality test

### Configuration File

Create `simclient.ini` to configure default settings:

```ini
# Server Settings
auth_server_ip=127.0.0.1
auth_server_port=1001
world_server_ip=127.0.0.1  
world_server_port=2000

# Simulation Settings
client_count=5
spawn_delay_ms=2000
tick_rate_ms=16

# Behavior Settings
enable_movement=1
enable_chat=0
enable_random_actions=1
action_interval_ms=5000

# Test Accounts
test_account_1=testuser1:testpass1
test_account_2=testuser2:testpass2
```

## Simulated Behaviors

### Movement Simulation
- Random movement within zones
- Configurable movement patterns (random, circular, linear)
- Position updates sent to server
- Collision-free pathfinding

### Chat Simulation  
- Predefined message sets (normal, spam, roleplay)
- Configurable chat intervals
- Multiple chat channels support

### Connection Management
- Automatic reconnection on failures
- Graceful disconnection handling
- Connection state tracking
- Error reporting and recovery

### Statistics Tracking
- Packets sent/received per client
- Bytes sent/received per client  
- Connection success/failure rates
- Real-time performance metrics

## Use Cases

### Load Testing
Test server performance under various loads:

```bash
# Test with 50 concurrent users
./SimClient -t load -n 50

# Stress test with 100 users
./SimClient -t stress -n 50  # Will actually use 100 (2x)
```

### Development Testing
Quick testing during development:

```bash
# Basic connectivity test
./SimClient -t basic

# Test movement system
./SimClient -t movement -n 3
```

### Automated Testing
Integrate into CI/CD pipelines:

```bash
#!/bin/bash
# Start server
./MasterServer &
./AuthServer &
./WorldServer &

sleep 5

# Run automated tests
./SimClient -t basic || exit 1
./SimClient -t load -n 10 || exit 1

# Cleanup
killall MasterServer AuthServer WorldServer
```

### Performance Monitoring
Monitor server performance over time:

```bash
# Long-running load test with statistics
./SimClient -t load -n 25 -v > load_test_$(date +%Y%m%d_%H%M%S).log
```

## Architecture

### Components

- **SimClient** - Main executable and command-line interface
- **SimConfig** - Configuration management
- **SimWorld** - Simulation coordinator and statistics  
- **SimUser** - Individual client simulation
- **SimBehavior** - Behavior patterns and actions

### Network Protocol

The SimClient implements a simplified version of the DarkflameServer client protocol:

1. **Authentication Flow**
   - Connect to AuthServer
   - Send login credentials
   - Receive session token
   - Get world server info

2. **World Connection**  
   - Connect to WorldServer
   - Send session validation
   - Load character/world data
   - Enter simulation mode

3. **Simulation Loop**
   - Send position updates
   - Process server messages  
   - Execute configured behaviors
   - Maintain connection health

### Error Handling

- Automatic retry on connection failures
- Graceful degradation on partial failures
- Detailed error logging and reporting
- Recovery strategies for different error types

## Troubleshooting

### Common Issues

**Connection Refused**
- Ensure servers are running and accessible
- Check firewall settings
- Verify IP addresses and ports

**Authentication Failures** 
- Verify test account credentials
- Check database connectivity
- Review auth server logs

**High Memory Usage**
- Reduce client count for available RAM
- Monitor for memory leaks
- Use 64-bit build for large tests

**Performance Issues**
- Adjust tick rate and spawn delays
- Monitor server resource usage  
- Check network bandwidth limits

### Debugging

Enable verbose logging to see detailed operations:

```bash
./SimClient -v -n 1 -t basic
```

Check log files in the `logs/` directory for detailed information.

## Contributing

When adding new features to SimClient:

1. Follow existing code patterns
2. Add appropriate error handling
3. Update configuration options
4. Add command-line parameters as needed
5. Update this documentation
6. Test with various scenarios

## Future Enhancements

Potential improvements for SimClient:

- **Protocol Coverage**: Support more game messages and features
- **Behavior Scripting**: Lua or Python scripting for custom behaviors  
- **Performance Profiling**: Built-in server performance monitoring
- **Web Interface**: Real-time web dashboard for monitoring
- **Distributed Testing**: Coordinate multiple SimClient instances
- **Recording/Playback**: Record real client sessions and replay them
