# Build configuration for GPU telemetry examples

# Temperature monitoring example
temperature_example = executable('temperature',
    'temperature.c',
    dependencies: [
        gpu_telemetry_dep,
        libnsm_dep,
    ],
    c_args: [
        '-Wall',
        '-Wextra',
        '-Werror',
    ],
    install: true,
    install_dir: get_option('datadir') / 'gpu-telemetry/examples'
)

# Install example source files for reference
install_data(
    'temperature.c',
    install_dir: get_option('datadir') / 'gpu-telemetry/examples'
)

# Generate example documentation
example_docs = custom_target('example-docs',
    input: 'temperature.c',
    output: 'temperature.html',
    command: [
        find_program('highlight'),
        '-i', '@INPUT@',
        '-o', '@OUTPUT@',
        '--syntax=c',
        '--format=html',
        '--inline-css',
        '--style=monokai',
    ],
    install: true,
    install_dir: get_option('datadir') / 'gpu-telemetry/examples'
)

# Example tests
if get_option('build_tests')
    temperature_test = executable('temperature_test',
        'temperature.c',
        c_args: [
            '-DUNIT_TESTING',
            '-Wall',
            '-Wextra',
            '-Werror',
        ],
        dependencies: [
            gpu_telemetry_dep,
            libnsm_dep,
            gtest_dep,
        ],
    )

    test('temperature_example',
        temperature_test,
        protocol: 'gtest',
        timeout: 30,
    )
endif
