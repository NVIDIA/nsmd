# Failing Tests - nsmProcessor

## Currently Failing Tests

No tests currently failing. All 279 tests pass.

## Previously Removed Tests (Never Added to Suite)

### createNsmProcessorSensor_NSMProcessor_NoSlashPath

**Target**: Cover line 3516 branch 1 (`pos == std::string::npos`)

**Reason for removal**: The test failed with:
```
sd_bus_add_object_vtable: org.freedesktop.DBus.Error.InvalidArgs: Invalid argument
```

D-Bus requires object paths to start with '/'. When `inventoryObjPath` does not
contain '/', the path is invalid and the D-Bus mock rejects it before the code
at line 3516 is reached.

**Branch impact**: Line 3516 branch 1 remains at 0%.

**Status**: Documented in BLOCKED_TESTS.md as uncoverable without source changes.
