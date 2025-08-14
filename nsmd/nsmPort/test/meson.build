dep_src_files = [
    '../nsmPort.cpp',
    '../nsmPCIePort.cpp',
    '../nsmPCIeErrors.cpp',
    '../../nsmDevice.cpp',
    '../../nsmSensorAggregator.cpp',
    '../../nsmSensor.cpp',
    '../../nsmSensorAggregator.cpp',
    '../../nsmObjectFactory.cpp',
    '../../sensorManager.cpp',
    '../../nsmEvent.cpp',
    '../../deviceManager.cpp',
    '../../socket_handler.cpp',
    '../../nsmEvent/nsmLongRunningEventHandler.cpp',
    '../../nsmHistograms/nsmHistogramInfo.cpp',
    '../../nsmSensors/nsmPCIeLinkSpeed.cpp',
    '../../../common/utils.cpp',
    '../../../common/test/mockDBusHandler.cpp',
    '../../../libnsm/base.c',
    '../../../libnsm/platform-environmental.c',
    '../../../libnsm/network-ports.c',
    '../../../libnsm/platform-environmental.c',
    '../../../libnsm/pci-links.c',
    '../../../libnsm/instance-id.c',
    '../../../libnsm/requester/mctp.c',
    '../../../requester/request_timeout_tracker.cpp',
    '../../nsmFwSwInventory/GPUSWInventory.cpp',
    '../../nsmDbusIfaceOverride/nsmAssetIntf.cpp',
    '../../nsmSensorAggregator.cpp',
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
    '../../../',
    '../../../common',
    '../../../libnsm',
    '../../nsmDbusIfaceOverride',
]

test_src = declare_dependency(
    sources: dep_src_files,
    include_directories: dep_src_headers,
)

tests = ['nsmPort_test']

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
