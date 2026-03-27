tests = [
    'debugTokenUtils_test',
    'nsmDebugToken_test',
    'nsmDebugTokenObjectFactory_test',
    'nsmDebugTokenNIC_test',
    'nsmDebugTokenNICBranch_test',
    'nsmDebugTokenUnified_test',
    'nsmDebugTokenUnifiedBranch_test',
    'nsmDebugTokenUnifiedBranch4_test',
    'nsmDebugTokenAggregation_test',
    'nsmDebugTokenAggregationBranch_test',
    'nsmDebugTokenNICBranch2_test',
    'nsmDebugTokenAggregationBranch2_test',
    'nsmDebugTokenNICBranch3_test',
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
