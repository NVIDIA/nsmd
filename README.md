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
