# nsmtool tests now use nsmd_test
# Only nsmtool-specific sources need to be compiled here

nsmtool_test_sources = [
    '../cmd_helper.cpp',
    '../nsm_config_cmd.cpp',
    '../nsm_diag_cmd.cpp',
    '../nsm_discovery_cmd.cpp',
    '../nsm_telemetry_cmd.cpp',
    '../../libnsm/requester/mctp.c',
]

tests = ['cmd_helper_test', 'nsmtool_branch_test']

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

# parseResponseMsg tests — use #include trick to access anonymous-namespace
# `commands` vector.  The .cpp under test must NOT appear separately in
# sources (it is already part of the TU via the #include in the test file).
parse_test_common_sources = [
    '../cmd_helper.cpp',
    '../../libnsm/requester/mctp.c',
]

foreach t : [
    'nsm_config_cmd_parse_test',
    'nsm_config_cmd_branch_test',
    'nsm_diag_cmd_parse_test',
    'nsm_discovery_cmd_parse_test',
    'nsm_discovery_cmd_branch_test',
    'nsm_telemetry_cmd_parse_test',
    'nsm_telemetry_cmd_branch_test',
    'nsm_telemetry_cmd_branch2_test',
    'nsm_firmware_cmd_parse_test',
    'nsm_firmware_cmd_branch_test',
    'nsm_firmware_cmd_branch2_test',
    'nsm_telemetry_cmd_branch3_test',
    'nsm_diag_cmd_branch2_test',
    'nsm_config_cmd_branch2_test',
]
    test(
        t,
        executable(
            t.underscorify(),
            [t + '.cpp'] + parse_test_common_sources,
            implicit_include_directories: false,
            include_directories: ['.', '..'],
            link_args: dynamic_linker,
            dependencies: tests_deps,
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
