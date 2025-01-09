dep_src_files = [
    '../nsmRawCommandHandler.cpp',
    '../../nsmDevice.cpp',
    '../../nsmSensor.cpp',
    '../../sensorManager.cpp',
    '../../deviceManager.cpp',
    '../../nsmObjectFactory.cpp',
    '../../nsmEvent.cpp',
    '../../deviceManager.cpp',
    '../../nsmEvent/nsmLongRunningEventDispatcher.cpp',
    '../../nsmEvent/nsmLongRunningEvent.cpp',
    '../../nsmSetAsync/asyncOperationManager.cpp',
    '../../nsmLongRunning/nsmLongRunningSensor.cpp',
    '../../../common/utils.cpp',
    '../../../common/test/mockDBusHandler.cpp',
    '../../../libnsm/base.c',
    '../../../libnsm/instance-id.c',
    '../../../libnsm/device-configuration.c',
    '../../../libnsm/platform-environmental.c',
    '../../../libnsm/diagnostics.c',
    '../../../libnsm/requester/mctp.c',
    '../../../requester/request_timeout_tracker.cpp',
]

dep_src_headers = [
    '..',
    '../../',
    '../../nsmEvent',
    '../../nsmSetAsync/',
    '../../nsmLongRunning/',
    '../../../',
    '../../../common',
    '../../../libnsm',
]

test_src = declare_dependency(sources: dep_src_files, include_directories: dep_src_headers)

tests = [
    'nsmRawCommandHandler_test',
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
