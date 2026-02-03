# libnsm tests now use nsmd_test
# common-tests.cpp contains shared test utilities specific to libnsm tests

test_common_sources = ['common-tests.cpp']

tests = [
    'libnsm_base_test',
    'libnsm_debug_token_test',
    'libnsm_device_configuration_test',
    'libnsm_device_capability_discovery_test',
    'libnsm_diagnostics_test',
    'libnsm_firmware_utils_test',
    'libnsm_platform_environmental_test',
    'libnsm_network_ports_test',
    'libnsm_pci_links_test',
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
            dependencies: tests_deps,
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
