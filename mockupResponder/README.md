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
 [--deviceType <DeviceType>] - assign DeviceType to mockup responder [GPU, Switch, PCIeBridge, Baseboard, ERot and MCTPBridge]
 [--failure_cycle] - replay the built-in dump-command failure cycle [default - off]
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

## Dump failure cycle (`--failure_cycle`)

The mockup ships with a built-in failure cycle for the five NSM Type-4 dump
commands so developers can walk every device-side failure -> reported status
mapping without modifying real firmware. Responses are built with the same libnsm
`encode_*_resp` helpers a real device uses, so the bytes on the wire are
bit-accurate.

It is off by default. Start the mockup with `--failure_cycle` to enable
it:

```bash
nsmMockupResponder -e 11 -d gpu -i 0 -v --failure_cycle
```

### How it works

- A single **global counter** is shared across all five dump commands
  (`0x40` GetDeviceDiagnostics, `0x50` GetNetworkDeviceDebugInfo, `0x52`
  GetNetworkDeviceLogInfo, `0x51` EraseTrace, `0x59` EraseDebugInfo).
- Each dump the consumer performs consumes **one list entry** (`DumpCycleCase`
  in `mockupResponder.hpp`). Most entries are a **single page**: the device
  answers the fresh request and the dump terminates. Issuing the same NSM raw
  dump command repeatedly therefore walks the whole list, one failure per
  dump.
- After the last entry the counter **wraps** back to the start, so the cycle
  can be re-walked indefinitely without restarting the mockup.
- When the flag is **off**, the handlers return their original happy-path
  responses (unchanged) and the counter is never touched.

Each response is logged at `info` level with a `[MOCK-CYCLE]` tag carrying the
case index, case id, page, completion code, reason code, and the
`AsyncOperationStatus` the design expects nsmd to publish, e.g.:

```text
[MOCK-CYCLE] cmd=GetNetworkDeviceDebugInfo case[3]=CC_NSM_BUSY page=0/1 silent=0 cc=0x7e reason=0x0000 expect=Unavailable
```

### The cycle (in walk order)

| # | Case id | Device response | Expected `AsyncOperationStatus` |
|---|---|---|---|
| 0 | `SUCCESS` | `cc=NSM_SUCCESS`, single page, END handle | `Success` |
| 1 | `CC_UNSUPPORTED_COMMAND_CODE` | `cc=0x05` | `UnsupportedRequest` |
| 2 | `CC_UNSUPPORTED_MSG_TYPE` | `cc=0x06` | `UnsupportedRequest` |
| 3 | `CC_NSM_BUSY` | `cc=0x7e` | `Unavailable` |
| 4 | `CC_NSM_ERR_NOT_READY` | `cc=0x04` | `Unavailable` |
| 5 | `CC_NSM_ERR_BUS_ACCESS` | `cc=0x7f` | `Unavailable` |
| 6 | `CC_INVALID_STATE_FOR_COMMAND` | `cc=0x80` | `Unavailable` |
| 7 | `CC_INVALID_DATA` | `cc=0x02` | `InvalidArgument` |
| 8 | `CC_INVALID_DATA_LENGTH` | `cc=0x03` | `InvalidArgument` |
| 9 | `CC_INVALID_REQUEST_TYPE` | `cc=0x81` | `InvalidArgument` |
| 10 | `CC_NSM_ACCEPTED` | `cc=0x7d` | `InProgress` |
| 11 | `REASON_ERR_TIMEOUT` | `cc=0x04`, `reason=0x03` | `Timeout` |
| 12 | `REASON_ERR_DOWNSTREAM_TIMEOUT` | `cc=0x04`, `reason=0x04` | `Timeout` |
| 13 | `REASON_ERR_NOT_SUPPORTED` | `cc=0x04`, `reason=0x0a` | `UnsupportedRequest` |
| 14 | `REASON_ERR_NO_BOOT_COMPLETE` | `cc=0x04`, `reason=0x103` | `Unavailable` |
| 15 | `REASON_ERR_UPDATE_IN_PROGRESS` | `cc=0x04`, `reason=0x104` | `Unavailable` |
| 16 | `REASON_ERR_IMAGE_COPY_IN_PROGRESS` | `cc=0x04`, `reason=0x105` | `Unavailable` |
| 17 | `REASON_ERR_FLASH_WEAR_MITIGATION` | `cc=0x04`, `reason=0x107` | `Unavailable` |
| 18 | `REASON_ERR_INVALID_PCI` | `cc=0x04`, `reason=0x01` | `InvalidArgument` |
| 19 | `REASON_ERR_INVALID_RQD` | `cc=0x04`, `reason=0x02` | `InvalidArgument` |
| 20 | `REASON_ERR_INCOMPLETE_COMPONENT_SET` | `cc=0x04`, `reason=0x108` | `InvalidArgument` |
| 21 | `REASON_ERR_I2C_NACK_FROM_DEV_ADDR` | `cc=0x04`, `reason=0x05` | `Unavailable` |
| 22 | `NO_RESPONSE_TIMEOUT` | no MCTP reply | `Timeout` |
| 23 | `CATCHALL_UNMAPPED` | `cc=0x01` (unmapped) | `InternalFailure` |
| 24 | `PROTO_STUCK_HANDLE` | 2 pages: advance, then repeat the request handle | `InternalFailure` |

Reason-code entries use `cc=NSM_ERR_NOT_READY` as a carrier; nsmd's mapper
gives a recognized reason code precedence over `cc` (see
`nsmd/nsmDumpCollection/nsmDumpUtils.cpp`), so the carrier never masks the
reason under test.

`PROTO_STUCK_HANDLE` is the only multi-page entry and only applies to the
iterative commands (`0x40`/`0x50`/`0x52`): page 0 advances the handle so the
BMC recurses, page 1 repeats the request handle so nsmd's stuck-loop guard
trips. The single-shot erase commands (`0x51`/`0x59`) skip this entry (and any
future multi-page entry) so a case is never left half-consumed between two
commands.

### Walking the cycle

Issue the same raw dump command N times (N = list length) and watch the
`[MOCK-CYCLE]` log and the resulting `com.nvidia.Async.Status.Status` value:

```bash
# Start the mock with the cycle enabled.
nsmMockupResponder -e 11 -d gpu -i 0 -v --failure_cycle &

# Each invocation consumes one list entry; repeat to walk the whole cycle.
for i in $(seq 1 25); do
  nsmtool raw -m 11 -d 0x10 0xde 0x80 0x89 0x04 0x50 0x06 0x00 0x00 0x00 0x00 0x00 0x00
done
```

The same applies driving it end-to-end from Redfish: repeat the
`LogService.CollectDiagnosticData` POST and cross-check each task's reported
status against the expected status for that cycle entry.

## Failure cases not exercised by the cycle

A few failure modes cannot be produced from the device side
with a single (or few) page response and are therefore not part of the cycle.
Test them as follows on a simulated platform (QEMU HMC) using `busctl`
directly against `xyz.openbmc_project.NSM`:

- **`WriteFailure`** (BMC failed to write the dump to its fd). Invoke the dump
  D-Bus method with a fd that is read-only or already closed so nsmd's
  `appendBufferToFd` fails:

  ```bash
  # fd 1 (stdout of busctl) is not writable as a dump sink in this context;
  # any read-only / closed fd reproduces the WriteFailure mapping.
  busctl call xyz.openbmc_project.NSM \
    /xyz/openbmc_project/inventory/system/chassis/CX_0/NetworkAdapters/CX_NIC_0 \
    com.nvidia.Dump.DebugInfo GetDebugInfo yh 0 1
  ```

- **`ConflictingOperation`** (a dump/log/erase is already running on the same
  object). Launch two `GetDebugInfo` calls against the same inventory path
  concurrently; the second returns `ConflictingOperation`.

- **`ResourceNotFound`** (target device absent from the inventory mapper).
  Issue the dump against a non-existent `DeviceType=` / inventory path.

- **`NO_END_SENTINEL_RUNAWAY`** (firmware advances `next_record_handle`
  forever without ever returning the END sentinel — the live GPU NetIR bug).
  This needs thousands of iterations and the bmcweb 45-minute task timer, so
  it is impractical to drive through the bounded cycle. Reproduce it by
  temporarily editing `kDumpFailureCycle` to a single `DumpHandleMode::Advance`
  page that never terminates, or run against the affected GPU silicon
  directly.

These BMC-side / transport-class statuses (`WriteFailure`,
`ConflictingOperation`, `ResourceNotFound`, and the generic `NSM_SW_ERROR*`
transport codes other than the timeout exercised by `NO_RESPONSE_TIMEOUT`) are
produced by nsmd / the dump manager, not by device response bytes, which is why
the device-side mockup cycle does not cover them.

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
