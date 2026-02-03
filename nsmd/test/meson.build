tests = [
    'circularQueue_test',
    'eventHandler_test',
    'eventTypeHandlers_test',
    'instance_id_test',
    'interfaceWrapper_test',
    'nsmDevice_test',
    'nsmObject_test',
    'nsmObjectFactory_test',
    'stateChangeLogger_test',
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
            build_rpath: '',
            dependencies: tests_deps,
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
