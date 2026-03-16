# nsmSetAsync tests now use nsmd_test
# nsmSetAsync subdir is ALWAYS compiled into nsmd_test (core functionality)
# All sources already compiled into nsmd_test, no need to recompile

tests = [
    'nsmSetWriteProtected_test',
    'asyncOperationManager_test',
    'nsmSetECCMode_test',
    'nsmSetErrorInjection_test',
    'nsmSetMigMode_test',
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
