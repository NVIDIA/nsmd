dep_src_files = [
    '../nsmMemory.cpp',
    '../../nsmDevice.cpp',
    '../../nsmCommon/nsmCommon.cpp',
    '../../nsmObjectFactory.cpp',
    '../../sensorManager.cpp',
    '../../deviceManager.cpp',
    '../../nsmSensor.cpp',
    '../../nsmEvent.cpp',
    '../../deviceManager.cpp',
    '../../nsmEvent/nsmLongRunningEventHandler.cpp',
    '../../nsmEvent/nsmLongRunningEvent.cpp',
    '../../nsmLongRunning/nsmLongRunningSensor.cpp',
    '../../../common/utils.cpp',
    '../../../common/test/mockDBusHandler.cpp',
    '../../../libnsm/base.c',
    '../../../libnsm/platform-environmental.c',
    '../../../libnsm/requester/mctp.c',
    '../../../libnsm/instance-id.c',
    '../../../requester/request_timeout_tracker.cpp',
    '../../nsmFwSwInventory/GPUSWInventory.cpp',
    '../../nsmDbusIfaceOverride/nsmAssetIntf.cpp',
]

if get_option('shmem').enabled()
    dep_src_files += '../../nsmCommon/sharedMemCommon.cpp'
endif

dep_src_headers = [
    '.',
    '..',
    '../../',
    '../../nsmEvent',
    '../../nsmLongRunning',
    '../../nsmSetAsync',
    '../../../',
    '../../../common',
    '../../../libnsm',
    '../../nsmDbusIfaceOverride',
]

test_src = declare_dependency(
    sources: dep_src_files,
    include_directories: dep_src_headers,
)

tests = ['nsmMemory_test']
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
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
