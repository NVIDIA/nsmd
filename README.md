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
meson setup --reconfigure -Db_sanitize=address,undefined -Db_lundef=true -Dwerror=true -Dwarning_level=3 -Db_colorout=never --buildtype=debug -Dcpp_args="-Wno-error=invalid-constexpr -Wno-invalid-constexpr -Werror=uninitialized -Werror=strict-aliasing" builddir
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
