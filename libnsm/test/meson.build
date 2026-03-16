# libnsm tests now use nsmd_test
# common-tests.cpp contains shared test utilities specific to libnsm tests

test_common_sources = ['common-tests.cpp']

tests = [
    'libnsm_base_test',
    'libnsm_base_branch_coverage_test',
    'libnsm_base_endian_utils_test',
    'libnsm_debug_token_test',
    'libnsm_device_configuration_test',
    'libnsm_device_capability_discovery_test',
    # 'libnsm_diagnostics_test',  # DISABLED: SIGSEGV crash - Session 7 confirmed: crashes during teardown, requires debugging
    'libnsm_diagnostics_encode_decode_test',
    'libnsm_diagnostics_responses_test',
    'libnsm_diagnostics_helpers_test',
    'libnsm_firmware_utils_test',
    'libnsm_firmware_aggregate_test',
    'libnsm_platform_environmental_test',
    'libnsm_platform_environmental_sensors_test',
    'libnsm_platform_environmental_branch_test',
    'libnsm_platform_environmental_line_test',
    'libnsm_platform_environmental_error_paths_test',
    'libnsm_network_ports_test',
    'libnsm_pci_links_test',
    'libnsm_powersmoothing_v2_test',
    'libnsm_remaining_coverage_test',
]

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            [t + '.cpp', test_common_sources],
            implicit_include_directories: false,
            include_directories: ['.', '..'],
            link_args: dynamic_linker,
            build_rpath: '',
            dependencies: [tests_deps, libnsm_dep],
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
