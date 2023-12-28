## Overview of nsmtool

nsmtool is a client tool that acts as a NSM requester which runs on the BMC.
nsmtool sends the request message and displays the response message and also
provides flexibility to parse the response message & display it in readable
format.

nsmtool supports the subcommands for NSM types such as discovery [Type 0], 
telemetry [Type 1, 2, & 3], diag [Type 4], and config [Type 5].

- Source files are implemented in C++.
- Consumes nsm/libnsm encode and decode functions.
- Communicates with nsmd daemon running on BMC.
- Enables writing functional test cases for NSM stack.

Please refer the NSM specifications with respect to the nsm types.
https://nvidia.sharepoint.com/:w:/r/sites/MCTPSystemManagementAPI/Shared%20Documents/Specifications%20(working%20copy)/System%20Management%20API%20Base%20Specification.docx?d=w9b1d3c7a195848dd91d1742808256f77&csf=1&web=1&e=LQRwYQ


## Code organization

Source files in nsmtool repository are named with respect to the NSM type.

Example:
```
nsm_discovery_cmd.[hpp/cpp], nsm_telemetry_cmd.[hpp/cpp]
```

nsmtool commands for corresponding NSM type is constructed with the help of
encode request and decode response APIs which are implemented in nsm/libnsm.

Example:

Given a NSM command "foo" of NSM type "discovery" the nsmtool should consume
following API from the libnsm.

```
- encode_foo_req()  - Send the required input parameters in the request message.
- decode_foo_resp() - Decode the response message.
```

If NSM commands are not yet supported in the nsmtool repository user can
directly send the request message with the help of **nsmtool raw -d <data>** option.


## Usage

User can see the nsmtool supported NSM types in the usage output available
with the **-h** help option as shown below:

```
nsmtool -h
NSM requester tool for OpenBMC
Usage: nsmtool [OPTIONS] SUBCOMMAND

Options:
  -h,--help                   Print this help message and exit

Subcommands:
  raw                         Send a raw request and print response
  discovery                   Device capability discovery type command
  telemetry                   Network, PCI link and platform telemetry type command
  diag                        Diagnostics type command
  config                      Device configuration type command

```
nsmtool command prompt expects a NSM type to display the list of supported
commands that are already implemented for that particular NSM type.

```
Command format: nsmtool <nsmType> -h
```
Example:

```
$ nsmtool telemetry -h
telemetry type command: Network, PCI link and platform telemetry type command
Usage: nsmtool telemetry [OPTIONS] SUBCOMMAND

Options:
  -h,--help                   Print this help message and exit

Subcommands:
   GetTemperature              get temperature from a given source in degrees Celsius
   GetAggregateTemp            get aggregate temperature from multiple sensors
   GetPower                    get power reading from a given source in milliwatts
   GetPowerLimits              get power limits from the devices
   SetPowerLimits              set power limits for the devices
   GetEDPpScalingFactor        get EDPp scaling factor in integer percentage
   SetEDPpScalingFactor        set EDPp scaling factor as an integer percentage
   GetInventoryInfo            get inventory information
```
More help on the command usage can be found by specifying the NSM type and the
command name with **-h** argument as shown below.

```
Command format: nsmtool <nsmType> <commandName> -h
```

Example:
```
$ nsmtool telemetry GetTemperature -h
GetTemperature command: Get temperature from a given source in degrees Celsius
Usage: nsmtool platform getTemperature [OPTIONS]

Options:
  -h,--help                     Print this help message and exit
  -m,--mctp_eid   UINT          MCTP endpoint ID
  -v,--verbose
  -s,--sensorId   UINT REQUIRED 
```


## nsmtool raw command usage

nsmtool raw command option accepts request message in the hexadecimal
bytes and send the response message in hexadecimal bytes.

```
$ nsmtool raw -h
raw type command: Send a raw request and print response
Usage: nsmtool raw [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  -m,--mctp_eid UINT          MCTP endpoint ID
  -v,--verbose
  -d,--data     UINT REQUIRED Raw data
```

**nsmtool request message format:**

```
nsmtool raw --data 0x30 <nsmType> <cmdType> <payloadReq>

payloadReq - stream of bytes constructed based on the request message format
             defined for the command type as per the spec.
```

**nsmtool response message format:**

```
<instanceId> <hdrVersion> <nsmType> <cmdType> <completionCode> <payloadResp>

payloadResp - stream of bytes displayed based on the response message format
              defined for the command type as per the spec.
```
Example:

```
root@e4869:~# ./nsmtool raw -v -m 30 -d 0x7e 0xde 0x10 0x80 0x01 0x00 0x00 0x02 0x00 0x00
nsmtool: <6> Tx: 1e 7e 7e de 10 80 01 00 00 02 00 00
Success in creating the socket : RC = 5
Success in connecting to socket : RC = 0
Success in sending message type as VDM to mctp demux daemon : RC = 1
nsmtool: <6> Rx: 7e de 10 00 00 00 00 00 00 00
```

## nsmtool with mctp_eid option

Use **-m** or **--mctp_eid** option to send nsm request message to remote mctp
end point and by default nsmtool consider mctp_eid value as **'30'**.

```
Command format:

nsmtool <nsmType> <cmdType> -m <mctpId>
nsmtool raw -d 0x80 <nsmType> <cmdType> <payloadReq> -m <mctpId>
```

## nsmtool verbosity

By default verbose flag is disabled on the nsmtool.

Enable verbosity with **-v** flag as shown below.

Example:

```
 nsmtool telemetry GetTemperatureReading -m {EID} -s {SENSOR_ID}
```