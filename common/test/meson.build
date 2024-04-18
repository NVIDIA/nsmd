dep_src_files = ['../utils.cpp']

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

tests = ['common_utils_test']

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            implicit_include_directories: false,
            link_args: dynamic_linker,
            build_rpath: '',
            dependencies: [test_src, sdbusplus, phosphor_logging, gtest, gmock],
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
