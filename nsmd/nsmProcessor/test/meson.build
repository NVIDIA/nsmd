# nsmProcessor tests now use nsmd_test
# nsmProcessor is CONDITIONAL - only runs if enabled, in which case it's in nsmd_test
# All sources already compiled into nsmd_test, no need to recompile

tests = [
    'nsmProcessor_test',
    'nsmWorkloadPowerProfile_test',
    'nsmWorkloadPowerProfileBranch_test',
    'nsmFpgaProcessor_test',
    'nsmReset_test',
    'nsmOemResetStatistics_test',
    'nsmReconfigPermissions_test',
    'nsmGpuOperationalStatus_test',
    'nsmPowerLimit_test',
    'nsmPowerLimitBranch_test',
    'nsmProcessorBranch2_test',
    'nsmProcessorBranch3_test',
    'nsmPowerSmoothingBranch2_test',
    'nsmWorkloadPowerProfileBranch2_test',
    'nsmReconfigPermissionsBranch_test',
    'nsmProcessorFactoryBranch2_test',
    'nsmReconfigPermissionsBranch2_test',
    'nsmFpgaProcessorBranch_test',
    'nsmProcessorFactoryBranch3_test',
    'nsmProcessorFactoryBranch4_test',
    'nsmPowerSmoothingBranch3_test',
    'nsmPowerLimitBranch2_test',
    'nsmProcessorFactoryBranch6_test',
    'nsmPowerSmoothingBranch4_test',
    'nsmProcessorFactoryBranch7_test',
    'nsmProcessorFactoryBranch8_test',
    'nsmReconfigPermissionsBranch3_test',
    'nsmPowerLimitBranch3_test',
    'nsmPowerSmoothingBranch5_test',
    'nsmWorkloadPowerProfileBranch3_test',
    'nsmOemResetStatisticsBranch_test',
    'nsmFpgaProcessorBranch2_test',
    'nsmProcessorFactoryBranch9_test',
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
