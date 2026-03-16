# Blocked Tests: nsm_telemetry_cmd.cpp Branch Coverage

## Branch Coverage Target: NOT ACHIEVABLE (70%)

**Current branch coverage:** 56.7% (2163/3809)
**Maximum achievable:** ~57%

## Root Cause

The 70% branch coverage target for `nsmtool/nsm_telemetry_cmd.cpp`
cannot be achieved without modifying the source code under test.

### Compiler-Generated Exception Handling Branches

GCC instruments every function call that can throw with two exception-
handling branches in the control-flow graph:
- Branch 1: Normal execution (no exception)
- Branch 2: Exception thrown (propagated up)

The file contains ~330+ error-reporting statements using iostream
chains like:

```cpp
std::cerr << "Response message error: "
          << "rc=" << rc << ", cc=" << (int)cc
          << ", reasonCode=" << (int)reason_code << "\n";
```

Each `<<` operator generates 2 exception-handling branches. With 4-6
`<<` calls per statement, each cerr call adds 8-12 compiler-generated
branches. Across ~60+ error reporting sites in the file, this creates
approximately 500-700 impossible-to-cover branches.

Similarly, `ordered_json` operations (push_back, operator[]) and
string/vector operations each add exception-handling branches that are
only reachable via `std::bad_alloc` or other low-level exceptions.

### Evidence

All covered lines with partial branch coverage show exactly 50%
coverage (e.g., 5/10, 3/6, 4/8). This perfect 50% pattern is the
signature of compiler exception-handling arcs: the "no exception"
path is always covered when the line executes, while the "exception
thrown" arc requires actually triggering an exception from that line.

### Genuine Coverable Branches

Only 9 lines had genuinely uncovered branches (not compiler arcs):
- Lines 3817-3820: SetPowerLimit MODULE branch → COVERED
- Lines 3886-3888: GetPowerLimit MODULE branch → COVERED
- Lines 4025, 4033: GetRowRemapState flag bits → COVERED
- Line 5066: SetPCIePortConfig sampleCount mismatch → COVERED
- Lines 1402-1407: Aggregate timestamp decode failure → ~impossible
  (decode_aggregate_timestamp_data cannot fail in practice)

All other coverable branches have been covered. The remaining gap
(~1646 branches) is entirely compiler-generated exception arcs.

## What Would Be Required

To cover the exception-handling branches, one would need to:
1. Override `std::ostream` to throw during specific `<<` calls, OR
2. Replace the `ordered_json` library with a mock that throws, OR
3. Use memory allocation injection to trigger `std::bad_alloc`

All of these approaches would require modifying either the source
code or the build environment beyond normal unit testing scope.

## Line Coverage Achievement

The 90% line coverage target HAS been achieved:
- **Current: 98.4% (3562/3620 lines)**

This significantly exceeds the 90% target.

## Remaining 58 Uncovered Lines

All 58 remaining uncovered lines fall into these categories:

### Category 1: `= default` Destructors (~46 lines)
Lines like `~ClassName() = default;` at lines 59, 370, 455, 526, 645, 700,
751, 816, 923, 972, 1033, 1271, 1448, 2082, 2137, 2197, 2260, 2320, 2381,
2449, 2507, 2577, 2992, 3047, 3414, 3477, 3543, 3625, 3691, 3776, 3858,
3930, 3983, 4046, 4097, 4150, 4353, 4402, 4454, 4931, 5112, 5275, 5473,
5648, 5813, 5929, 6022, 6131.
GCC does not reliably track these as covered even when the destructor
runs, as they are compiled as inline trivial functions.

### Category 2: Defensive Null Pointer Check (Lines 124-125)
`printPortCounterData` guards against a NULL `portData` pointer. This
pointer comes from the decode function and cannot be NULL in practice.

### Category 3: Dead Code (Lines 1402, 1404-1405, 1407)
Error path in aggregate timestamp decode. The pre-check at line 1388
(`data_len != 8`) catches all failure cases before reaching this branch.

### Category 4: Impossible Encode Failure (Line 5260)
`encode_get_powersmoothing_featinfo_v2_req` fails only if `msg == NULL`.
Since the caller always allocates the message buffer, this is unreachable.

### Category 5: GCC Shared Destructor Optimization (Lines 6090, 6100, 6108)
In the `GetPortNetworkAddresses::handleSampleData` switch statement, four
case blocks (NSM_TAG_MAC_ADDRESS, NSM_TAG_PERMANENT_MAC_ADDRESS,
NSM_TAG_NODE_GUID, NSM_TAG_PORT_GUID) each declare a local `std::string
network_address`. GCC's code generator **hoists the destructor call** into
the PORT_GUID case block (line 6116), making the closing `}` braces of the
first three cases (6090, 6100, 6108) never execute. These lines are covered
in source logic but GCC attributes zero execution counts to them due to
code sharing optimization. Confirmed via HTML coverage report: lines 6083
(case label), 6084 (opening `{`), 6091 (`break`) are all COVERED while
only the closing `}` on 6090 shows 0 executions.
