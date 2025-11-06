dep_src_files = [
    '../nsmChassis.cpp',
    '../nsmChassisAssembly.cpp',
    '../nsmChassisPCIeDevice.cpp',
    '../nsmChassisPCIeSlot.cpp',
    '../nsmClockOutputEnableState.cpp',
    '../nsmPowerSupplyStatus.cpp',
    '../nsmGpuPresenceAndPowerStatus.cpp',
    '../nsmPCIeFunction.cpp',
    '../nsmPCIeLTSSMState.cpp',
    '../nsmWriteProtectedJumper.cpp',
    '../nsmAERError.cpp',
    '../../nsmPort/nsmRetimerPort.cpp',
    '../../nsmSensorAggregator.cpp',
    '../../nsmCommon/nsmCommon.cpp',
    '../../nsmLongRunning/nsmLongRunningSensor.cpp',
    '../../nsmSensors/nsmInventoryProperty.cpp',
    '../../nsmSensors/nsmPCIeLinkSpeed.cpp',
    '../../nsmDbusIfaceOverride/nsmAssetIntf.cpp',
    '../../nsmDumpCollection/nsmDebugInfo.cpp',
    '../../nsmDevice.cpp',
    '../../nsmSensor.cpp',
    '../../nsmObjectFactory.cpp',
    '../../sensorManager.cpp',
    '../../nsmEvent.cpp',
    '../../socket_handler.cpp',
    '../../nsmProgressCounters/progressCounters.cpp',
    '../../nsmProgressCounters/counterProducer.cpp',
    '../../nsmEvent/nsmLongRunningEventHandler.cpp',
    '../../nsmEvent/nsmLongRunningEvent.cpp',
    '../../nsmSetAsync/asyncOperationManager.cpp',
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
    '../../nsmFwSwInventory/GPUSWInventory.cpp',
    '../../nsmErrorInjection/nsmErrorInjection.cpp',
    '../../nsmSetAsync/nsmSetErrorInjection.cpp',
    '../../nsmDbusIfaceOverride/nsmAssetIntf.cpp',
    '../../nsmProcessor/nsmOemResetStatistics.cpp',
    '../../../requester/mctp_endpoint_discovery.cpp',
]

if get_option('shmem').enabled()
    dep_src_files += '../../nsmCommon/sharedMemCommon.cpp'
endif

dep_src_headers = [
    '.',
    '..',
    '../../',
    '../../nsmEvent',
    '../../nsmCommon',
    '../../nsmSensors',
    '../../nsmPort',
    '../../nsmSetAsync',
    '../../nsmLongRunning',
    '../../nsmDbusIfaceOverride',
    '../../nsmDumpCollection',
    '../../nsmSetAsync',
    '../../nsmRawCommand',
    '../../nsmProcessor',
    '../../nsmProgressCounters',
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
    gtest,
    gmock,
]

if get_option('shmem').enabled()
    tests_deps += dependency('nvidia-tal', required: true)
endif

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
