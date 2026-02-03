# nsmCommon tests now use nsmd_test
# nsmCommon subdir is ALWAYS compiled into nsmd_test (core functionality)
# No additional sources needed - everything is already in nsmd_test

tests = ['nsmCommon_test', 'nsmPcieGroup_test']

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
