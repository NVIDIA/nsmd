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
    '../../nsmEvent.cpp',
    '../../socket_handler.cpp',
    '../../nsmProgressCounters/progressCounters.cpp',
    '../../nsmProgressCounters/counterProducer.cpp',
    '../../nsmEvent/nsmLongRunningEventHandler.cpp',
    '../../nsmChassis/nsmPowerControl.cpp',
    '../../nsmChassis/nsmChassis.cpp',
    '../../nsmChassis/nsmClockOutputEnableState.cpp',
    '../../nsmChassis/nsmPowerSupplyStatus.cpp',
    '../../nsmChassis/nsmGpuPresenceAndPowerStatus.cpp',
    '../../nsmChassis/nsmPCIeFunction.cpp',
    '../../nsmChassis/nsmPCIeLTSSMState.cpp',
    '../../nsmChassis/nsmWriteProtectedJumper.cpp',
    '../../nsmChassis/nsmAERError.cpp',
    '../../nsmCommon/nsmCommon.cpp',
    '../../nsmCommon/nsmPciePortIntf.cpp',
    '../../nsmCommon/nsmPcieGroup.cpp',
    '../../nsmMemory/nsmMemory.cpp',
    '../../nsmPort/nsmPortDisableFuture.cpp',
    '../../nsmSensors/nsmInventoryProperty.cpp',
    '../../nsmSensors/nsmPCIeLinkSpeed.cpp',
    '../../nsmDbusIfaceOverride/nsmApSkuIdIntf.cpp',
    '../../nsmDbusIfaceOverride/nsmAssetIntf.cpp',
    '../../nsmDbusIfaceOverride/nsmMNNVLinkTopologyIntf.cpp',
    '../../nsmErrorInjection/nsmErrorInjection.cpp',
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
    '../../../libnsm/powersmoothing-powerprofile-api-v2.c',
    '../../../libnsm/diagnostics.c',
    '../../../libnsm/network-ports.c',
    '../../../libnsm/device-configuration.c',
    '../../../libnsm/network-ports.c',
    '../../../libnsm/pci-links.c',
    '../../../libnsm/requester/mctp.c',
    '../../../requester/request_timeout_tracker.cpp',
    '../../nsmSetAsync/nsmSetEgmMode.cpp',
    '../../nsmFwSwInventory/GPUSWInventory.cpp',
    '../../nsmDbusIfaceOverride/nsmAssetIntf.cpp',
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
    '../../nsmSensors',
    '../../nsmChassis',
    '../../nsmCommon',
    '../../nsmPort',
    '../../nsmDumpCollection',
    '../../nsmDbusIfaceOverride',
    '../../nsmSetAsync',
    '../../nsmLongRunning',
    '../../nsmRawCommand',
    '../../nsmProgressCounters',
    '../../../',
    '../../../common',
    '../../../libnsm',
]

test_src = declare_dependency(
    sources: dep_src_files,
    include_directories: dep_src_headers,
)

tests = ['nsmProcessor_test']

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
