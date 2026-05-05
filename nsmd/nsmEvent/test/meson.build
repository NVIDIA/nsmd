# nsmEvent tests now use nsmd_test
# nsmEvent subdir is ALWAYS compiled into nsmd_test (core functionality)
# All sources already compiled into nsmd_test, no need to recompile

tests = [
    'nsmEvent_test',
    'nsmXIDEvent_test',
    'nsmGPIOStateChangeEvent_test',
    'nsmEvent_dump_test',
    'nsmLongRunningEvent_test',
    'nsmLongRunningEventHandler_test',
    'nsmFabricManagerStateEvent_test',
    'nsmThresholdEvent_test',
    'nsmResetRequiredEvent_test',
    'nsmRediscoveryEvent_test',
    'nsmRuntimeISTCompleteEvent_test',
    'nsmEventInfoBranch_test',
    'nsmEventBranch_test',
    'nsmRediscoveryEventBranch_test',
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
