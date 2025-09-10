dep_src_files = ['../tlv_decoder.cpp', '../tlv_encoder.cpp']

dep_src_headers = ['../..']

test_src = declare_dependency(
    sources: dep_src_files,
    include_directories: dep_src_headers,
)

tests = ['debugTokenTLV_test']

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            implicit_include_directories: false,
            link_args: dynamic_linker,
            build_rpath: '',
            dependencies: [test_src, CLI11_dep, gtest, gmock],
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
