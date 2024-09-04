dep_src_files = [
    '../nsmGpmOem.cpp',
    '../../nsmDevice.cpp',
    '../../nsmObjectFactory.cpp',
    '../../nsmSensorAggregator.cpp',
    '../../nsmSensor.cpp',
    '../../sensorManager.cpp',
    '../../../common/utils.cpp',
    '../../../common/test/mockDBusHandler.cpp',
    '../../../libnsm/base.c',
    '../../../libnsm/platform-environmental.c',
    '../../../libnsm/requester/mctp.c',
    '../../../libnsm/instance-id.c',
    '../../../requester/request_timeout_tracker.cpp',
]

dep_src_headers = [
    '.',
    '..',
    '../../',
    '../../test/',
    '../../../',
    '../../../common',
    '../../../libnsm',
]

test_src = declare_dependency(sources: dep_src_files, include_directories: dep_src_headers)

tests_files = [
    'nsmGpmOem_test.cpp',
]

test(
    'nsmGpmOem_test',
    executable(
        'nsmGpmOem_test',
        tests_files,
        implicit_include_directories: false,
        link_args: dynamic_linker,
        build_rpath: '',
        dependencies: [
            test_src,
            CLI11_dep,
            nlohmann_json,
            sdbusplus,
            sdeventplus,
            phosphor_logging,
            nvidia_shmem,
            gtest,
            gmock,
        ],
    ),
    workdir: meson.current_source_dir(),
)