# nsmd - Nvidia System Management Daemon

## How to build

Source an NvBMC ARM/x86 SDK.

```bash
meson build && ninja -C build
```

### Installing clang-format-17 for CI Usage

To ensure code consistency and formatting standards in the CI pipeline, `clang-format-17` needs to be installed. Follow the steps below to install `clang-format-17` on your system:

```bash
# Update the package list
sudo apt update

# Install wget if not already installed
sudo apt install wget

# Download the LLVM installation script
wget https://apt.llvm.org/llvm.sh

# Make the script executable
sudo chmod +x llvm.sh

# Run the script to install LLVM version 17
sudo ./llvm.sh 17

# Install clang-format-17
sudo apt install clang-format-17
```

This will install `clang-format-17` on your system, enabling it for use in the CI pipeline.

### Using clang-format-17 for all changed files before commit

To automatically format your code before each commit, create a pre-commit hook with the following steps:
```
cat > .git/hooks/pre-commit << EOL
#!/bin/sh

# Get list of staged files that are C/C++ source files
files=$(git diff --cached --name-only --diff-filter=ACMR | grep ".*\.[ch]\(pp\)\?$")

if [ -n "$files" ]; then
    # Format the files
    clang-format-17 -i $files
    
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
usage is to test nsmd and nsmtool features on an emulator like QEMU. The mockup
NSM responder includes modified MCTP control and demux daemon, user can create
a emulated MCTP endpoint by providing a json file to modified MCTP control
daemon to expose the emulated MCTP Endpoint to D-Bus.

The mockup NSM responder listens to demux unix socket for the request from
nsmd/nsmtool and returns the respond through modified MCTP demux daemon.
