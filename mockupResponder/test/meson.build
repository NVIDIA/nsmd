dep_src_files = [
    '../mockupResponder.cpp',
    '../../common/utils.cpp',
    '../../common/test/mockDBusHandler.cpp',
    '../../libnsm/base.c',
    '../../libnsm/network-ports.c',
    '../../libnsm/device-capability-discovery.c',
    '../../libnsm/device-configuration.c',
    '../../libnsm/platform-environmental.c',
    '../../libnsm/pci-links.c',
    '../../libnsm/requester/mctp.c',
]

dep_src_headers = [
    '.',
    '..',
    '../../common',
    '../../libnsm',
]

test_src = declare_dependency(
    sources: dep_src_files,
    include_directories: dep_src_headers,
)

tests = [
    'mockupResponder_test',
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
                gtest,
                gmock,
            ],
        ),
        workdir: meson.current_source_dir(),
    )
endforeach