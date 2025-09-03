# SimClient Integration Guide

This guide explains how the SimClient has been integrated into the DarkflameServer project and how to use it effectively.

## Integration Overview

The SimClient has been added as a new executable target to the existing DarkflameServer build system. It's designed to work alongside the existing server components for testing and load testing purposes.

### Files Added

#### Core SimClient Files
- `dSimClient/` - New directory containing all SimClient source code
- `dSimClient/CMakeLists.txt` - Build configuration for SimClient
- `dSimClient/SimClient.cpp` - Main executable entry point
- `dSimClient/SimUser.h/cpp` - Individual simulated user implementation
- `dSimClient/SimWorld.h/cpp` - Simulation coordinator and manager
- `dSimClient/SimBehavior.h/cpp` - Behavior patterns and test scenarios
- `dSimClient/SimConfig.h/cpp` - Configuration management
- `dSimClient/README.md` - Detailed SimClient documentation

#### Configuration and Build Files
- `resources/simclient.ini` - Default configuration file
- `build_simclient.sh` - Linux/macOS build script
- `build_simclient.bat` - Windows build script

#### Modified Files
- `CMakeLists.txt` - Added SimClient subdirectory to main build

## Build Integration

The SimClient is built as part of the normal DarkflameServer build process:

```bash
# Standard build process includes SimClient
mkdir build && cd build
cmake ..
make  # Builds all targets including SimClient

# Or build just SimClient
make SimClient
```

### Dependencies

SimClient reuses existing DarkflameServer libraries:
- `dCommon` - Common utilities and data types
- `dDatabase` - Database connectivity (minimal usage)
- `dNet` - Network communication and RakNet integration
- `dServer` - Server utilities and logging
- `raknet` - RakNet networking library

This ensures consistency with the main server codebase and reduces maintenance overhead.

## Usage Scenarios

### Development Testing

During development, developers can use SimClient to quickly test server functionality:

```bash
# Quick connectivity test
./SimClient -t basic -v

# Test new features with multiple clients
./SimClient -n 3 -t movement
```

### Continuous Integration

Integrate SimClient into CI/CD pipelines:

```yaml
# Example GitHub Actions step
- name: Test Server with SimClient
  run: |
    # Start servers in background
    ./MasterServer &
    ./AuthServer &
    ./WorldServer &
    
    # Wait for startup
    sleep 10
    
    # Run tests
    ./SimClient -t basic || exit 1
    ./SimClient -t load -n 5 || exit 1
    
    # Cleanup
    killall MasterServer AuthServer WorldServer
```

### Load Testing

Simulate realistic server loads:

```bash
# Test with 50 concurrent users
./SimClient -t load -n 50

# Stress test with high load
./SimClient -t stress -n 25  # Actually uses 50 clients (2x)

# Custom configuration for specific tests
./SimClient -c load_test.ini -t load
```

### Performance Monitoring

Monitor server performance over time:

```bash
# Long-running test with detailed logging
./SimClient -t load -n 20 -v > performance_test_$(date +%Y%m%d).log &

# Let it run for hours or days, then analyze logs
```

## Configuration Management

### File-based Configuration

The SimClient uses an INI-style configuration file that can be customized for different test scenarios:

```ini
# Basic connectivity test config
client_count=1
enable_movement=0
enable_chat=0
verbose_logging=1

# Load test config  
client_count=25
spawn_delay_ms=1000
enable_movement=1
enable_random_actions=1

# Stress test config
client_count=100
spawn_delay_ms=500
tick_rate_ms=8
```

### Command-line Overrides

Command-line parameters override configuration file settings:

```bash
# Override client count from config file
./SimClient -c load_test.ini -n 10

# Override server addresses
./SimClient -a 192.168.1.100:1001 -w 192.168.1.100:2000
```

## Testing Integration

### Unit Testing

The SimClient components can be unit tested independently:

```cpp
// Example unit test
TEST(SimConfigTest, LoadValidConfig) {
    SimConfig config;
    ASSERT_TRUE(config.LoadFromFile("test_config.ini"));
    ASSERT_EQ(config.GetClientCount(), 5);
}
```

### Integration Testing

SimClient can be used for automated integration testing:

```bash
#!/bin/bash
# Integration test script

# Test basic server startup and connectivity
./SimClient -t basic || { echo "Basic connectivity failed"; exit 1; }

# Test multiple client handling
./SimClient -t load -n 5 || { echo "Load test failed"; exit 1; }

# Test movement system
./SimClient -t movement -n 3 || { echo "Movement test failed"; exit 1; }

echo "All integration tests passed!"
```

## Error Handling and Debugging

### Logging Integration

SimClient integrates with the DarkflameServer logging system:

- Uses same Logger class as server components
- Writes to `logs/SimClient_<timestamp>.log`
- Supports verbose and debug logging levels
- Correlates with server logs for debugging

### Error Recovery

SimClient implements robust error handling:

- Automatic reconnection on network failures
- Graceful degradation when servers are overloaded
- Detailed error reporting for debugging
- State recovery after temporary issues

### Debugging Features

Enable detailed debugging with:

```bash
# Maximum verbosity
./SimClient -v -n 1 -t basic

# Debug specific client behaviors
./SimClient -t movement -n 1 -v | grep "movement"
```

## Performance Considerations

### Resource Usage

SimClient is designed to be lightweight:

- Minimal memory footprint per simulated client
- Efficient network communication
- Configurable tick rates to balance load
- Automatic cleanup of failed connections

### Scalability

For large-scale testing:

```bash
# Multiple SimClient instances for extreme load
./SimClient -n 100 &
./SimClient -n 100 &
./SimClient -n 100 &
wait

# Distributed testing across multiple machines
ssh server1 "./SimClient -n 50 -a $SERVER_IP" &
ssh server2 "./SimClient -n 50 -a $SERVER_IP" &
```

### Monitoring

Track performance metrics:

- Packets per second sent/received
- Connection success/failure rates
- Response times and latency
- Memory and CPU usage

## Future Enhancements

### Planned Features

1. **Protocol Coverage**: Support more game messages and interactions
2. **Behavior Scripting**: Lua/Python scripting for complex behaviors  
3. **Web Dashboard**: Real-time monitoring interface
4. **Distributed Coordination**: Multiple SimClient instance coordination
5. **Performance Profiling**: Built-in server performance analysis

### Extension Points

The SimClient architecture supports easy extension:

```cpp
// Add new behavior patterns
class CustomBehavior : public SimBehavior {
    void ExecuteCustomAction(SimUser* user) override {
        // Implement custom test behavior
    }
};

// Add new test scenarios
void SimWorld::RunCustomTest() {
    // Implement custom test logic
}
```

## Best Practices

### Development Workflow

1. **Start Small**: Begin with basic connectivity tests
2. **Incremental Load**: Gradually increase client counts
3. **Monitor Resources**: Watch server CPU/memory during tests
4. **Log Analysis**: Correlate SimClient and server logs
5. **Automate Testing**: Integrate into development workflow

### Production Testing

1. **Staging Environment**: Test on production-like infrastructure
2. **Gradual Rollout**: Start with small loads, increase gradually
3. **Baseline Metrics**: Establish performance baselines
4. **Failure Scenarios**: Test server behavior under failures
5. **Recovery Testing**: Verify graceful degradation and recovery

## Troubleshooting

### Common Issues

**Build Failures**
- Ensure all DarkflameServer dependencies are met
- Check CMake version and compiler compatibility
- Verify RakNet and other third-party libraries

**Connection Issues**
- Verify server components are running
- Check firewall and network connectivity
- Validate configuration file settings
- Review server logs for authentication errors

**Performance Issues**
- Reduce client count for available resources
- Adjust tick rate and spawn delays
- Monitor network bandwidth usage
- Check for memory leaks in long-running tests

### Getting Help

- Check SimClient logs in `logs/` directory
- Review server logs for corresponding errors
- Enable verbose logging for detailed diagnostics
- Correlate timestamps between SimClient and server logs

## Contributing

When contributing to SimClient:

1. Follow existing code style and patterns
2. Add comprehensive error handling
3. Update configuration options as needed
4. Add command-line parameters for new features
5. Update documentation
6. Test with various scenarios and loads
7. Consider backwards compatibility

The SimClient is designed to grow with the DarkflameServer project and can be extended to support new testing scenarios as the server evolves.
