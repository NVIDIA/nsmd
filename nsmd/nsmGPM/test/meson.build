dep_src_files = [
    '../nsmGpmOem.cpp',
    '../../nsmDevice.cpp',
    '../../nsmObjectFactory.cpp',
    '../../nsmSensorAggregator.cpp',
    '../../nsmSensor.cpp',
    '../../sensorManager.cpp',
    '../../nsmEvent.cpp',
    '../../nsmEvent/nsmLongRunningEventHandler.cpp',
    '../../../common/utils.cpp',
    '../../../common/test/mockDBusHandler.cpp',
    '../../../libnsm/base.c',
    '../../../libnsm/platform-environmental.c',
    '../../../libnsm/requester/mctp.c',
    '../../../libnsm/instance-id.c',
    '../../../requester/request_timeout_tracker.cpp',
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
    '../../test/',
    '../../../',
    '../../../common',
    '../../../libnsm',
    '../../nsmDbusIfaceOverride',
]

test_src = declare_dependency(
    sources: dep_src_files,
    include_directories: dep_src_headers,
)

tests_files = ['nsmGpmOem_test.cpp']
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
test(
    'nsmGpmOem_test',
    executable(
        'nsmGpmOem_test',
        tests_files,
        implicit_include_directories: false,
        link_args: dynamic_linker,
        build_rpath: '',
        dependencies: tests_deps,
    ),
    workdir: meson.current_source_dir(),
)
