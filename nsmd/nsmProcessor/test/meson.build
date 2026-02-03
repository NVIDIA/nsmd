# nsmProcessor tests now use nsmd_test
# nsmProcessor is CONDITIONAL - only runs if enabled, in which case it's in nsmd_test
# All sources already compiled into nsmd_test, no need to recompile

tests = [
    'nsmProcessor_test',
    'nsmPowerLimit_test',
    'nsmWorkloadPowerProfile_test',
    'nsmFpgaProcessor_test',
    'nsmReset_test',
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
