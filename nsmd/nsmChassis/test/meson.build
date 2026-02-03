# nsmChassis tests now use nsmd_test
# nsmChassis is CONDITIONAL - only runs if enabled, in which case it's in nsmd_test
# All sources already compiled into nsmd_test, no need to recompile

tests = [
    'nsmChassis_test',
    'nsmChassisAssembly_test',
    'nsmChassisPCIeDevice_test',
    'nsmChassisPCIeSlot_test',
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
