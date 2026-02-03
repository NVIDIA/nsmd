# mockupResponder tests now use nsmd_test
# Only mockupResponder-specific sources need to be compiled here

mockup_test_sources = [
    '../debugToken.cpp',
    '../mockupResponder.cpp',
    '../firmwareUtils.cpp',
]

tests = ['mockupResponder_test']

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            [t + '.cpp', mockup_test_sources],
            implicit_include_directories: false,
            include_directories: ['.', '..'],
            link_args: dynamic_linker,
            build_rpath: '',
            dependencies: tests_deps,
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
