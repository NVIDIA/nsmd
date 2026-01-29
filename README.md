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

## Logging API

The NSM daemon provides a logging framework with flood prevention capabilities to avoid overwhelming the log system with repetitive messages. This is particularly useful when errors occur repeatedly during polling operations.

### Overview

The logging framework provides two main mechanisms:

1. **`LG2_LEVEL_FLT` macros** - **PREFERRED** - Convenient logging macros with built-in flood prevention
2. **`shouldLog()` API** - Lower-level API for manual state change tracking (used by `LG2_LEVEL_FLT` internally)

**Important:** Always prefer `LG2_LEVEL_FLT` macros over manual `shouldLog()` calls unless you have a specific reason to use the lower-level API.

### When to Use Flood-Prevention Logging

Use `LG2_LEVEL_FLT` macros when:

- Logging errors in polling loops that execute frequently
- Logging errors in operations that may fail repeatedly
- You want to log when a state changes (error occurs or clears)
- You need to reduce log noise while maintaining visibility into state changes

### Class Inheritance and Usage Context

**CRITICAL:** The `shouldLog()` API is available only in classes that inherit from `NsmObject`, which inherits from `StateChangeLogger`:

```cpp
class StateChangeLogger {
    // Provides shouldLog() method
};

class NsmObject : virtual public StateChangeLogger {
    // Inherits shouldLog() method
};

class YourClass : public NsmObject {
    // Your class has access to shouldLog() via inheritance
};
```

**DO NOT** use global `shouldLog()` functions. Always use the inherited method within your class implementation. This ensures proper state tracking per object instance.

### Basic Usage Examples

#### Example 1: Using LG2_ERROR_FLT Macro (PREFERRED)

```cpp
// In a method of a class inheriting from NsmObject
Task<> NsmPort::queryPortStatus()
{
    nsm_sw_codes rc = decode_query_port_status_resp(responseMsg.get(), responseLen, 
                                                     &cc, &reasonCode, &dataSize, 
                                                     &portState, &portStatus);

    // PREFERRED: Use LG2_ERROR_FLT macro - automatically handles shouldLog()
    // The odd-indexed arguments (values in key-value pairs) are tracked for state changes
    LG2_ERROR_FLT(
        "decode_query_port_status_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode,  // logged and tracked
        "CC", cc,                   // logged and tracked
        "RC", rc                    // logged and tracked
    );
    
    // ... rest of implementation
}
```

**How it works:**
- First call with error codes: logs the error
- Subsequent calls with same error codes: silently skips logging (flood prevention)
- Call with different error codes: logs the new error
- Call with success codes: logs a success message with cleared codes and removes the logger

**Note:** When using `LG2_LEVEL_FLT` macros, the framework automatically extracts odd-indexed arguments (values in key-value pairs) for state change tracking. **Only the following types are supported for state tracking:**
- `bool`
- `nsm_reason_codes`
- `nsm_sw_codes`
- `nsm_completion_codes`

Other types (like `int`, `uint8_t`, `uint16_t`, `std::string`) can be used in log messages but will NOT be tracked for state changes.

#### Example 2: Using LG2_ERROR_FLT with Boolean Flag

```cpp
// In a method of NsmEventConfig class
bool NsmEventConfig::validateEventIds()
{
    bool isNotSupported = !isEventIdSupported(eventId);
    
    // Create a descriptive logger message for state tracking
    const auto loggerMsg = std::format("Validation of Event ID {} for Message Type {}", 
                                       eventId, messageType);
    
    // Use LG2_ERROR_FLT - only 'isNotSupported' (bool) is tracked for state changes
    // Note: eventId, messageType, eid are NOT tracked (not supported types)
    LG2_ERROR_FLT(
        "Event ID {ID} for Message Type {MSG_TYPE} is not supported, EID: {EID}",
        "ID", eventId,              // logged but NOT tracked (int type)
        "MSG_TYPE", messageType,     // logged but NOT tracked (enum/int type)
        "EID", eid,                  // logged but NOT tracked (eid_t type)
        "NOTSUPPORTED", isNotSupported  // logged AND tracked (bool type)
    );
}
```

**Important:** Only values of supported types (`bool`, `nsm_reason_codes`, `nsm_sw_codes`, `nsm_completion_codes`) are tracked for state changes. Other argument values are logged but ignored for flood prevention logic.

#### Example 3: Manual shouldLog() Usage (Advanced, Less Common)

Only use manual `shouldLog()` when you need explicit control over when to log:

```cpp
// In a method of a class inheriting from NsmObject
Task<> NsmPort::queryPortStatus()
{
    nsm_sw_codes rc = decode_query_port_status_resp(responseMsg.get(), responseLen, 
                                                     &cc, &reasonCode, &dataSize, 
                                                     &portState, &portStatus);

    // Manual check with shouldLog() - only use if you need conditional logging logic
    if (shouldLog("decode_query_port_status_resp", reasonCode, cc, rc))
    {
        LG2_ERROR(
            "decode_query_port_status_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    }
}
```

**Important:** `shouldLog()` is a method inherited from `StateChangeLogger` via `NsmObject`. It must be called within a class method, not as a global function.

#### Example 4: Using Human-Readable Translations (REQUIRED Pattern)

**CRITICAL:** When you need to log human-readable translations of error codes using utility functions like `utils::nsmSwCodeToString()`, you **MUST** use the manual `if(shouldLog()) { LG2_LEVEL() }` pattern. You **CANNOT** use `LG2_LEVEL_FLT` in this case.

**Why?** The `shouldLog()` function only accepts specific types (`bool`, `nsm_reason_codes`, `nsm_sw_codes`, `nsm_completion_codes`) and does **NOT** accept `std::string` or string conversions.

```cpp
// CORRECT: Manual shouldLog() with LG2_ERROR for human-readable output
Task<> NsmDevice::processResponse()
{
    nsm_sw_codes rc = performOperation();
    
    // Check state with raw enum values (supported types)
    if (shouldLog("processResponse", rc))
    {
        // Now we can use string conversion utilities in the log message
        LG2_ERROR(
            "processResponse failure | rc: {RC}, readable: {RC_STR}",
            "RC", rc,
            "RC_STR", utils::nsmSwCodeToString(rc)  // String translation
        );
    }
}
```

```cpp
// WRONG: This will NOT compile - LG2_ERROR_FLT doesn't accept string conversions
Task<> NsmDevice::processResponse()
{
    nsm_sw_codes rc = performOperation();
    
    // ❌ ERROR: This won't work - utils::nsmSwCodeToString() returns std::string
    LG2_ERROR_FLT(
        "processResponse failure | rc: {RC}, readable: {RC_STR}",
        "RC", rc,
        "RC_STR", utils::nsmSwCodeToString(rc)  // ❌ String not supported!
    );
}
```

**Summary:** Use `if(shouldLog()) { LG2_LEVEL() }` when you need string translations or other non-supported types in your log messages.

### Choosing the Right Logging Macro

The framework provides several macro levels:

1. **`LG2_<LEVEL>_FLT`** - **PREFERRED** - Logging with flood prevention
   - Automatically calls `shouldLog()` with odd-indexed arguments
   - Includes file name, line number
   - Includes object name and device ID if used in `NsmObject`
   - **Use this by default in polling loops and frequently executed code paths**
   - **Limitation:** Cannot use with string translations (e.g., `utils::nsmSwCodeToString()`)

2. **`if(shouldLog()) { LG2_<LEVEL>() }`** - Manual flood prevention with flexibility
   - Use when you need string translations like `utils::nsmSwCodeToString()`
   - Use when you need to log non-supported types (strings, converted values, etc.)
   - Provides same flood prevention as `LG2_<LEVEL>_FLT`
   - Includes file name, line number, object name, and device ID

3. **`LG2_<LEVEL>`** - Standard logging without flood prevention
   - Includes file name, line number
   - Includes object name and device ID if used in `NsmObject`
   - Use only for one-time operations or when you explicitly want every occurrence logged

Available levels: `EMERGENCY`, `ALERT`, `CRITICAL`, `ERROR`, `WARNING`, `NOTICE`, `INFO`, `DEBUG`

**Recommendation:** Always use `LG2_<LEVEL>_FLT` macros by default. Use the manual `if(shouldLog())` pattern only when you need string translations or other non-supported types in your log messages.

### Success State Logging

When all tracked arguments return to success state, the framework automatically logs a success message:

```
{FUNCNAME} SUCCESSFUL | Cleared Codes : ReasonCodes=[ERR_TIMEOUT, ERR_NOT_SUPPORTED], ResultCodes=[NSM_SW_ERROR]
```

This helps you track when issues are resolved without manual success logging.

### Best Practices

1. **Prefer `LG2_<LEVEL>_FLT` macros** - Always use these by default instead of manual `shouldLog()` calls
2. **Only use in `NsmObject` derived classes** - Never use `shouldLog()` as a global function; it's a method inherited from `StateChangeLogger` via `NsmObject`
3. **Use manual pattern for string translations** - When you need human-readable translations (e.g., `utils::nsmSwCodeToString()`), you MUST use `if(shouldLog()) { LG2_LEVEL() }` pattern instead of `LG2_LEVEL_FLT`
4. **Use descriptive message text** - The message text is used as the logger name for state tracking
5. **Pass all relevant state** - Include all error codes/flags that should trigger state change logging
6. **Consistent argument order** - Keep the same argument order in `LG2_LEVEL_FLT` calls for the same log message
7. **Use in polling loops** - Especially important for high-frequency operations (priority sensors, round-robin polling)
8. **Check merge request template** - The project requires use of flood-prevention logging (`shouldLog` API) for throttled logs

### Implementation Details

- Logger state is stored per logger name in a map
- State tracking uses a `Bitfield256` for enum types to track multiple error codes
- Logger entries are automatically removed when all states return to success
- Argument count and types must remain consistent for each logger name

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

**Important**: Always add new counters before `EnumCount`, as `EnumCount` must remain the last entry for the `PollingCountersSize` calculation.

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

#### 3. Update Counter Headers Map
Add your counter name to the `return` vector in `nsmd/nsmProgressCounters/progressCounters.cpp`:

```cpp
ProgressCounters::ProgressCounters(eid_t eid) :
    PollingCountersBase(
        SENSOR_PROGRESS_COUNTERS_DUMP_COUNT_THRESHOLD,
        SENSOR_PROGRESS_COUNTERS_DUMP_TIME_THRESHOLD,
        progressCountersObjectBasePath / "polling" / std::to_string(eid),
        "Polling Progress Counters for device EID=" + std::to_string(eid),
        {
            "Priority",
            "GPM",
            "LongRunning",
            "Static",
            "RoundRobin",
            "PriorityTimeExceeded",
            "PostPatch",
            "Event",
            "Error",
            "Timeout",
            "YourNewCounter",  // Add your counter name here
        })
{}
```

**Important**: The order must match the enum order in `ProgressCounterType`. This vector is exposed via D-Bus as the `CountersHeaders` property and is used by `nsmProgressCountersReader` to display counter names in CSV output.

#### 4. Increment the Counter
In the appropriate location in your code, increment the counter:

```cpp
// For successful operations
nsmDevice->progressCounters().increment(ProgressCounterType::YourNewCounter, rc);
```

### Data Structure

Counters are stored in a packed structure for efficient memory usage:

```cpp
template <std::size_t Size>
struct __attribute__((packed)) CountersDataRow
{
    uint32_t key;                    // Iteration/dump key
    uint64_t timestamp;              // Timestamp in microseconds
    CountersArray<Size> counters;    // Array of counter values
};
```

The data rotates in the memfd using `key % maxRows` to ensure bounded memory usage.

## Discovery Events

The NSM daemon tracks device discovery operations using discovery event counters. These counters are stored in a memory-mapped file descriptor (memfd) and can be accessed via D-Bus for monitoring and debugging the device discovery process.

### Discovery Event Types and Values

Discovery event counters track the state of device discovery operations. Unlike polling counters which increment continuously, discovery event counters track the state or result of specific discovery operations. Each counter is initialized to `-1` (not executed/not triggered) and is updated as the discovery process progresses.

#### Maintained DiscoveryEvents

The following discovery events are maintained for each NSM device:

#### 1. **InterfaceAddedSignal**
- **Description**: Tracks MCTP interface added signal events
- **When updated**: When MCTP interface is added
- **Values**: 
  - `-1`: Not triggered
  - `0+`: Count of interface added signals received

#### 2. **InterfaceRemovedSignal**
- **Description**: Tracks MCTP interface removed signal events
- **When updated**: When MCTP interface is removed
- **Values**: 
  - `-1`: Not triggered
  - `0+`: Count of interface removed signals received

#### 3. **ConnectivityAvailable**
- **Description**: Tracks connectivity status changes
- **When updated**: When device connectivity property changes
- **Values**: 
  - `-1`: Not set
  - `0`: Not available
  - `1`: Available

#### 4. **SetDeviceStateOnline**
- **Description**: Tracks online state transition task result
- **When updated**: When device online state task completes
- **Location**: Device online discovery task
- **Values**: 
  - `-1`: Not executed
  - `RC`: NSM return code (0 = success, non-zero = error code)

#### 5. **Ping**
- **Description**: Tracks ping command result during online discovery
- **When updated**: When ping command is executed during device online discovery
- **Location**: Online discovery process
- **Values**: 
  - `-1`: Not executed
  - `RC`: NSM return code (0 = success, non-zero = error code)

#### 6. **QueryDeviceIdentification**
- **Description**: Tracks device identification query result
- **When updated**: When Query Device Identification is executed during online discovery
- **Location**: Online discovery process
- **Values**: 
  - `-1`: Not executed
  - `RC`: NSM return code (0 = success, non-zero = error code)

#### 7. **OnlineMapNsmDeviceUsingEid**
- **Description**: Tracks success of device mapping during online discovery
- **When updated**: When device mapping is attempted using EID during online discovery
- **Location**: Online discovery process
- **Values**: 
  - `-1`: Not attempted
  - `0`: Failed
  - `1`: Success

#### 8. **GetSupportedNvidiaMessageType**
- **Description**: Tracks supported NVIDIA message types query result
- **When updated**: When supported message types are queried during online discovery
- **Location**: Online discovery process
- **Values**: 
  - `-1`: Not executed
  - `RC`: NSM return code (0 = success, non-zero = error code)

#### 9-15. **GetSupportedCommandCodes0-6**
- **Description**: Tracks supported command codes query results for message types 0-6
- **When updated**: When command codes are queried for each message type during online discovery
- **Location**: Online discovery process
- **Values**: 
  - `-1`: Not executed
  - `RC`: NSM return code (0 = success, non-zero = error code)

#### 16. **GetFru**
- **Description**: Tracks FRU (Field Replaceable Unit) information retrieval result
- **When updated**: When FRU information is retrieved during online discovery
- **Location**: Online discovery process
- **Values**: 
  - `-1`: Not executed
  - `RC`: NSM return code (0 = success, non-zero = error code)

#### 17. **SetDeviceStateOffline**
- **Description**: Tracks offline state transition task result
- **When updated**: When device offline state task completes
- **Location**: Device offline discovery task
- **Values**: 
  - `-1`: Not executed
  - `RC`: NSM return code (0 = success, non-zero = error code)

#### 18. **OfflineMapNsmDeviceUsingEid**
- **Description**: Tracks success of device mapping during offline discovery
- **When updated**: When device mapping is attempted using EID during offline discovery
- **Location**: Offline discovery process
- **Values**: 
  - `-1`: Not attempted
  - `0`: Failed
  - `1`: Success

### Configuration Options

Discovery event counters share the progress counter configuration:

- `progressCounter`: Enable/disable progress counter functionality (default: `enabled`)
- `discovery-progress-counters-memfd-size`: Size of the memory-mapped file in bytes (default: `8192`)

Discovery event counters are automatically dumped when any counter value changes after being set, ensuring each discovery operation snapshot is captured.

### Accessing Discovery Event Data

Discovery event data is exposed via D-Bus at:
```
/xyz/openbmc_project/progress_counters/discovery/<device_eid>
```

Use the `nsmProgressCountersReader` tool to read discovery event data:
```bash
# Read discovery events for all devices
nsmProgressCountersReader

# Read discovery events for specific device
nsmProgressCountersReader <device_eid>
```

### Understanding Discovery Event Values

Discovery event counters use signed 8-bit integers (`int8_t`) to represent three distinct states:

1. **Not Executed/Triggered** (`-1`): The operation has not been performed yet
2. **Failure** (`0` or error code): The operation failed or returned an error
3. **Success** (`1` or `0` for success): The operation completed successfully

This three-state model allows distinguishing between operations that haven't run yet versus operations that ran but failed.


## Primary Temperature Sensor

For `NSM_Temp` sensors, `SensorId = 0` is considered the primary temperature sensor.

As per NSM Spec (Get Temperature Reading command):
> SensorId 0 = Device temperature averaged across all device sensors

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
