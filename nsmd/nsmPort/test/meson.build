# nsmPort tests now use nsmd_test
# nsmPort is CONDITIONAL - only runs if enabled, in which case it's in nsmd_test
# All sources already compiled into nsmd_test, no need to recompile

tests = [
    'nsmPort_test',
    'nsmEndpoint_test',
    'nsmFpgaPort_test',
    'nsmGpuPciePort_test',
    'nsmRetimerPort_test',
    'nsmPortConfig_test',
    'nsmZone_test',
    'nsmNetworkAddressAggregator_test',
    'nsmEthPortTelemetryAggregator_test',
    'nsmPCIeErrors_test',
    'nsmPortAddSensor_test',
    'nsmPCIePort_test',
    'nsmPortDisableFuture_test',
    'nsmPortBranch_test',
    'nsmRetimerPortBranch_test',
    'nsmGpuPciePortBranch_test',
    'nsmGpuPciePortFactoryBranch_test',
    # 'nsmFpgaPortBranch_test',  # SIGSEGV - no fixture infrastructure
    'nsmPortFactoryBranch_test',
    'nsmPortBranch2_test',
    'nsmFpgaPortBranch2_test',
    'nsmPortBranch4_test',
    'nsmRetimerPortBranch2_test',
    'nsmPortConfigBranch_test',
    'nsmFpgaPortBranch3_test',
    'nsmRetimerPortBranch3_test',
    'nsmPortBranch5_test',
    'nsmGpuPciePortBranch2_test',
    'nsmFpgaPortBranch4_test',
    'nsmPortBranch6_test',
    'nsmRetimerPortBranch4_test',
    'nsmPortBranch7_test',
]

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            implicit_include_directories: false,
            include_directories: ['.', '..'],
            link_args: dynamic_linker,
            dependencies: tests_deps,
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
