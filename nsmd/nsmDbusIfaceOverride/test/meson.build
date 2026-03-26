tests = [
    'nsmDbusIfaceOverride_test',
    'nsmResetIface_test',
    'nsmWorkloadPowerProfileInfoIface_test',
    'nsmResetEdppAsyncIface_test',
    'nsmPowerSmoothing_v2_test',
    'nsmClearPowerCapIface_test',
    'nsmClearPowerCapBranch_test',
    'nsmActivateErrorInjectAsyncIntf_test',
    'nsmPowerSmoothingFeatureIntf_test',
    'nsmPowerSmoothingAdminProfileIntf_test',
    'nsmLogDumpOnDemand_test',
    'nsmPowerSmoothingV2Branch_test',
    'nsmPowerSmoothingV2Branch2_test',
    'nsmDbusIfaceOverrideBranch_test',
    'nsmPowerSmoothingAdminProfileV2Branch_test',
    'nsmPowerSmoothingFeatureV2Branch_test',
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
