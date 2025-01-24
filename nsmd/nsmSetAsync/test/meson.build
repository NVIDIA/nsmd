dep_src_files = [
    '../asyncOperationManager.cpp',
    '../nsmAsyncSensor.cpp',
    '../nsmSetWriteProtected.cpp',
    '../../nsmDevice.cpp',
    '../../nsmSensor.cpp',
    '../../sensorManager.cpp',
    '../../deviceManager.cpp',
    '../../nsmObjectFactory.cpp',
    '../../nsmEvent.cpp',
    '../../deviceManager.cpp',
    '../../nsmEvent/nsmLongRunningEventHandler.cpp',
    '../../nsmEvent/nsmLongRunningEvent.cpp',
    '../../nsmLongRunning/nsmLongRunningSensor.cpp',
    '../../nsmLongRunning/nsmAsyncLongRunningSensor.cpp',
    '../../../common/utils.cpp',
    '../../../common/test/mockDBusHandler.cpp',
    '../../../libnsm/base.c',
    '../../../libnsm/instance-id.c',
    '../../../libnsm/device-configuration.c',
    '../../../libnsm/platform-environmental.c',
    '../../../libnsm/diagnostics.c',
    '../../../libnsm/requester/mctp.c',
    '../../../requester/request_timeout_tracker.cpp',
    '../nsmSetEgmMode.cpp',
]

dep_src_headers = [
    '..',
    '../../',
    '../../nsmEvent',
    '../../nsmLongRunning/',
    '../../../',
    '../../../common',
    '../../../libnsm',
]

test_src = declare_dependency(sources: dep_src_files, include_directories: dep_src_headers)

tests = [
    'asyncOperationManager_test',
    'nsmSetWriteProtected_test',
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
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
