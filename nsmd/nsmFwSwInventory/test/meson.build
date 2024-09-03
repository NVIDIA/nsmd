dep_src_files = [
    '../NVLinkManagementNICSWInventory.cpp',
    '../nsmFirmwareInventory.cpp',
    '../nsmWriteProtectedControl.cpp',
    '../../nsmDevice.cpp',
    '../../nsmObjectFactory.cpp',
    '../../sensorManager.cpp',
    '../../nsmSensor.cpp',
    '../../nsmSensors/nsmInventoryProperty.cpp',
    '../../nsmSetAsync/asyncOperationManager.cpp',
    '../../nsmSetAsync/nsmSetWriteProtected.cpp',
    '../../nsmDbusIfaceOverride/nsmAssetIntf.cpp',
    '../../../common/utils.cpp',
    '../../../common/test/mockDBusHandler.cpp',
    '../../../libnsm/base.c',
    '../../../libnsm/platform-environmental.c',
    '../../../libnsm/device-configuration.c',
    '../../../libnsm/diagnostics.c',
    '../../../libnsm/requester/mctp.c',
    '../../../libnsm/instance-id.c',
    '../../../requester/request_timeout_tracker.cpp',
]

dep_src_headers = [
    '.',
    '..',
    '../../',
    '../../nsmSensors',
    '../../nsmSetAsync',
    '../../nsmDbusIfaceOverride',
    '../../../',
    '../../../common',
    '../../../libnsm',
]

test_src = declare_dependency(
    sources: dep_src_files,
    include_directories: dep_src_headers,
)

tests = [
    'NVLinkManagementNICSWInventory_test',
    'nsmFirmwareInventory_test',
]

tests_deps = [
    test_src,
    CLI11_dep,
    nlohmann_json,
    sdbusplus,
    sdeventplus,
    phosphor_logging,
    nvidia_shmem,
    gtest,
    gmock,
]

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            implicit_include_directories: false,
            link_args: dynamic_linker,
            build_rpath: '',
            dependencies: tests_deps,
            cpp_args: '-DMOCK_DBUS_ASYNC_UTILS',
        ),
        workdir: meson.current_source_dir(),
    )
endforeach