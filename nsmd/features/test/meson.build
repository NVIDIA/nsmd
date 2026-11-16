tests = [
    'feature_test',
    'paramTemplates_test',
    'overrideApply_test',
    'overrideDecode_test',
    'policyStore_test',
    'featureDriver_test',
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
