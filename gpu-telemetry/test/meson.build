# Test configuration for GPU telemetry

# Test dependencies
gtest_dep = dependency('gtest', required: true)
gmock_dep = dependency('gmock', required: true)
thread_dep = dependency('threads', required: true)

# Common test dependencies
test_deps = [
    gtest_dep,
    gmock_dep,
    thread_dep,
    gpu_telemetry_dep,
    stdexec_dep,
]

# Unit tests
unit_tests = [
    'client_test',
    'server_test',
    'mock_device_test',
]

foreach test_name : unit_tests
    test_exe = executable(test_name,
        test_name + '.cpp',
        dependencies: test_deps,
        cpp_args: ['-DUNIT_TESTING'],
    )
    
    test(test_name, test_exe,
        protocol: 'gtest',
        timeout: 30,
    )
endforeach

# Integration test
integration_test = executable('integration_test',
    'integration_test.cpp',
    dependencies: test_deps,
)

test('integration_test', integration_test,
    protocol: 'gtest',
    timeout: 60,
    is_parallel: false,
)

# Coverage configuration
if get_option('b_coverage')
    lcov = find_program('lcov')
    genhtml = find_program('genhtml')
    
    coverage_dir = meson.current_build_dir() / 'coverage'
    
    run_target('coverage',
        command: [
            'sh', '-c',
            '''
            rm -rf @0@
            @1@ --directory @2@ --capture --output-file @3@
            @4@ --output-directory @0@ @3@
            '''.format(
                coverage_dir,
                lcov.path(),
                meson.build_root(),
                coverage_dir / 'coverage.info',
                genhtml.path(),
            )
        ],
        depends: [unit_tests, integration_test],
    )
endif
