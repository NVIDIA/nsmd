dep_src_files = [
    '../nsmChassis.cpp',
    '../nsmChassisAssembly.cpp',
    '../nsmChassisPCIeDevice.cpp',
    '../nsmChassisPCIeSlot.cpp',
    '../nsmClockOutputEnableState.cpp',
    '../nsmPowerSupplyStatus.cpp',
    '../nsmGpuPresenceAndPowerStatus.cpp',
    '../nsmPCIeFunction.cpp',
    '../nsmPCIeLinkSpeed.cpp',
    '../nsmPCIeLTSSMState.cpp',
    '../nsmWriteProtectedJumper.cpp',
    '../../nsmCommon/sharedMemCommon.cpp',
    '../../nsmSensors/nsmInventoryProperty.cpp',
    '../../nsmDbusIfaceOverride/nsmWriteProtectedIntf.cpp',
    '../../nsmDevice.cpp',
    '../../nsmSensor.cpp',
    '../../nsmObjectFactory.cpp',
    '../../sensorManager.cpp',
    '../../deviceManager.cpp',
    '../../../common/utils.cpp',
    '../../../common/test/mockDBusHandler.cpp',
    '../../../libnsm/base.c',
    '../../../libnsm/platform-environmental.c',
    '../../../libnsm/pci-links.c',
    '../../../libnsm/device-configuration.c',
    '../../../libnsm/diagnostics.c',
    '../../../libnsm/instance-id.c',
    '../../../libnsm/requester/mctp.c',
    '../../../requester/request_timeout_tracker.cpp',
]

dep_src_headers = [
    '.',
    '..',
    '../../',
    '../../nsmCommon',
    '../../nsmSensors',
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
    'nsmChassis_test',
    'nsmChassisAssembly_test',
    'nsmChassisPCIeDevice_test',
    'nsmChassisPCIeSlot_test',
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
        ),
        workdir: meson.current_source_dir(),
    )
endforeach