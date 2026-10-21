# nsmFpgaTelemetry sources are compiled into nsmd_base when the subdir is enabled.

tests = [
    'nsmFpgaDiscoveryCheck_test',
    'nsmFpgaController_test',
    'nsmFpgaSensors_test',
]

# Test tables name only the fields a row needs
foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            cpp_args: ['-Wno-missing-field-initializers'],
            implicit_include_directories: false,
            include_directories: ['.', '..'],
            link_args: dynamic_linker,
            dependencies: tests_deps,
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
