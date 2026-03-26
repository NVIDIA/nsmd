# Blocked Unit Tests

This document tracks tests that cannot be generated due to missing mocks or unmockable
dependencies. Only items that are **currently** blocked are listed here. Previously blocked
items that have since been resolved are summarized in the History section at the bottom.

---

## Current Status (2026-03-25)

**411 test binaries, ~55 DISABLED individual tests (infrastructure limitations).**
**90.0% line coverage · 97.4% function coverage · 76.2% branch coverage**
(gcovr: 54,531/60,501 lines · 39,098/51,303 branches · 4,747/4,876 functions)

### Session additions (2026-03-25)

**12 new branch coverage test files** across 8 modules:
nsmPcieGroupBranch, nsmLeakDetectionBranch6, nsmGpuClockControlBranch3,
nsmFpgaPortBranch4, nsmSecurityRBPBranch3, nsmEventConfigDeepBranch,
nsmApSkuIdBranch, nsmMemoryBranch5, nsmRoTPropertyBranch4,
nsmDebugTokenNICBranch3, nsmProcessorFactoryBranch9, nsmGpmOemBranch7.

**8 test files renamed** for consistent naming convention
(commonBranch→common_branch, branches→branch, _branch→Branch).

Coverage delta: +3.8% branches (+1,502), +2.9% lines.

### Session additions (2026-03-18)

**Production code changes for testability** (2 commits before tests):

1. **`ed8cf48c` fix: remove static from factory functions** — removed `static`
   from 40 factory coroutines/helpers in 14 files, enabling tests to
   forward-declare and call them directly.

2. **`51e7ef00` fix: dependency injection for testability** — made
   MctpDiscovery public methods `virtual` (enabling mock subclasses),
   made `NsmDevice::discoveryEvents()` virtual with try-catch fallback,
   guarded `dumpNsmDeviceInfoTask()` MctpDiscovery call.

**350+ new branch coverage tests** across 14 test files:
nsmProcessorBranch2 (92), nsmPowerLimitBranch (40), nsmMemoryBranch (22),
nsmGpmOemBranch2 (25), nsmSwitchBranch (22), nsmPortBranch (27),
nsmDebugTokenNICBranch (20), nsmProcessorModulePowerControlBranch (30),
nsmEventConfigBranch (21), nsmCommonBranch (10),
nsmGpuPresenceAndPowerStatusBranch (15), nsmLeakDetectionBranch (15),
nsmSetECCModeBranch (5), nsmSetEgmModeBranch (5).

### Session additions (2026-03-11)

**Branch 12 second-operand coverage** — systematic coverage of the
`rc == NSM_SW_SUCCESS && cc != NSM_SUCCESS` (second operand of `||`) pattern
across all `nsmd` source files. Two commits:

**Commit 5c6d932d** — nsmClockOutputEnableState L65, nsmIRoTResponder L77,
nsmSecurityRBP L118, nsmDebugTokenUnified L702, GPUSWInventory L121.

**Commit ef5b7cda** — nsmLeakDetection L596, nsmGpuClockControl L301
(clearReqClockLimit + setRangeClockLimits), nsmProcessorModulePowerControl L187,
nsmLogInfo L92, nsmDebugInfo L110 + L240, nsmRetimerPort L1201.

Coverage delta: +1.0% branches, +2.3% decisions. 15 new tests added.

### Recently Fixed DISABLED Tests (2026-03-10)

Three previously-disabled tests have been fixed and re-enabled:

| Test | File | Fix |
|------|------|-----|
| `CoroutineSemaphoreMock.Contended_DeferFails_WaiterNotResumed` | `common/test/sleep_semaphore_mock_test.cpp` | Fixed memory leak in `coroutineSemaphore.hpp` — `delete awaiterPtr` added on `sd_event_add_defer` failure |
| `NsmEraseTraceObjectTest.EraseDebugInfo_UnsupportedType_SetsInternalFailure` | `nsmd/nsmEvent/test/nsmEvent_dump_test.cpp` | Changed `static_cast<EraseInfoType>(99)` → `EraseInfoType::Other` to avoid sdbusplus enum-to-string throw |
| `NsmSwitchIsolationModeHandleResp.HandleResponseMsg_ShortLen_ReturnsError` | `nsmd/nsmChassis/test/nsmSwitchBranch_test.cpp` | Fixed buffer size from `sizeof(nsm_msg_hdr)` → `sizeof(nsm_msg_hdr)+sizeof(nsm_common_non_success_resp)`; cc=0 so decode_reason_code_and_cc succeeds → NSM_SW_ERROR_LENGTH correctly returned |

### Test Infrastructure Available

The following mock/test infrastructure is fully functional and available for new tests:

| Component | Location | Purpose |
|-----------|----------|---------|
| `MockNsmDevice` | `nsmd/test/commonMock.hpp` | Mocks all NsmDevice pure virtuals (sensorIO, postPatchIO, updateNsmDevice, doSleep, refreshCapabilitySensor) |
| `MockSensorManager` | `nsmd/test/mockSensorManager.hpp` | Provides mockManager singleton, `getEid()`, `getObjServer()` |
| `NullReturnMockSensorManager` | `nsmd/test/mockSensorManager.hpp` | Returns `nullptr` from `getNsmDeviceFromStaticUUID()` to test factory error branches |
| `SensorManagerTest` fixture | `nsmd/test/mockSensorManager.hpp` | Base class: `mockSensorIO()`, `mockPostPatchIO()`, `cleanupDeviceSensors()` |
| `MockDbusAsync` | `common/test/mockDBusHandler.hpp` | `propertyMap(objPath, intf)` registers D-Bus property maps; `findPropertyMap()` returns nullptr if absent |
| `DBusTest` fixture | `common/test/mockDBusHandler.hpp` | Clears D-Bus mock state in destructor |
| `EXPECT_THROW_COROUTINE` | `nsmd/test/commonMock.hpp` | Works in both `COVERAGE_DISABLE_COROUTINES` mode (direct throw) and normal mode |
| `MctpTestFixture` | `common/test/mockSocketIo.hpp` | MCTP socket pair infrastructure for socket_handler tests |
| `COVERAGE_DISABLE_COROUTINES` | build configuration | Makes coroutines execute synchronously; `co_await` → direct call, `co_return` → `return` |

---

## Files Genuinely Blocked (Current)

### 1. `requester/mctp_endpoint_discovery.cpp` — 0% coverage, 606 lines

**Why blocked**: The `MctpDiscovery` class requires a fully-initialized singleton
(`MctpDiscovery::getInstance()`). The singleton initialization requires:
- A real `sdeventplus::Event` loop
- A real `sdbusplus::bus::bus` session
- A real MCTP socket file descriptor
- D-Bus service registration for `xyz.openbmc_project.MCTP.Control`

In unit tests `MctpDiscovery::getInstance()` throws `std::runtime_error` because the
singleton is never initialized. There is no way to inject a mock instance.

**Uncovered functionality**: Endpoint discovery, MCTP UUID resolution, device enumeration,
endpoint state change notifications, service ready detection, all 1,200+ lines of discovery
logic across `mctp_endpoint_discovery.cpp` and related headers.

**Recommended action**: Integration tests with a real event loop. Note:
`MctpDiscovery` methods are now `virtual` (commit `51e7ef00`), enabling mock
subclasses. A `MockMctpDiscovery` test fixture can now override
`getNsmDeviceFromEid()`, `discoveryEvents()`, etc. However, the singleton
`initialize()` pattern still prevents full injection — a `setTestInstance()`
or constructor-based injection would complete the testability story.

---

### 2. `nsmd/nsmDevice.cpp` — 36% coverage, 411 uncovered lines (of 643)

**Why blocked**: `MockNsmDevice` overrides all pure virtuals — meaning the real
implementations in `nsmDevice.cpp` are never called. The actual coroutine bodies
(which contain the real logic) require:
- A real `requester::Handler<requester::Request>` for `SendRecvNsmMsg`
- A real `sdeventplus::Event` and `InstanceIdDb` for async MCTP send/receive
- A real `mctp_socket::Manager` for socket communication

The real `SendRecvNsmMsg` is the root call for all sensor I/O, and without a real
requester handler the entire chain from `sensorIO` → `SendRecvNsmMsg` → MCTP is
untestable.

**Uncovered lines (183–1228)**:

| Lines | Function | Reason |
|-------|----------|--------|
| 183–265 | `SendRecvNsmMsg()`, `sensorIO()`, `postPatchIO()` | Real implementations never called — MockNsmDevice overrides these |
| 359–943 | `updateNsmDevice()`, `getSupportedNvidiaMessageType()`, `getSupportedCommandCodes()`, `waitForNsmDeviceUpdate()`, `setOnline()`, `setOffline()`, `updateSensorsForOffline()`, `markSensorsUnrefreshed()` | All are coroutines calling real sensorIO |
| 966–1084 | `getFRU()`, `getInventoryInformation()`, `updateFruDeviceIntf()` | Coroutines calling real sensorIO |
| 1114–1228 | `refreshCapabilitySensor()`, `refreshCommandMatrix()`, `dumpNsmDeviceInfoTask()` | Coroutines calling real sensorIO |

**Recommended action**: Same as mctp_endpoint_discovery.cpp — requires real requester
infrastructure or a full coroutine executor mock. Note: `discoveryEvents()` is now
`virtual` with a try-catch fallback (commit `51e7ef00`), and `dumpNsmDeviceInfoTask()`
guards the MctpDiscovery call. These changes prevent test crashes but don't make the
real coroutine bodies testable without real MCTP infrastructure.

---

### 3. `nsmd/sensorManager.cpp` — 29% coverage, 290 uncovered lines (of 411)

**Why blocked**: Covered lines (121/411) are the static helper methods that were
extracted and tested in isolation (`isNSMPollReady`, `dumpReadinessLogs`, `markEMReady`,
`startPolling`). The remaining 71% consists of event-driven handlers that fire only via
D-Bus signals and `sdeventplus` event loop callbacks.

**Covered (29%)**:
- `isNSMPollReady()` (L50-65) — static, tested directly
- `dumpReadinessLogs()` (L386-415) — static, tested directly
- `markEMReady()`, `isEMReady()` (partial) — tested via SensorManagerTest
- `startPolling()`, `pollPrioritySensors()`, `pollNonPrioritySensors()` — tested via
  deviceTask loop mock in `sensorManager_test.cpp`

**Uncovered (71%) — all require real D-Bus/event-loop infrastructure**:

| Lines | Function | Blocker |
|-------|----------|---------|
| 72–358 | Constructor body — D-Bus signal matches, `sd_event` setup, timer callbacks, `sdbusplus::asio::object_server` | `sdbusplus::asio::object_server` not mockable |
| 461–545 | `scanInventory()` | Requires live EntityManager D-Bus tree |
| 552–635 | `interfaceAddedHandler()`, `interfaceAddedTask()` | D-Bus signal callbacks, never fire in tests |
| 639–711 | `gpioStatusPropertyChangedHandler()`, `mctpReadinessSigHandler()` | Signal handlers, never fire |
| 771–785 | `checkAllDevicesReady()`, `dumpNsmDevicesInfo()` | Called from event handlers |

**Recommended action**: Integration tests. The static helpers are fully covered;
the async event handlers cannot be unit-tested without a real D-Bus session.

---

### 4. `nsmd/nsmEvent/nsmLongRunningEventHandler.cpp` — 50% coverage, 6 uncovered lines (of 12)

**Why blocked**: `handle()` function (lines 35–44) immediately calls
`MctpDiscovery::getInstance()` which throws `std::runtime_error` in tests.
The test `Delegate_MctpDiscoveryNotInit_ThrowsRuntimeError` in `nsmEvent_test.cpp`
covers the exception path (lines 37–39 via catch). Lines 35, 37, 39–40, 42, 44
(the success path after getInstance() returns) are permanently unreachable.

**Recommended action**: Refactor `handle()` to accept `MctpDiscovery&` by injection
instead of using the singleton directly.

---

### 5. `requester/mctp_endpoint_prober.cpp` — 47% coverage, 96 uncovered lines (of 183)

**Why blocked**: `McEndpointProber` requires a real `sendRecvFn` callback
(actual MCTP socket I/O function pointer). The uncovered half consists of the
async send/receive paths triggered by the real sendRecvFn:
- `probe()` body (lines 44–95) — async MCTP probe logic
- `handleResponse()` (lines 131–185) — response parsing per endpoint
- Retry/timeout paths (lines 226–301) — require real timer callbacks

The test file `requester_test.cpp` covers the synchronous paths that don't
require real socket I/O.

**Recommended action**: Inject `sendRecvFn` as a mock lambda in tests.

---

### 6. `requester/handler.hpp` — 50% coverage, 56 uncovered lines (of 112)

**Why blocked**: Template retry/timeout logic (lines 222–339) is tested only
via the mock path in `requester_test.cpp`. The real paths require:
- `sdeventplus::Event` for async timer callbacks
- Real `sd_event_source` for timeout handling
- `InstanceIdDb` for MCTP instance ID allocation under load

**Uncovered paths**: Retry-after-timeout (lines 222–260), concurrent request
limiting (lines 312–339), request queue draining (lines 449–513).

---

### 7. `requester/request.hpp` — 8% coverage, 31 uncovered lines (of 34)

**Why blocked**: Template class `Request<ExecutionContext, Handler>` — all paths
except construction require a live `sdeventplus::Event` loop. The timer-based
retry/expiry logic (lines 65–234) fires via `sd_event` callbacks that never
trigger in unit tests.

---

### 8. `common/dBusHandler.cpp` — 27% coverage, 202 uncovered lines (of 279)

**Why blocked**: Real `sdbusplus::bus::bus` D-Bus methods. The 77 covered lines
are the `MockDBusHandler` dispatch paths. The remaining 73% (lines 26–197 approx)
are real D-Bus `call()` and `new_method_call()` implementations that require a
live D-Bus session bus. `MockDBusHandler` intercepts all calls in tests and the
real implementations are never reached.

---

### 9. `nsmd/nsmPort/nsmRetimerPort.cpp` — **83%** coverage (up from 66%)

**Partially resolved** — static factory functions unblocked.

**9a. Previously blocked static factory functions — NOW RESOLVED**

`createNsmPCIeRetimerPorts()` and `createNsmMultiPCIeRetimerPorts()` had `static`
linkage. `static` was removed, forward declarations added to test files, and 16 new
tests were written covering all `count()` FALSE branches, portType switch paths,
CX9 `includeInboundCounters`, and null-device error paths.

**9b. Remaining uncovered lines (gcov B50/B51 false negatives, lines 23–90)**

Constructor/destructor bodies and string literal continuation lines in `lg2::` calls
(nsmRetimerPort.hpp constructors, lines 23–90) — appear uncovered in the HTML report
but ARE executed. This is a tooling artifact, not a real coverage gap.

---

### 10. `nsmd/nsmMsghandler.hpp` — 51% coverage, 13 uncovered lines (of 27)

**Why blocked**: The real `SendRecvNsmMsg` implementation in `NsmMsgHandler` requires
a live MCTP socket session. Lines 107, 120, 124–138 are the actual send/receive
coroutine body. In tests, `MockNsmMessageHandler` is used instead.

---

## Individual Branch Blockers

### Remaining `||` second-operand branches (rc==NSM_SW_SUCCESS, cc!=NSM_SUCCESS)

All `if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)` conditions have been
systematically analysed (2026-03-11). All second operands are now covered
**except** the items below, which are permanently blocked:

| File | Line | Reason |
|------|------|--------|
| `nsmd/nsmDevice.cpp` | L623, L688, L870 | `MockNsmDevice.nsmMsgHandler` is null → `getSupportedNvidiaMessageType`, `getSupportedCommandCodes`, `getInventoryProperty` never called (see §2) |
| `nsmd/nsmChassis/nsmPCIeFunction.cpp` | L116 | Test binary `nsmPCIeFunction_test` disabled in meson.build (`NsmInterfaceProvider` deleted constructor) |
| `nsmd/nsmDebugToken/nsmDebugTokenNIC.cpp` | L614 | Dead code: second `decode_nsm_query_device_ids_resp` call (full buffer) after first (size query) already succeeded — same response can't fail the second time |
| `nsmd/nsmDebugToken/nsmDebugTokenUnified.cpp` | L715 | Dead code: defensive check after `cc == NSM_SUCCESS` already verified at L702 |
| `nsmd/nsmChassis/nsmIRoTResponder.cpp` | L89 | Dead code: second decode after `cc == NSM_SUCCESS` already verified at L77 |

---

### `nsmd/nsmDumpCollection/nsmLogInfo.cpp` — `default:` in synced_time switch

**Lines**: 121–123 (`default:` branch in `getLogInfoAsyncHandler()`)

**Pattern**:
```cpp
switch (logInfo.synced_time) {
    case SYNCED_TIME_TYPE_BOOT:   // = 0
    case SYNCED_TIME_TYPE_SYNCED: // = 1
    default: → InternalFailure   // UNREACHABLE
}
```

**Why blocked**: `logInfo.synced_time` is a **1-bit bitfield** (`uint8_t synced_time : 1`).
A 1-bit field holds only values 0 or 1. Any attempt to assign `logInfo.synced_time = 2`
is silently truncated to 0 by C++ standard. The `default:` branch is structurally
impossible to reach without changing the struct definition.

---

### `nsmd/nsmNumericSensor/nsmThresholdFactory.cpp` — error branch in `make()`

**Lines**: 58–63 (`if (result != NSM_SW_SUCCESS)` after `getThresholdInterfacesAsync()`)

**Why blocked**: `getThresholdInterfacesAsync()` always `co_return NSM_SW_SUCCESS`.
There is no code path that returns a non-success value. `coGetDbusProperty<string>`
failure in the mock infrastructure suspends permanently rather than returning an error
code. So `result` is always `NSM_SW_SUCCESS` and the error branch is dead code.

---

### `nsmd/nsmEvent.cpp` — blocked decision branches

**Line 93** — `co_return NSM_SW_SUCCESS` (FALSE branch of `if (!logSuccess)` at L85):
In `COVERAGE_DISABLE_COROUTINES` mode, `-Dco_await=` replaces `co_await` with nothing.
`coLogEvent` constructor initializes `success = false`. `await_ready()` (which would
set `success = true`) is never called. So `operator bool()` always returns `false`,
meaning `!logSuccess` is always `true` and only the error path is taken.

**Line 73** — FALSE branch of `if (!cachedLoggingService)` inside mutex:
Requires two concurrent calls where the second call caches the service between the
outer guard and the inner check. Impossible in single-threaded tests.

**Lines 158–170 in `DelegatingEventHandler::delegate()`** — both branches of
`if (!nsmDevice)`: `MctpDiscovery::getInstance()` throws at line 156 before line 158
is reached. Neither branch is ever taken.

---

## Lowest-Coverage Files — Root Cause Summary

Deep analysis (2026-03-10, updated 2026-03-11) confirmed that **all remaining
uncovered lines** fall into one of three categories. No new test opportunities
were found beyond the two items listed under Category G below.

| % | File | Uncovered lines | Root cause |
|---|------|-----------------|------------|
| 27% | `common/dBusHandler.cpp` | 202/279 | **BLOCKED** — real D-Bus `call()` required (see §8) |
| 36% | `nsmd/nsmDevice.cpp` | 411/643 | **BLOCKED** — MCTP transport required (see §2) |
| 53% | `nsmd/nsmCommon/nsmPciePortIntf.cpp` | 6/13 | **EH_BRANCH** — constructor initializer list (gcov artifact) |
| 63% | `nsmd/nsmChassis/nsmPCIeRetimer.cpp` | 33/90 | **EH_BRANCH** — constructor initializer list (gcov artifact) |
| 68% | `nsmd/nsmEvent/nsmRediscoveryEvent.cpp` | 33/104 | **BLOCKED** — `MctpDiscovery` singleton required |
| 70% | `nsmd/nsmPort/nsmEndpoint.cpp` | 19/65 | **EH_BRANCH** — constructor initializer list (gcov artifact) |
| 73% | `nsmd/nsmChassis/nsmPowerControl.cpp` | 38/145 | **EH_BRANCH** (L33–64) + **DEAD** (L129–156 loop getters: NsmPowerCap objects complex to build; L200 stub always returns 0) |
| 74% | `nsmd/nsmChassis/nsmPowerSubSystem.cpp` | 16/62 | **EH_BRANCH** — constructor initializer list (gcov artifact) |
| 75% | `nsmd/nsmChassis/nsmApSkuId.cpp` | 20/82 | **EH_BRANCH** — constructor initializer list (gcov artifact) |
| 76% | `nsmd/nsmProcessor/nsmFpgaProcessor.cpp` | 25/105 | **EH_BRANCH** — constructor initializer list (gcov artifact) |
| 78% | `nsmd/nsmPort/nsmZone.cpp` | 11/50 | **EH_BRANCH** — constructor initializer list (gcov artifact) |
| 79% | `nsmd/nsmChassis/nsmRoTProperty.cpp` | 100/498 | **EH_BRANCH** (constructors) + **DEAD** (2-value enum exhausts switch, hardcoded encode) + **BLOCKED** (imageCopyAsyncHandler, getActiveSlotComponentInfo need real D-Bus subtree) |
| 80% | `nsmd/nsmFwSwInventory/GPUSWInventory.cpp` | 25/128 | **EH_BRANCH** (constructor) + **BLOCKED** (real `sensorIO` required) |
| 80% | `nsmd/nsmPort/nsmFpgaPort.cpp` | 36/186 | **EH_BRANCH** — constructor initializer list (gcov artifact) |
| 81% | `nsmd/nsmNumericSensor/nsmEnergy.cpp` | 10/54 | **EH_BRANCH** — constructor + shared-memory macro lines (gcov artifact) |
| 81% | `nsmd/nsmNumericSensor/nsmTemp.cpp` | 10/54 | **EH_BRANCH** — constructor + shared-memory macro lines (gcov artifact) |
| 82% | `nsmd/nsmChassis/nsmLeakDetection.cpp` | 84/469 | **EH_BRANCH** (L42–118 constructor) + **DEAD** (L358–362 encode fail unreachable) + small testable remainder |
| 82% | `nsmd/nsmDebugToken/nsmDebugTokenNIC.cpp` | 67/389 | **EH_BRANCH** (constructors) + **DEAD** (protocol: decode strips reasonCode on NSM_SUCCESS → else branches unreachable) |
| 82% | `nsmd/nsmEvent/nsmLongRunningEvent.cpp` | 6/35 | **EH_BRANCH** — constructor initializer list (gcov artifact) |
| 83% | `nsmd/nsmChassis/nsmUpdateApSkuId.cpp` | 17/102 | **DEAD** — `validateApSkuIdFormat` enforces exactly 10 chars → stoul never overflows, pos always equals length, parsedValue always fits uint32 |
| 83% | `nsmd/nsmGPIO/nsmGPIOState.cpp` | 10/59 | **EH_BRANCH** — constructor initializer list (gcov artifact) |
| 83% | `nsmd/nsmGPM/nsmGpmOem.cpp` | 52/323 | **EH_BRANCH** (constructor multi-line initializers) + **TESTABLE**: `addGpmIntfProperty(name, dataType)` overload (L447–480) has zero tests — see Category G |
| 83% | `nsmd/nsmPort/nsmRetimerPort.cpp` | 199/1180 | **EH_BRANCH** — constructor initializer lists (L23–90 gcov artifact); factory branches now covered (see §9) |
| 83% | `nsmd/nsmNumericSensor/nsmPower.cpp` | 12/71 | **EH_BRANCH** — constructor + shared-memory macro lines (gcov artifact) |

**Root cause breakdown of all remaining uncovered lines (estimated):**
- ~60% Compiler-generated EH branches / gcov false negatives (tool limitation)
- ~25% Dead code (input validation / enum exhaustion / hardcoded instance IDs)
- ~15% Infrastructure-blocked (real MCTP/D-Bus/sensorIO required)

---

## Branch Coverage Analysis

### Current state: 72.4% branches (37,596 / 51,925) — 2026-03-18

The 12,836 not-taken branches fall into these categories:

#### Category A: gcov B50/B51 compound-condition false negatives
The build generates gcno/gcda in 'B50' format but the host gcov expects 'B51'. This
causes the **FALSE branch** of compound `&&` conditions to be reported as "not-taken"
even when tests exercise the error path.

**Pattern** (across hundreds of handleResponseMsg functions):
```cpp
if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS) { /* update */ }
// ↑ FALSE branch always "not taken" in report, even though
//   tests with NSM_ERROR cc DO execute the error path
```
**Not fixable** — tooling limitation.

#### Category B: Compiler-generated EH cleanup branches
The compiler inserts hidden exception-handling branches around constructor/destructor
member initializer lists and RAII cleanup code. These appear as "not-taken" in gcov
reports for every C++ project.

**Confirmed files**: `nsmThresholdValue.cpp` (all 6 branches at L26/38/50/62/74/86),
`nsmFabricManager.cpp` constructors (L45–82, L135–150), `nsmMemory.cpp` constructors
(91 branches), `nsmPciePortIntf.cpp` (L18–20), `nsmSensorAggregator.cpp` (L28/30),
`nsmPeakPower.cpp`, `nsmGpuOperationalStatus.cpp`.
**Not fixable** — compiler artifact.

#### Category C: `if constexpr` template branches
`nsmInterface.hpp` line 152 has **860 not-taken branches** — all from a single
`if constexpr (std::is_invocable_v<Func, std::string, IntfType&>)` inside
`NsmInterfaces<T>::invoke()`. Each template instantiation compiles only one
branch; gcov counts the dead branch of every other instantiation.
**Not fixable** — compile-time decision.

#### Category D: LG2 macro flood-control branches
`LG2_ERROR_FLT` expands to `if (SHOULD_LOG(...)) { ... }`. The FALSE branch
(flood suppression) requires the flood threshold to be reached during a test.
**Affected**: `nsmSetECCMode.cpp` L65, `nsmGPIOState.cpp` L48/69,
`nsmFabricManager.cpp` L198, and others.
**Not fixable** — requires timing/count conditions.

#### Category E: Concurrency-required branches
`nsmEvent.cpp` L73 — FALSE branch of `if (!cachedLoggingService)` inside mutex.
Requires a second concurrent call to set the cache between the outer check and the
inner check. Impossible in single-threaded tests.
**Not fixable** — architectural constraint.

#### Category F: Infrastructure-blocked success paths
- `nsmEvent.cpp` L93 — `co_return NSM_SW_SUCCESS` in logEventAsync (see above)
- `nsmRediscoveryEvent.cpp` handle() success path — MctpDiscovery singleton not initialized
- `nsmLongRunningEventHandler.cpp` — MctpDiscovery singleton not initialized (see above)
**Not fixable without architectural changes.**

#### Category G: Remaining testable opportunities

Two specific items have been identified as genuinely testable. Everything else has
been analysed and confirmed as EH_BRANCH, dead code, or infrastructure-blocked.

**G1. `nsmd/nsmGPM/nsmGpmOem.cpp` — `addGpmIntfProperty(name, dataType)` overload**

Lines 447–480 are completely untested. The function has three exercisable paths:
- Null `gpmIntf` (before `createGPMIntf()` is called) → early-return log at L449–451
- `DataType::Double` switch case → `register_property(name, NaN)` at L455–458
- `DataType::VectorDouble` switch case → `register_property(name, vector{})` at L459–462

**Estimated gain**: ~15 lines, ~6 branches.

**Test pattern**:
```cpp
// null gpmIntf path
NsmGPMInterfaceCreator creator(objServer, "/test/path");
creator.gpmIntf.reset();
EXPECT_NO_THROW(creator.addGpmIntfProperty("Metric", DataType::Double));

// valid gpmIntf + switch cases
NsmGPMInterfaceCreator creator2(objServer, "/test/path2");
creator2.createGPMIntf();
creator2.addGpmIntfProperty("DoubleMetric",  DataType::Double);
creator2.addGpmIntfProperty("VectorMetric",  DataType::VectorDouble);
```

**G2. `count()` FALSE branches in factory coroutines**

Every factory coroutine guards D-Bus property access with:
```cpp
if (allBaseIfaceProperties.count("Name"))
    name = std::get<std::string>(...);  // FALSE branch: name stays ""
```
Existing tests always provide complete property maps, so the FALSE branch is never
taken. These **are testable** — exercising them increments the gcov counter.

**Estimated remaining**: ~150–200 branches across the files below (~0.3–0.4% gain).

**Partially fixed**: commit d09a534a (nsmFpgaPort: Health, ChasisPowerState, PortType,
PortProtocol, Priority absent tests), commit 6208fb16 (nsmGpuPciePort: DeviceIndex,
ClearableScalarGroup; nsmChassisPCIeDevice: InventoryObjPath; nsmGpuClockControl:
else-if FALSE).

**Remaining files** (by count() check count in factory):

| File | ~count() checks | Key absent properties |
|------|----------------|-----------------------|
| `nsmd/nsmProcessor/nsmProcessor.cpp` | 26 | Name, UUID, per-type properties |
| `nsmd/nsmChassis/nsmIRoTResponder.cpp` | 13 | Multiple factory guards |
| `nsmd/nsmDeviceInventory/nsmSwitch.cpp` | 11 | Various per-type properties |
| `nsmd/nsmEvent/nsmXIDEvent.cpp` | 10 | Base property guards |
| `nsmd/nsmPort/nsmPort.cpp` | 9 | Health, ChasisPowerState, PortType |
| `nsmd/nsmChassis/nsmNVSwitchAndNVMgmtNICChassis.cpp` | 9 | Chassis-specific |
| `nsmd/nsmNumericSensor/nsmThresholdEvent.cpp` | 8 | Threshold properties |
| `nsmd/nsmChassis/nsmChassisAssembly.cpp` | 8 | Assembly properties |
| `nsmd/nsmMemory/nsmMemory.cpp` | 7 | ErrorCorrection, DeviceType, Priority |
| `nsmd/nsmChassis/nsmChassisLED.cpp` | 4 | Name, Type, UUID, InventoryObjPath |
| `nsmd/nsmPort/nsmZone.cpp` | 4 | Zone-specific properties |
| `nsmd/nsmEventConfig/nsmEventConfig.cpp` | 4 | EventConfig properties |

**Test pattern**:
```cpp
auto& pm = utils::MockDbusAsync::propertyMap(objPath, interface);
pm["UUID"] = uuid;
pm["InventoryObjPath"] = std::string("/xyz/.../device");
// "Name" and "Type" intentionally omitted → count() returns 0 → FALSE branches taken
createNsmXxxSensor(mockManager, interface, objPath);
```

---

## Compilation-Blocked Tests

### `nsmd/nsmPort/nsmRetimerPort.cpp` — RESOLVED

`createNsmPCIeRetimerPorts` and `createNsmMultiPCIeRetimerPorts` — `static` keyword
removed (2026-03-10). Tests added in `nsmRetimerPort_test.cpp` (12 tests) and
`nsmNullDeviceBranch_test.cpp` (2 tests). Coverage: 66% → 83%.

### `nsmd/nsmPort/nsmPort.cpp` — RESOLVED

`createNsmPortSensorWithNetworkPortAddresses` and `createNsmPortSensorGeneric` —
`static` keyword removed (2026-03-10). Tests added in `nsmPort_test.cpp` (2 tests).

---

## DISABLED Tests (~50 total)

Tests prefixed with `DISABLED_` are compiled but not run. Common reasons:

| Reason | Example |
|--------|---------|
| Sync validation not in production code | DISABLED_SetPowerCap_AboveMax in some fixtures |
| MockSensorManager always creates device (null branch unreachable) | Some factory null-device tests |
| Valgrind false positive isolation | nsmRawCommandHandler valgrind sdbusplus false positive |
| Test infrastructure mismatch | GetBlobHandlesEmptyFile behavior changed |
| sdbusplus property getter/setter disconnect | `DISABLED_RequestImageCopy_AlreadyProcessing_Throws` — `imageCopyRequestStatus(Processing)` setter not reflected by `ImageCopyRequestStatus()` getter in test D-Bus environment |

---

## History — Previously Blocked, Now Solved

The following were documented as blocked in earlier sessions but have since been
resolved with tests:

| File | Old Status | Current Coverage | Solution |
|------|-----------|-----------------|----------|
| `nsmd/socket_handler.cpp` | 0% — "socket operations require real system calls" | **98%** (246/249 lines) | `socket_handler_test.cpp` using `socketpair()` + `MctpTestFixture`; 3 uncovered lines (303–305) are unreachable dead code in a `[[noreturn]]` error path |
| `nsmd/sensorQueueMap.hpp` | "compilation fails — excess elements in struct initializer" | **97%** (33/34 lines) | `sensorQueueMap_test.cpp` fixed initialization; line 33 is a closing brace (gcov artifact) |
| `nsmd/nsmDebugToken/nsmDebugTokenAggregation.cpp` | "file I/O, TLV parsing, async operations unblocked" | **92%** (190/206 lines) | Tests in `nsmKeyMgmtAndDebugToken_test.cpp` using mockSensorIO; remaining 16 lines are constructor EH branches (gcov artifacts) |
| `nsmd/nsmFirmwareUtils/nsmKeyMgmt.cpp` | "coroutine-based key management, NsmDevice mock needed" | **89%** (148/165 lines) | Tests in `nsmKeyMgmtAndDebugToken_test.cpp`; uncovered: coroutine infrastructure-blocked paths |
| `nsmd/nsmPort/nsmGpuPciePort.cpp` | "coroutine async, NsmDevice mock" | **86%** (283/328 lines) | Tests in `nsmGpuPciePort_test.cpp`; uncovered: gcov B50/B51 artifacts (lines 39–119) |
| `nsmd/nsmChassis/nsmRoTProperty.cpp` | "similar to nsmKeyMgmt, likely coroutine-based" | **79%** (398/498 lines) | Tests in `nsmChassis/test/`; uncovered: coroutine bodies and EH branches |
| `nsmd/nsmDumpCollection/nsmLogInfo.cpp` | "1-bit bitfield blocks default: branch" | **86%** (88/102 lines) | Tests in `nsmLogInfo_test.cpp`; lines 121–123 remain (1-bit bitfield, see above) |
| `requester/mctp.c` (libnsm) | "requires real sendRecvFn" | **100%** | Socket mocking infrastructure (MctpTestFixture) |
| `nsmd/sensorManager.cpp` (static helpers) | "extensive unmockable dependencies, 0%" | **29%** (static helpers covered) | Static helper methods extracted and tested directly; event-driven portion remains blocked |
| Factory `if (!nsmDevice)` error paths | "mockManager always creates devices" | **Covered** | `NullReturnMockSensorManager` fixture; 19 null-device tests across 8 subsystems |
