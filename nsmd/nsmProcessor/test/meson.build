dep_src_files = [
    '../nsmProcessor.cpp',
    '../nsmReconfigPermissions.cpp',
    '../nsmPowerSmoothing.cpp',
    '../nsmOemResetStatistics.cpp',
    '../nsmWorkloadPowerProfile.cpp',
    '../../nsmDevice.cpp',
    '../../nsmSensorAggregator.cpp',
    '../../nsmSensor.cpp',
    '../../nsmEvent.cpp',
    '../../nsmEvent/nsmLongRunningEvent.cpp',
    '../../nsmObjectFactory.cpp',
    '../../sensorManager.cpp',
    '../../deviceManager.cpp',
    '../../nsmEvent.cpp',
    '../../nsmEvent/nsmLongRunningEventHandler.cpp',
    '../../nsmChassis/nsmPowerControl.cpp',
    '../../nsmCommon/nsmCommon.cpp',
    '../../nsmCommon/sharedMemCommon.cpp',
    '../../nsmMemory/nsmMemory.cpp',
    '../../nsmPort/nsmPortDisableFuture.cpp',
    '../../nsmSensors/nsmInventoryProperty.cpp',
    '../../nsmSensors/nsmPCIeLinkSpeed.cpp',
    '../../nsmDbusIfaceOverride/nsmAssetIntf.cpp',
    '../../nsmDbusIfaceOverride/nsmMNNVLinkTopologyIntf.cpp',
    '../../nsmSensors/nsmErrorInjection.cpp',
    '../../nsmSetAsync/asyncOperationManager.cpp',
    '../../nsmSetAsync/nsmAsyncSensor.cpp',
    '../../nsmSetAsync/nsmSetMigMode.cpp',
    '../../nsmSetAsync/nsmSetECCMode.cpp',
    '../../nsmSetAsync/nsmSetCpuOperatingConfig.cpp',
    '../../nsmSetAsync/nsmSetErrorInjection.cpp',
    '../../nsmLongRunning/nsmLongRunningSensor.cpp',
    '../../nsmLongRunning/nsmAsyncLongRunningSensor.cpp',
    '../../nsmDumpCollection/nsmDebugInfo.cpp',
    '../../nsmDumpCollection/nsmEraseTrace.cpp',
    '../../nsmDumpCollection/nsmLogInfo.cpp',
    '../../../common/utils.cpp',
    '../../../common/test/mockDBusHandler.cpp',
    '../../../libnsm/base.c',
    '../../../libnsm/platform-environmental.c',
    '../../../libnsm/diagnostics.c',
    '../../../libnsm/network-ports.c',
    '../../../libnsm/device-configuration.c',
    '../../../libnsm/network-ports.c',
    '../../../libnsm/pci-links.c',
    '../../../libnsm/instance-id.c',
    '../../../libnsm/requester/mctp.c',
    '../../../requester/request_timeout_tracker.cpp',
    '../../nsmSetAsync/nsmSetEgmMode.cpp',
]

dep_src_headers = [
    '.',
    '..',
    '../../',
    '../../nsmEvent',
    '../../nsmSensors',
    '../../nsmCommon',
    '../../nsmPort',
    '../../nsmDumpCollection',
    '../../nsmDbusIfaceOverride',
    '../../nsmSetAsync',
    '../../nsmLongRunning',
    '../../nsmRawCommand',
    '../../../',
    '../../../common',
    '../../../libnsm',
]

test_src = declare_dependency(
    sources: dep_src_files,
    include_directories: dep_src_headers,
)

tests = [
    'nsmProcessor_test',
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
    tests_deps += nvidia_shmem
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
