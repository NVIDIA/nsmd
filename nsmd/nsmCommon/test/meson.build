dep_src_files = [
    '../nsmCommon.cpp',
    '../nsmPcieGroup.cpp',
    '../sharedMemCommon.cpp',
    '../../nsmDevice.cpp',
    '../../nsmObjectFactory.cpp',
    '../../sensorManager.cpp',
    '../../nsmSensor.cpp',
    '../../nsmEvent.cpp',
    '../../nsmProgressCounters/progressCounters.cpp',
    '../../nsmProgressCounters/counterProducer.cpp',
    '../../nsmEvent/nsmLongRunningEventHandler.cpp',
    '../../nsmEvent/nsmLongRunningEvent.cpp',
    '../../nsmSensors/nsmInventoryProperty.cpp',
    '../../nsmLongRunning/nsmLongRunningSensor.cpp',
    '../../../common/utils.cpp',
    '../../../common/test/mockDBusHandler.cpp',
    '../../../libnsm/base.c',
    '../../../libnsm/platform-environmental.c',
    '../../../libnsm/pci-links.c',
    '../../../libnsm/requester/mctp.c',
    '../../../requester/request_timeout_tracker.cpp',
    '../../nsmFwSwInventory/GPUSWInventory.cpp',
    '../../nsmDbusIfaceOverride/nsmAssetIntf.cpp',
    '../../../requester/mctp_endpoint_discovery.cpp',
    '../../../requester/mctp_endpoint_prober.cpp',
]

dep_src_headers = [
    '.',
    '..',
    '../../',
    '../../nsmEvent',
    '../../nsmSensors',
    '../../nsmRawCommand',
    '../../nsmLongRunning',
    '../../nsmSetAsync',
    '../../nsmDbusIfaceOverride',
    '../../nsmProgressCounters',
    '../../../',
    '../../../common',
    '../../../libnsm',
]

test_src = declare_dependency(
    sources: dep_src_files,
    include_directories: dep_src_headers,
)

tests = ['nsmCommon_test', 'nsmPcieGroup_test']

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
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
