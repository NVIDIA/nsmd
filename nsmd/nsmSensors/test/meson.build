tests = [
    'nsmPCIeLinkSpeed_test',
    'nsmInventoryProperty_test',
    'nsmDeepCoverage_test',
    'nsmSensors_test',
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
