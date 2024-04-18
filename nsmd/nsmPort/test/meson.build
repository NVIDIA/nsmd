dep_src_files = [
    '../nsmPort.cpp',
    '../../nsmDevice.cpp',
    '../../nsmObjectFactory.cpp',
    '../../sensorManager.cpp',
    '../../nsmSensor.cpp',
    '../../../common/utils.cpp',
    '../../../libnsm/base.c',
    '../../../libnsm/network-ports.c',
    '../../../libnsm/instance-id.c',
    '../../../libnsm/requester/mctp.c',
]

dep_src_headers = [
    '.',
    '..',
    '../../',
    '../../../',
    '../../../common',
    '../../../libnsm',
]

test_src = declare_dependency(
    sources: dep_src_files,
    include_directories: dep_src_headers,
)

tests = [
    'nsmPort_test',
]

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
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
endforeach
