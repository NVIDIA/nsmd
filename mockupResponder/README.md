## Overview of nsmMockup responder

A mockup NSM responder is developed for developing nsmd on QEMU. The mockup NSM
responder is based on the work of PLDM mockup responder project. The  mockup
NSM responder includes modified MCTP control and demux daemon, user can create
a emulated MCTP endpoint by providing a json file to modified MCTP control
daemon to expose the emulated MCTP Endpoint to D-Bus.
The mockup NSM responder is a program listening to demux unix socket for
the request from nsmd/nsmtool and returning the respond to nsmd through
modified MCTP demux daemon.

Please refer the NSM design doc for more details.
https://gitlab-master.nvidia.com/dgx/bmc/docs/-/blob/develop/designs/oem/Nvidia/nsmd.md?ref_type=heads#nsm-endpoint-mockup-responder


## Arguments details

Mockup will need below CLI arguments:

EID {-e}        Eid to be assigned to the mockup device
DeviceType {-d} Which device mockup will mock 
                [possible values: GPU, Switch, PCIeBridge and Baseboard] 
                (this field is not case-sensitive)
InstanceId {-i} instanceID of the mock device.
                (default value is 0)

Please refer the help for more details.

## How to run

User can see the required arguments for nsmMockup with the **-h** help option as shown below:

```
root@umbriel:~# nsmMockupResponder -h
Usage: nsmMockupResponder [options]
Options:
 [--verbose] - would enable verbosity
 [--eid <EID>] - assign EID to mockup responder
 [--instanceId <InstanceID>] - assign instanceId to mockup responder [default - 0]
 [--deviceType <DeviceType>] - assign DeviceType to mockup responder [GPU, Switch, PCIeBridge and Baseboard]
root@umbriel:~# 

```
For running the nsmMockup user can runn command as shown below.

```
root@umbriel:~# nsmMockupResponder -e 30 -d "gpu" -i 1 -v
<6> start a Mockup Responder EID=30 DeviceType=gpu (0) InstanceID=1
<6> connect to Mockup EID
<6> Rx: 00 1e 7e 10 de 87 89 01 01 01 0b 
<3> received NSM request length=11
...........
```

## MockupResponder events examples

### genThreasholdEvent

#### Parameters

- **dest**: The destination EID.
- **ackr**: Acknowledgment request flag.
- **port_rcv_errors_threshold**: Port receive errors threshold flag.
- **port_xmit_discard_threshold**: Port transmit discard threshold flag.
- **symbol_ber_threshold**: Symbol bit error rate threshold flag.
- **port_rcv_remote_physical_errors_threshold**: Port receive remote physical errors threshold flag.
- **port_rcv_switch_relay_errors_threshold**: Port receive switch relay errors threshold flag.
- **effective_ber_threshold**: Effective bit error rate threshold flag.
- **estimated_effective_ber_threshold**: Estimated effective bit error rate threshold flag.
- **portNumber**: The port number associated with the event.

#### Usage

```bash
busctl call xyz.openbmc_project.NSM.eid_22 /xyz/openbmc_project/NSM/22 xyz.openbmc_project.NSM.Device genThreasholdEvent ybbbbbbbby 22 1 0 0 1 0 0 0 0 0
busctl call xyz.openbmc_project.NSM.eid_28 /xyz/openbmc_project/NSM/28 xyz.openbmc_project.NSM.Device genThreasholdEvent ybbbbbbbby 28 1 0 0 1 0 0 0 0 0
```
