# nsmGPM tests now use nsmd_test
# nsmGPM is CONDITIONAL - only runs if enabled, in which case it's in nsmd_test
# All sources already compiled into nsmd_test, no need to recompile

tests = [
    'nsmGpmOem_test',
    'nsmGpmPerInstance_test',
    'nsmGpmOemFactory_test',
    'nsmGpmOemBranch_test',
    'nsmGpmOemBranch2_test',
    'nsmGpmOemBranch3_test',
    'nsmGpmOemFactoryBranch_test',
    'nsmGpmOemBranch4_test',
    'nsmGpmOemBranch5_test',
    'nsmGpmOemBranch6_test',
    'nsmGPMBranch_test',
    'nsmGpmOemBranch7_test',
    'nsmGpmOemBranch8_test',
    'nsmGpmOemBranch9_test',
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
