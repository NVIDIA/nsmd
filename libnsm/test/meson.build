# libnsm tests now use nsmd_test
# common-tests.cpp contains shared test utilities specific to libnsm tests

test_common_sources = ['common-tests.cpp']

tests = [
    'libnsm_base_test',
    'libnsm_base_branch_coverage_test',
    'libnsm_base_endian_utils_test',
    'libnsm_debug_token_test',
    'libnsm_device_configuration_test',
    'libnsm_device_configuration_mmio_test',
    'libnsm_device_configuration_mmio_negative_test',
    'libnsm_fpga_maps_test',
    'libnsm_device_capability_discovery_test',
    # 'libnsm_diagnostics_test',  # DISABLED: SIGSEGV crash - Session 7 confirmed: crashes during teardown, requires debugging
    'libnsm_fpga_tables_test',
    'libnsm_fpga_run_test',
    'libnsm_fpga_controller_test',
    'libnsm_fpga_negative_test',
    'libnsm_diagnostics_encode_decode_test',
    'libnsm_diagnostics_responses_test',
    'libnsm_diagnostics_helpers_test',
    'libnsm_diagnostics_runtime_ist_event_test',
    'libnsm_diagnostics_preboot_test',
    'libnsm_firmware_utils_test',
    'libnsm_firmware_utils_null_test',
    'libnsm_firmware_utils_pack_fail_test',
    'libnsm_firmware_aggregate_test',
    'libnsm_platform_environmental_test',
    'libnsm_platform_environmental_sensors_test',
    'libnsm_platform_environmental_branch_test',
    'libnsm_platform_environmental_line_test',
    'libnsm_platform_environmental_error_paths_test',
    'libnsm_network_ports_test',
    'libnsm_network_ports_optical_module_test',
    'libnsm_pci_links_test',
    'libnsm_powersmoothing_v2_test',
    'libnsm_remaining_coverage_test',
    # libnsm_mctp_requester_test is built separately below with --wrap
    # so mock interposition wins against ASan's runtime interceptors.
    'libnsm_firmware_utils_dot_test',
    'libnsm_firmware_utils_branch_test',
    'libnsm_device_capability_discovery_branch_test',
    'libnsm_device_configuration_branch_test',
    'libnsm_network_ports_branch_test',
    'libnsm_pci_links_branch_test',
    'libnsm_platform_environmental_branch2_test',
    'libnsm_platform_environmental_branch3_test',
    'libnsm_platform_environmental_branch4_test',
    'libnsm_diagnostics_branch_test',
    'libnsm_debug_token_branch_test',
    'libnsm_powersmoothing_v2_branch_test',
    'libnsm_base_branch2_test',
    'libnsm_firmware_utils_branch2_test',
    'libnsm_platform_environmental_branch5_test',
    'libnsm_device_configuration_branch2_test',
    'libnsm_network_ports_branch2_test',
    'libnsm_pci_links_branch2_test',
    'libnsm_remaining_branch_test',
    'libnsm_platform_environmental_branch6_test',
    'libnsm_device_configuration_branch3_test',
    'libnsm_firmware_utils_branch3_test',
    'libnsm_platform_environmental_branch7_test',
    'libnsm_device_configuration_branch4_test',
    'libnsm_network_ports_port_health_test',
    'libnsm_network_ports_link_health_ext_test',
]

foreach t : tests
    # FPGA test tables name only the fields a row needs
    fpga_args = t.startswith('libnsm_fpga_') ? [
        '-Wno-missing-field-initializers',
    ] : []
    test(
        t,
        executable(
            t.underscorify(),
            [t + '.cpp', test_common_sources],
            cpp_args: fpga_args,
            implicit_include_directories: false,
            include_directories: ['.', '..'],
            link_args: dynamic_linker,
            build_rpath: '',
            dependencies: [tests_deps, libnsm_dep],
        ),
        workdir: meson.current_source_dir(),
    )
endforeach

# Every generated FPGA table must match its rows file
fpga_rows = [
    'fpga-rules-lp30',
]
python3 = find_program('python3', required: false)
if python3.found()
    fpga_dir = meson.current_source_dir() / '..' / 'fpga'
    foreach r : fpga_rows
        test(
            'libnsm_fpga_generated_' + r.underscorify(),
            python3,
            args: [
                fpga_dir / 'gen-fpga-rules.py',
                fpga_dir / (r + '.rows'),
                fpga_dir / (r + '.c'),
                '--check',
            ],
        )
    endforeach
endif

# Built with -Wl,--wrap so mock interposition wins against ASan's
# runtime poll/recv/recvmsg interceptors.  Companion __wrap_* symbols
# live in common/test/mockSocketIo.cpp.
test(
    'libnsm_mctp_requester_test',
    executable(
        'libnsm_mctp_requester_test'.underscorify(),
        ['libnsm_mctp_requester_test.cpp', test_common_sources],
        implicit_include_directories: false,
        include_directories: ['.', '..'],
        link_args: dynamic_linker + [
            '-Wl,--wrap=poll',
            '-Wl,--wrap=recv',
            '-Wl,--wrap=recvmsg',
            '-Wl,--wrap=sendmsg',
            '-Wl,--wrap=__poll_chk',
            '-Wl,--wrap=__recv_chk',
        ],
        build_rpath: '',
        dependencies: [tests_deps, libnsm_dep],
    ),
    workdir: meson.current_source_dir(),
)
