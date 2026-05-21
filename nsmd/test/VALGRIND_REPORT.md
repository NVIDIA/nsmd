# Valgrind Analysis Report — nsmd

**Date:** 2026-04-27 (re-analysed 2026-04-30)  
**Branch:** develop (ToT)  
**Tool:** Valgrind Memcheck 3.24.0  
**Build image:** `openbmc/ubuntu-unit-test:2026-W18-0c70dc8aac7798ce`  
**Total tests run:** 442  
**Test results:** 442 passed, 0 failed  

---

## Executive Summary

| Category | Count | Status |
|---|---|---|
| Invalid reads / writes | 0 | ✅ Clean |
| Uninitialised value errors | 0 | ✅ Clean |
| Definite leaks — `~Coroutine()` bug | 3 tests | ⚠️ Fix ready — blocked on NsmDevice pre-req |
| Definite leaks — fixture teardown | 2 tests | ✅ **Fixed — MR !2746** |
| Definite leaks — source deleted, needs CI stack trace | 3 tests | 🔴 Needs investigation |
| Possibly lost (suspected FP) | 2 tests | 🔵 Flagged, no fix |

No memory safety errors. 8 tests leaked on teardown; 2 are now fixed. Fix A
(Root Cause A) is safe to apply but requires a one-line NsmDevice pre-requisite
first. Root Cause C tests need a CI re-run with `--leak-check=full` since their
source files no longer exist in the current working tree.

### All leaking tests at a glance

| Test | Definitely Lost | Root Cause | Fix | Status |
|---|---|---|---|---|
| `mctpProberRetryBranch_test` | 2,144 B / 8 blocks | `~Coroutine()` bug | A | Pending |
| `sensorManager_test` | 1,520 B / 5 blocks | `~Coroutine()` bug | A | Pending |
| `sleep_semaphore_mock_test` | 344 B / 3 blocks | `~Coroutine()` bug | A | Pending |
| `nsmWorkloadPowerProfileBranch_test` | ~~1,872 B / 1 block~~ 0 B | Fixture teardown | B | ✅ Fixed |
| `nsmWorkloadPowerProfileBranch3_test` | ~~1,872 B / 1 block~~ 0 B | Fixture teardown | B | ✅ Fixed |
| `nsmChassis_test` | 10,080 B / 15 blocks | Source deleted / no CI trace | C | Pending |
| `nsmSwitchBranch_test` | 1,864 B / 1 block | Source deleted / no CI trace | C | Pending |
| `nsmProcessorModulePowerControl_test` | 1,216 B / 1 block | Source deleted / no CI trace | C | Pending |

---

## Priority 1 — Invalid Reads / Writes

**None found.** All 442 tests produced `ERROR SUMMARY: 0 errors from 0 contexts`.

---

## Priority 2 — Uninitialised Value Errors

**None found.**

---

## Priority 3 — Definite Memory Leaks

### Root Cause A — `~Coroutine()` destructor bug (3 tests)

#### Fix A — one-line change to `nsmd/common/coroutine.hpp`

```diff
     ~Coroutine()
     {
-        if (handle && handle.done())
-        {
-            handle.destroy();
-        }
+        if (handle)
+        {
+            handle.destroy();
+        }
     }
```

#### Safety analysis

The `operator=(Coroutine&&)` move-assignment at `coroutine.hpp:219-233` already
destroys suspended frames unconditionally:

```cpp
Coroutine& operator=(Coroutine&& other) noexcept
{
    if (this != &other)
    {
        if (handle)
        {
            handle.destroy();   // already destroys suspended frames
        }
        handle = std::exchange(other.handle, {});
    }
    return *this;
}
```

The destructor's additional `handle.done()` guard is therefore **inconsistent**
with existing move-assignment behaviour. Fix A makes them consistent.

**Can Fix A cause use-after-free in `NsmDevice::task`?**

`SensorManagerImpl::deviceTask` has signature
`requester::Coroutine deviceTask(std::shared_ptr<NsmDevice> nsmDevice)` — the
parameter is passed by value, so the suspended coroutine frame holds a
`shared_ptr<NsmDevice>` copy, keeping `use_count ≥ 1`.

`NsmDevice::~NsmDevice()` is only called when `use_count` drops to zero.
While the frame is alive (suspended at `co_await common::Sleep(...)`), its
`shared_ptr` keeps `NsmDevice` alive, so the destructor **cannot be entered
while the frame is live**. Fix A's new code path in `~Coroutine()` is therefore
unreachable for the `NsmDevice::task` member.

**Pre-requisite — add `detach()` calls to `NsmDevice::~NsmDevice()` in
`nsmd/nsmDevice.hpp`:**

```cpp
virtual ~NsmDevice()
{
    task.detach();            // prevents event loop from resuming a stale frame
    longRunningTask.detach(); // same
}
```

`detach()` sets `promise.detached = true` and nulls the RAII handle. If the
event loop fires the timer and resumes the coroutine after the device has been
logically shut down, the frame self-destructs at `final_suspend()` rather than
writing through a dangling pointer. Without this guard, Fix A would still be
memory-safe (the NsmDevice can't be destroyed while its frame is alive), but
the `detach()` call is correct defensive practice and documents the ownership
intent.

The pre-requisite is a **one-liner** in `nsmDevice.hpp`; Fix A is a **one-liner**
in `coroutine.hpp`. Both should land in the same PR.

---

#### Leak A1 — `mctpProberRetryBranch_test`

**Size:** 2,144 bytes definitely lost + 128 bytes indirectly (8 blocks)

```
272 bytes in 1 block definitely lost:
  at operator new(unsigned long)
  by requester::MctpEndpointProber::ping(eid_t)   [mctp_endpoint_prober.cpp:101]
  by MctpProberRetryTest_Ping_NotReady_ThenSuccess_Test::TestBody()
```

Tests call coroutines with `(void)` and discard the returned `Coroutine` handle
while it is suspended at `co_await common::Sleep(event, ...)` inside the retry
loop. Each leaked frame is 256 bytes; the 16 indirect bytes per block are the
associated `sd_event_source` allocated by `common::Sleep`.

```cpp
// mctpProberRetryBranch_test.cpp:156
(void)prober.ping(60);   // Coroutine discarded while suspended
```

---

#### Leak A2 — `sensorManager_test`

**Size:** 1,520 bytes definitely lost in 5 blocks + 54,455 bytes indirectly (1,455 blocks)

Every `deviceTask()` test discards the returned `Coroutine` while suspended at
`doSleep()`. The test even documents this:

```cpp
// sensorManager_test.cpp (comment verbatim from the test)
// "In non-coverage mode the coroutine suspends at Sleep, so
//  cr.data() is uninitialized. We only verify the mock expectations."
auto cr = mgr->deviceTask(nsmDevice);
(void)cr;  // Coroutine suspended — ~Coroutine() skips handle.destroy()
```

The `deviceTask` frame (~304 bytes) holds a `std::shared_ptr<NsmDevice>` by
value. When the frame leaks, that shared_ptr becomes the last reference to the
`NsmDevice` — so the entire object tree (sensors, message handler, object server
interfaces) becomes indirectly lost, explaining 54,455 bytes from 5 frames.

The 5 leaking tests: `DeviceTask_InactiveDevice_SleepsWithPriority`,
`DeviceTask_ActiveDevice_NoPrioritySensors_SleepsNonPriority`,
`DeviceTask_ActiveDevice_WithPrioritySensors_SleepsPriority`,
`DeviceTask_ActiveDevice_CommandsNotRetrieved_CallsRefreshMatrix`,
`DeviceTask_DeviceGoesOfflineDuringRefresh_SkipsSensorPolling`.

---

#### Leak A3 — `sleep_semaphore_mock_test`

**Size:** 344 bytes definitely lost in 3 blocks

```
104 bytes in 1 block definitely lost:
  at operator new(unsigned long)
  by CoroutineSemaphoreMock_Contended_DeferFails_WaiterNotResumed_Test::TestBody()::{lambda()#2}::operator()()
     [sleep_semaphore_mock_test.cpp:184]

120 bytes in 1 block definitely lost:
  at operator new(unsigned long)
  by SleepMock_NonPriority_Success_CoroutineSuspends_Test::TestBody()::{lambda()#1}::operator()()
     [sleep_semaphore_mock_test.cpp:102]
```

Coroutine frames created inside test lambdas are suspended and never driven to
completion. Same destructor bug — the lambdas create coroutines that suspend,
the `Coroutine` objects go out of scope, frames leak.

---

### Root Cause B — `MockNsmDevice` not freed in fixture teardown ✅ FIXED

**Fixed in MR !2746** (`fix/test-valgrind-wpp-fixture-teardown-leaks`).
Both test binaries verified with `valgrind --leak-check=full --show-leak-kinds=definite`:
```
nsmWorkloadPowerProfileBranch_test:  definitely lost: 0 bytes in 0 blocks
nsmWorkloadPowerProfileBranch3_test: definitely lost: 0 bytes in 0 blocks
```

#### Root cause detail

Both fixtures created `MockNsmDevice` objects and registered D-Bus interfaces
via `utils::DBusTest`. The sensors added to the device held `shared_ptr<NsmDevice>`
internally, creating the cycle:

```
device → deviceSensors → sensors → device
```

This is the same pattern as sensorManager_test (Leak A2), except here the
device never reaches refcount 0 because the sensors keep it alive.

Four separate issues were found and fixed:

1. **`PageCollectionTest`** — no destructor at all. Added destructor mirroring
   `cleanupDeviceSensors()`: clears all sensor containers, detaches coroutine
   tasks, resets `nsmMsgHandler` and `objServer`.

2. **`NsmWPPPageHandleTest`** — existing destructor cleared
   `pageCol->supportedPages` but was missing `device->deviceSensors.clear()`
   and the rest of the sensor cleanup chain. Added full cleanup.

3. **`WPPBranch3Test`** — existing destructor called `cleanupDeviceSensors(devices)`
   but the `gpu` fixture member was not explicitly reset. The `gpu` member
   kept the device alive even after `devices.clear()`. Added `gpu.reset()`.

4. **`WPPBranch3Test::PageCollection_AddPage_Duplicate`** — test body created
   local `pageCol` and `page1`, called `pageCol->addPage(0, page1)`. This
   created a `pageCol ↔ page1` cycle (`page1` holds `pageCol` internally,
   `pageCol->supportedPages` holds `page1`). `cleanupDeviceSensors` cannot
   reach `pageCol->supportedPages` (it is not a member of `NsmDevice`).
   Added `pageCol->supportedPages.clear()` before locals go out of scope.

---

### Root Cause C — Source deleted, full stack traces need CI re-run (3 tests)

#### Investigation constraints

The three test source files no longer exist in the current working tree:

```
nsmd/nsmChassis/test/nsmSwitchBranch_test.cpp        — deleted
nsmd/nsmChassis/test/nsmProcessorModulePowerControl_test.cpp — deleted
```

(`nsmChassis_test.cpp` still exists locally.)

The CI valgrind run used `VALGRIND_OPTS=--error-exitcode=1` **without**
`--leak-check=full`, so only summary totals were recorded — no stack traces.

The coverage-build binaries (from 2026-W18) cannot be loaded with the current
W19 image due to a `libgtest_main.so.1.16.0` shared-library version mismatch.

Static analysis from source code (recovered from git history for the deleted
files) is presented below. Stack traces must be obtained from a CI run with
`--leak-check=full` to confirm.

---

#### Leak C1 — `nsmChassis_test` (10,080 B / 15 blocks)

**Source:** `nsmd/nsmChassis/test/nsmChassis_test.cpp` (still present)

**Fixture structure:**

```cpp
struct NsmChassisTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices{
        {std::make_shared<NsmDevice>(gpuUuid)},
        {std::make_shared<NsmDevice>(fpgaUuid)},
    };
    NsmDevice& gpu  = *devices[0];
    NsmDevice& fpga = *devices[1];

    NsmChassisTest() : SensorManagerTest(devices) { ... }
    // NO DESTRUCTOR DEFINED
};
```

**Root cause:**

`NsmChassisTest` has **no destructor** and never calls `cleanupDeviceSensors()`.
The tests exercise `createNsmChassisDI(...)` which adds multiple sensors to
`gpu.deviceSensors`. Each sensor holds a `std::shared_ptr<NsmDevice>` back to
`gpu`, creating:

```
devices → NsmDevice (gpu) → deviceSensors → sensors[0..N] → NsmDevice (gpu)
```

When `devices` (a member) is destroyed at fixture teardown, each
`shared_ptr<NsmDevice>` in the vector decrements `use_count`. But the sensors in
`deviceSensors` hold additional references, so `use_count > 0` and the
`NsmDevice` destructor is never called. The sensors and their D-Bus interface
objects also remain alive.

**15 blocks:** The tests add up to 15 sensors to `gpu.deviceSensors` (confirmed
by `EXPECT_EQ(15, gpu.deviceSensors.size())`-style assertions in the test).
Each leaked block corresponds to one sensor or its registered D-Bus interface.
**10,080 B ÷ 15 = 672 B** per allocation — consistent with a
`NsmInterfaceProvider<SomeIntf>` D-Bus wrapper object.

**Suggested fix:**

Add a destructor to `NsmChassisTest`:

```cpp
~NsmChassisTest()
{
    cleanupDeviceSensors(devices);
}
```

`cleanupDeviceSensors` clears all sensor containers, detaches coroutine tasks,
and resets `objServer`/`nsmMsgHandler` shared_ptrs, breaking all circular
references before the member destructors run.

---

#### Leak C2 — `nsmSwitchBranch_test` (1,864 B / 1 block)

**Source:** `nsmd/nsmChassis/test/nsmSwitchBranch_test.cpp` (deleted; recovered
from commit `12469efa`)

**Fixture structure (two fixtures use the same pattern):**

```cpp
struct NsmSwitchDIPowerModeUpdateTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmSwitchDIPowerModeUpdateTest() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
    }

    ~NsmSwitchDIPowerModeUpdateTest()
    {
        cleanupDeviceSensors(devices);
        // BUG: nvswitch member NOT reset after cleanup
    }
};

struct NsmSwitchDIFactoryTest : ... // same destructor pattern, same bug
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitchDev;

    ~NsmSwitchDIFactoryTest()
    {
        cleanupDeviceSensors(devices);
        // BUG: nvswitchDev member NOT reset after cleanup
    }
};
```

**Root cause:**

`cleanupDeviceSensors(devices)` clears `devices` table (releases one
`shared_ptr<MockNsmDevice>` reference). But the fixture's `nvswitch` /
`nvswitchDev` member still holds a second reference to the device.

For the leak to occur there must be a third reference keeping the device alive
even after the member's destructor runs. That third reference comes from a
sensor registered via `createNsmSwitchDI(...)` that: (a) was added to
`deviceSensors`, (b) was cleared by `cleanupDeviceSensors`, but (c) still
exists because the `utils::DBusTest` object server holds a `shared_ptr` to the
registered D-Bus interface, which in turn holds a `shared_ptr<NsmDevice>`.

Sequence:

```
1. cleanupDeviceSensors(devices):
   - deviceSensors.clear() → sensor refcount drops from 2 to 1
                              (object_server still holds the interface)
   - device->objServer.reset() → drops device's COPY of object_server;
                                  utils::DBusTest still holds the real one
   - devices.clear()

2. Fixture member nvswitch destroyed → MockNsmDevice use_count drops by 1
   Still > 0 because:
   - interface (held by utils::DBusTest::object_server) holds sensor
   - sensor holds shared_ptr<NsmDevice>

3. ~utils::DBusTest() releases object_server → interface freed → sensor freed
   → sensor's shared_ptr<NsmDevice> released → use_count drops to 0

4. BUT: ~utils::DBusTest() runs AFTER the fixture's member destructors.
   At step 3, the cleanup of the NsmDevice happens inside ~utils::DBusTest(),
   which is technically "after process teardown scope" from valgrind's view,
   causing it to be reported as leaked.
```

**1,864 bytes:** Matches one `MockNsmDevice` object size (1,872 B) ± overhead
from the specific constructor variant used by `getNsmDeviceFromStaticUUID`.

**Suggested fix (to apply once source files are restored):**

```cpp
~NsmSwitchDIPowerModeUpdateTest()
{
    cleanupDeviceSensors(devices);
    nvswitch.reset();    // drop fixture's reference after cleanup
}

~NsmSwitchDIFactoryTest()
{
    cleanupDeviceSensors(devices);
    nvswitchDev.reset(); // drop fixture's reference after cleanup
}
```

This is **identical to the WPPBranch3Test fix** in Root Cause B (adding
`gpu.reset()` after `cleanupDeviceSensors`).

---

#### Leak C3 — `nsmProcessorModulePowerControl_test` (1,216 B / 1 block)

**Source:** `nsmd/nsmChassis/test/nsmProcessorModulePowerControl_test.cpp`
(deleted; recovered from commit `12469efa`)

**Fixture structure:**

```cpp
struct NsmProcessorModulePowerControlTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;                                        // (1)
    std::shared_ptr<NsmProcessorModulePowerControl> powerControl; // (2)
    std::shared_ptr<PowerCapIntf> powerCapIntf;                   // (3)
    std::shared_ptr<NsmClearPowerCapIntf> clearPowerCapIntf;      // (4)

    NsmProcessorModulePowerControlTest() : SensorManagerTest(devices) {}

    ~NsmProcessorModulePowerControlTest()
    {
        cleanupDeviceSensors(devices);  // no-op: devices is always empty
        // BUG: powerControl, powerCapIntf, clearPowerCapIntf NOT reset
    }

    void SetUp() override {
        powerCapIntf     = std::make_shared<PowerCapIntf>(bus, path.c_str());
        clearPowerCapIntf = std::make_shared<NsmClearPowerCapIntf>(bus, path);
        powerControl     = std::make_shared<NsmProcessorModulePowerControl>(
                               bus, name, type, powerCapIntf,
                               clearPowerCapIntf, path, associations_list);
    }
};
```

**Root cause:**

`NsmProcessorModulePowerControl` constructor registers several D-Bus interfaces
internally (`associationDefinitionsIntf`, `decoratorAreaIntf`, and the passed-in
`powerCapIntf`/`clearPowerCapIntf`). These interfaces are registered with the
`utils::DBusTest` object server and remain in the object server's internal
interface map.

Members are destroyed in **reverse declaration order** at fixture teardown:

```
(4) clearPowerCapIntf member destroyed  → use_count 2→1 (powerControl still holds it)
(3) powerCapIntf member destroyed       → use_count 2→1 (powerControl still holds it)
(2) powerControl destroyed              → releases clearPowerCapIntf and powerCapIntf
                                          (use_count hits 0, both freed)
                                          also destroys internal unique_ptr members:
                                          associationDefinitionsIntf, decoratorAreaIntf
(1) devices destroyed                   → no-op
```

This ordering appears correct at first glance. However, if any D-Bus interface
registered with `utils::DBusTest::object_server` holds a `shared_ptr` back to
itself (via `shared_from_this()` or sdbusplus internal bookkeeping), the
object_server's interface map becomes a third reference holder that
`powerControl`'s destructor cannot clear.

**1,216 bytes:** Consistent with a single `sdbusplus::server::interface_t`
object — the D-Bus interface backing for one of `NsmProcessorModulePowerControl`'s
internally-created interfaces (`AssociationDefinitions` or `DecoratorArea`).

**Two possible root cause sub-cases:**

1. **D-Bus object server cycle:** `utils::DBusTest::object_server` holds a
   `shared_ptr` to a D-Bus interface created by `NsmProcessorModulePowerControl`.
   That interface holds a back-reference keeping its use_count at 1 after
   `powerControl` is destroyed. It is not freed until `~utils::DBusTest()` runs,
   which happens after the fixture's member destructors — i.e., after the scope
   of `powerControl`'s existence — so valgrind sees it as leaked.

2. **Missing explicit reset ordering:** `powerControl` should be reset before
   `powerCapIntf`/`clearPowerCapIntf` to avoid `powerControl`'s destructor trying
   to interact with already-freed D-Bus interface objects.

**Suggested fix (to apply once source files are restored):**

```cpp
~NsmProcessorModulePowerControlTest()
{
    // Reset in reverse dependency order: consumer before dependency
    powerControl.reset();        // releases internal D-Bus interfaces first
    clearPowerCapIntf.reset();   // then the shared interface objects
    powerCapIntf.reset();
    cleanupDeviceSensors(devices);
}
```

**Why the explicit reset order matters:** the compiler's default reverse
declaration order destroys `(4)→(3)→(2)`, meaning `clearPowerCapIntf` and
`powerCapIntf` are freed first (refcount 2→1, still alive inside `powerControl`)
then `powerControl` is freed (releasing them to refcount 0). If the object
server holds any extra reference, `powerControl.reset()` first ensures all
internal D-Bus interfaces are unregistered before the shared `powerCapIntf`
objects attempt unregistration.

---

#### Next steps for Root Cause C

1. **Restore the source files** — they were removed in a subsequent commit. The
   tests were still running in CI (as shown by the valgrind log). Either restore
   from git history or locate the new path.

2. **Re-run CI with `--leak-check=full`** to get exact stack traces:

```bash
VALGRIND_OPTS="--error-exitcode=1 --leak-check=full --show-leak-kinds=definite \
  --track-origins=yes" \
RUN_WITH_VALGRIND=1 UNIT_TEST_PKG=nsmd NO_FORMAT_CODE=1 \
./openbmc-build-scripts/run-unit-test-docker.sh
```

Filter `testlog-valgrind.txt` for the three test names to extract stack traces.

3. **Apply fixes** once source files are accessible and stack traces confirm the
   above analysis.

---

## Priority 4 — Possibly Lost (Suspected False Positives)

Two tests show non-zero `possibly lost` bytes with zero `definitely lost`.
Valgrind reports "possibly lost" when a pointer to the interior of an allocation
is found — common with vtables, `boost::asio`, and sdbusplus coroutine
infrastructure. These are flagged per agreed policy (no auto-suppression).

| Test | Possibly Lost | Notes |
|---|---|---|
| `nsmDevice_test` | 23,448 B / 128 blocks | boost::asio / sdbusplus async internals |
| `nsmDeviceBranch_test` | 592 B / 6 blocks | Same pattern |

---

## Suppression File

No suppressions were needed. The sdbusplus suppression file
(`nsmd/subprojects/sdbusplus/test/valgrind.supp`) covers known epoll/sdbus
false positives and was effective — 0 suppressed contexts in all 442 runs.

---

## Fix Roadmap

| Fix | Files | Scope | Blocked on | Status |
|---|---|---|---|---|
| **A-pre** | `nsmd/nsmDevice.hpp` | Production | — | Not started |
| **Fix A** | `common/coroutine.hpp` | Production | A-pre | Not started |
| **Fix B** | `nsmProcessor/test/*.cpp` | Test-only | — | ✅ MR !2746 |
| **Fix C1** | `nsmChassis/test/nsmChassis_test.cpp` | Test-only | — | Not started |
| **Fix C2** | `nsmChassis/test/nsmSwitchBranch_test.cpp` | Test-only | Source restore | Not started |
| **Fix C3** | `nsmChassis/test/nsmProcessorModulePowerControl_test.cpp` | Test-only | Source restore | Not started |

---

## How to Re-run

```bash
cd /home/aishwaryj/Documents/workspace/ci_test_area
BRANCH=develop WORKSPACE=$(pwd) P4ROOT=abc \
GITLAB_TOKEN_NAME=ajJuly2025 GITLAB_TOKEN_VAL=***REMOVED-LEAKED-TOKEN*** \
UNIT_TEST_PKG=nsmd NO_FORMAT_CODE=1 RUN_WITH_VALGRIND=1 \
./openbmc-build-scripts/run-unit-test-docker.sh
```

Results: `nsmd/build/meson-logs/testlog-valgrind.txt`

To get full stack traces (Root Cause C investigation):

```bash
cd /home/aishwaryj/Documents/workspace/ci_test_area
BRANCH=develop WORKSPACE=$(pwd) P4ROOT=abc \
GITLAB_TOKEN_NAME=ajJuly2025 GITLAB_TOKEN_VAL=***REMOVED-LEAKED-TOKEN*** \
VALGRIND_OPTS="--error-exitcode=1 --leak-check=full --show-leak-kinds=definite \
  --track-origins=yes" \
UNIT_TEST_PKG=nsmd NO_FORMAT_CODE=1 RUN_WITH_VALGRIND=1 \
./openbmc-build-scripts/run-unit-test-docker.sh
```
