# nsmMemory tests now use nsmd_test
# nsmMemory is CONDITIONAL - only runs if enabled, in which case it's in nsmd_test
# All sources already compiled into nsmd_test, no need to recompile

tests = [
    'nsmMemory_test',
    'nsmMemory_extended_test',
    'nsmMemoryBranch_test',
    'nsmMemoryFactoryBranch_test',
    'nsmMemoryBranch2_test',
    'nsmMemoryBranch3_test',
    'nsmMemoryBranch4_test',
    'nsmMemoryBranch5_test',
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
