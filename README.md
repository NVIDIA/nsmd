# nsmd - Nvidia System Management Daemon

## How to build

### Install dependencies

```bash
sudo apt install build-essential gcc-13 g++-13 python3-dev nlohmann-json3-dev
pip install --user meson ninja
```

#### Install Boost

> sudo apt install libboost1.83-all-dev # for Ubuntu 22.04

or
> sudo apt install libboost1.84-all-dev # for Ubuntu 24.04

or if it not installed, download and install it from source.

```bash
wget https://downloads.sourceforge.net/project/boost/boost/1.84.0/boost_1_84_0.tar.gz
tar -xzf boost_1_84_0.tar.gz
cd boost_1_84_0
./bootstrap.sh --prefix=/usr/local
./b2 install
```

#### Copy libmctp header for local development

> git archive --remote=ssh://git@gitlab-master.nvidia.com:12051/dgx/bmc/libmctp.git develop libmctp-externals.h | tar -x -C common/

### Configure and build with Meson

```bash
# Configure Meson build with debug options and compiler flags (copied from openbmc-build-scripts repo)
meson setup --reconfigure -Db_sanitize=address,undefined -Db_lundef=true -Dwerror=true -Dwarning_level=3 -Db_colorout=never -Ddebug=true -Doptimization=g -Dcpp_args="-DBOOST_USE_VALGRIND -Wno-error=invalid-constexpr -Wno-invalid-constexpr -Werror=uninitialized -Wno-error=maybe-uninitialized -Werror=strict-aliasing" builddir
# Build all targets
ninja -C builddir
```

### Build and run unit tests

```bash
# Run all unit tests
meson test -C builddir
# Run specific unit test
meson test -C builddir nsmChassis_test
```

### Troubleshooting Build Issues

#### sdbusplus Version Mismatch
If you encounter `sdbusplus` build errors, verify that the revision in `subprojects/sdbusplus.wrap` matches the version specified in the [openbmc-build-scripts](https://gitlab-master.nvidia.com/dgx/bmc/openbmc-build-scripts/-/blob/develop/scripts/build-unit-test-docker#L273) repository. Version mismatches can cause build failures.

#### Updating Subproject Dependencies
For other subproject-related errors, you can update all subproject repositories to their latest commits using:

```
cd subprojects

find -L . -type d -name ".git" | while read gitdir; do
    repo=$(dirname "$gitdir")
    echo "Pulling updates in $repo"
    cd "$repo"
    git pull
    cd - > /dev/null
done
```

## Unit Tests Debugging

### Debugging with GDB in console

```bash
# Debug all tests
meson test -C builddir --gdb

# Debug specific test
meson test -C builddir nsmChassis_test --gdb
```

### Debugging with GDB in VSCode/Cursor

1. Configure launch.json

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug file with Meson",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/builddir/${relativeFileDirname}/${fileBasenameNoExtension}",
            "cwd": "${workspaceFolder}/builddir/${relativeFileDirname}",
            "preLaunchTask": "Compile meson test"
        }
    ]
}
```

2. Configure tasks.json

```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Compile meson test",
            "type": "shell",
            "command": "meson compile -C builddir ${fileBasenameNoExtension}",
            "group": "build",
        }
    ]
}
```

3. Open the unit test file you want to debug in VSCode/Cursor
4. Set breakpoints in the code where needed
5. Press F5 to start debugging the test


## Installing clang-format-19 for CI Usage

To ensure code consistency and formatting standards in the CI pipeline, `clang-format-19` needs to be installed. Follow the steps below to install `clang-format-19` on your system:

```bash
# Update the package list
sudo apt update

# Install clang-format-19
sudo apt install clang-format-19
```

This will install `clang-format-19` on your system, enabling it for use in the CI pipeline.

### Using clang-format-19 for all changed files before commit

To automatically format your code before each commit, create a pre-commit hook with the following steps:
```
cat > .git/hooks/pre-commit << EOL
#!/bin/sh

# Get list of staged files that are C/C++ source files
files=$(git diff --cached --name-only --diff-filter=ACMR | grep ".*\.[ch]\(pp\)\?$")

if [ -n "$files" ]; then
    # Format the files
    clang-format-19 -i $files
    
    # Add the formatted files back to staging
    git add $files
    
    # Check if any files were modified after formatting
    if ! git diff --cached --quiet; then
        echo "Formatted C/C++ files were automatically fixed up"
    fi
fi

exit 0
EOL
chmod +x .git/hooks/pre-commit
```

## Progress Counters

The NSM daemon tracks various sensor polling operations using progress counters. These counters are stored in a memory-mapped file descriptor (memfd) and can be accessed via D-Bus for duming, monitoring and debugging purposes.

### Counter Types and When They Are Incremented

Each counter type tracks a specific aspect of sensor polling operations:

#### 1. **Priority**
- **Description**: Tracks successful updates of priority sensors
- **When incremented**: After each successful priority sensor update during the priority polling phase (every 150ms)
- **Location**: `sensorManager.cpp::pollPrioritySensors()`
- **Purpose**: Monitor high-frequency critical sensor updates

#### 2. **GpuPerformanceMonitoring**
- **Description**: Tracks GPU Performance Monitoring (GPM) sensor updates
- **When incremented**: After each successful GPM sensor update (NVDEC, NVJPG utilization metrics)
- **Polling interval**: 1000ms
- **Location**: `nsmGpmOemFactory.cpp` when creating GPM sensors
- **Purpose**: Monitor GPU-specific performance metric collection

#### 3. **LongRunning**
- **Description**: Tracks completion of long-running sensor operations
- **When incremented**: After a long-running sensor operation completes
- **Location**: `sensorManager.cpp::updateLongRunningSensor()`
- **Purpose**: Monitor operations that may take extended time and potentially return events as second responses (e.g., throttle duration sensors)

#### 4. **Static**
- **Description**: Tracks one-time static sensor updates
- **When incremented**: After each static sensor update
- **Location**: `sensorManager.cpp::pollNonPrioritySensors()` when `pollingType == Static`
- **Purpose**: Monitor sensors with values that don't change during runtime (polled once and removed from queue upon success)

#### 5. **RoundRobin**
- **Description**: Tracks non-priority sensor updates in round-robin fashion
- **When incremented**: After each non-priority sensor update during round-robin polling
- **Location**: `sensorManager.cpp::pollNonPrioritySensors()` when `pollingType == RoundRobin`
- **Purpose**: Monitor sensors polled in circular queue fashion when time permits after priority sensors

#### 6. **PriorityTimeExceeded**
- **Description**: Tracks when priority polling exceeds its time window
- **When incremented**: When priority sensor polling takes longer than `SENSOR_POLLING_TIME` (typically 150ms)
- **Location**: `sensorManager.cpp::pollPrioritySensors()` when `(t1 - t0) > pollingTimeInUsec`
- **Purpose**: Detect performance issues where priority polling is taking too long and may affect system responsiveness

#### 7. **PostPatch**
- **Description**: Tracks post-patch I/O operations
- **When incremented**: After each post-patch I/O operation on the device
- **Location**: `nsmDevice.cpp::postPatchIO()`
- **Purpose**: Monitor operations that occur after device firmware updates or patches to verify device state

#### 8. **Event**
- **Description**: Tracks NSM event processing
- **When incremented**: After each NSM event is received and processed by the event dispatcher
- **Location**: `nsmEvent.cpp::DelegatingEventHandler::delegate()`
- **Purpose**: Monitor asynchronous notifications from devices (e.g., long-running operation completion, state changes)

#### 9. **Error**
- **Description**: Tracks failed operations (excluding timeouts)
- **When incremented**: When any sensor update or operation fails with an error code other than `NSM_SUCCESS` or `NSM_SW_ERROR_TIMEOUT`
- **Location**: `progressCounters.cpp::increment()` when `rc != NSM_SUCCESS` and `rc != NSM_SW_ERROR_TIMEOUT`
- **Purpose**: Monitor general error conditions during polling operations

#### 10. **Timeout**
- **Description**: Tracks timeout errors
- **When incremented**: When a sensor update or operation times out (`NSM_SW_ERROR_TIMEOUT`)
- **Location**: `progressCounters.cpp::increment()` when `rc == NSM_SW_ERROR_TIMEOUT`
- **Purpose**: Monitor operations where devices did not respond within the expected time window

### Configuration Options

Progress counters can be configured via meson options:

- `progressCounter`: Enable/disable progress counter functionality (default: `enabled`)
- `sensor-progress-counters-dump-count-threshold`: Number of counter updates before dumping to memfd (default: `100000`)
- `sensor-progress-counters-dump-time-threshold`: Time threshold in microseconds before dumping (default: `600000000` = 10 minutes)
- `sensor-progress-counters-memfd-size`: Size of the memory-mapped file in bytes (default: `65536`)

### Accessing Counter Data

Counter data is exposed via D-Bus at:
```
/xyz/openbmc_project/progress_counters/<device_eid>
```

Use the `nsmProgressCountersReader` tool to read counter data:
```bash
# Read counters for all devices
nsmProgressCountersReader

# Read counters for specific device
nsmProgressCountersReader <device_eid>
```

### Adding Support for a New Counter Type

To add a new progress counter type, follow these steps:

#### 1. Update the Enum Definition
Add your new counter type to `nsmd/nsmProgressCounters/progressCounterType.hpp`:

```cpp
enum class ProgressCounterType
{
    Priority,
    GpuPerformanceMonitoring,
    // ... existing counters ...
    YourNewCounter,  // Add here, before EnumCount (must be last)
    EnumCount,
};
```

**Important**: Always add new counters before `EnumCount`, as `EnumCount` must remain the last entry for the `CountersCount` calculation.

#### 2. Update Documentation
Add comprehensive documentation for your new counter in the `nsmd/nsmProgressCounters/progressCounterType.hpp` file:

```cpp
/**
 * @brief Your new counter description
 *
 * Incremented when: Describe when this counter is incremented
 *
 * Location: File.cpp::functionName()
 */
YourNewCounter,
```

Add your new counter to the "Counter Types and When They Are Incremented" section in this README with:
- Description
- When it's incremented
- Location in code
- Purpose

#### 3. Update Counter Names Map
Add your counter name to the `counterNames` array in `nsmd/nsmProgressCounters/progressCounterReader.cpp`:

```cpp
static constexpr std::array<std::string_view, CountersCount> counterNames = {
    "Priority",  "GPM",        "LongRunning",
    "Static",    "RoundRobin", "PriorityTimeExceeded",
    "PostPatch", "Event",      "Error",
    "Timeout",   "YourNewCounter",  // Add your counter name here
};
```

**Important**: The order must match the enum order in `ProgressCounterType`. This array is used by `nsmProgressCountersReader` to display counter names in CSV output.

#### 4. Increment the Counter
In the appropriate location in your code, increment the counter:

```cpp
// For successful operations
nsmDevice->progressCounters.increment(ProgressCounterType::YourNewCounter, rc, timestamp);

// Or directly without return code checking
nsmDevice->progressCounters.increment(ProgressCounterType::YourNewCounter, timestamp);
```

### Data Structure

Counters are stored in a packed structure for efficient memory usage:

```cpp
struct __attribute__((packed)) CounterDataRow
{
    uint32_t key;           // Iteration/dump key
    uint64_t timestamp;     // Timestamp in microseconds
    CountersArray counters; // Array of counter values
};
```

The data rotates in the memfd using `key % maxRows` to ensure bounded memory usage.



## Artifacts

Successful build should generate three binary artifacts.

1. nsmd (NSM Daemon)
2. nsmtool (NSM Requester utility) 
3. nsmMockupResponder (NSM Endpoint Mockup Responder) 

### nsmd

A Daemon that can discover NSM endpoint, gather telemetry data from the
endpoints, and can publish them to D-Bus or similar IPC services, for consumer
services like bmcweb.

### nsmtool

nsmtool is a client tool that acts as a NSM requester which can be invoked from
the BMC. nsmtool sends the request message and parse the response message &
display it in readable format.

### nsmMockupResponder

A mockup NSM responder that can be used for development purpose. Its primary
usage is to test nsmd and nsmtool features on an emulator like QEMU.

Follow this steps to run nsmMockupResponder:
Step 1 -
On the QEMU instance, restart the `nsmd` service.

Step 2
Assign an address to the loopback (`lo`) interface
$ mctp addr add 12 dev lo

Step 3
Immediately start the mock responder using the assigned address
$ nsmMockupResponder -v -d Baseboard -i 0 -e 12

Run Step 3 right after Step 2. If there is any delay, nsmd will fail to detect
the endpoint. If detection fails, repeat all steps from the beginning.
