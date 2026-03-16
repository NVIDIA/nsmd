# nsmtool tests now use nsmd_test
# Only nsmtool-specific sources need to be compiled here

nsmtool_test_sources = [
    '../cmd_helper.cpp',
    '../nsm_discovery_cmd.cpp',
    '../nsm_telemetry_cmd.cpp',
    '../../libnsm/requester/mctp.c',
]

tests = ['cmd_helper_test']

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            [t + '.cpp', nsmtool_test_sources],
            implicit_include_directories: false,
            include_directories: ['.', '..'],
            link_args: dynamic_linker,
            dependencies: tests_deps,
        ),
        workdir: meson.current_source_dir(),
    )
endforeach

# parseResponseMsg / createRequestMsg tests — use #include trick to access
# anonymous-namespace `commands` vector. The .cpp under test is included
# directly by the test file, so only shared helpers go in sources here.
parse_test_common_sources = [
    '../cmd_helper.cpp',
    '../../libnsm/requester/mctp.c',
]

test(
    'nsm_diag_cmd_parse_test',
    executable(
        'nsm_diag_cmd_parse_test',
        ['nsm_diag_cmd_parse_test.cpp'] + parse_test_common_sources,
        implicit_include_directories: false,
        include_directories: ['.', '..'],
        link_args: dynamic_linker,
        dependencies: tests_deps,
    ),
    workdir: meson.current_source_dir(),
)
