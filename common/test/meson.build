# common tests now use nsmd_test

tests = [
    'common_utils_test',
    'common_utils_coroutine_test',
    'common_branch_test',
    'common_utils_branch2_test',
    'common_branch2_test',
    'common_utils_branch4_test',
    'coroutineSemaphore_test',
    'timer_test',
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

test(
    'sleep_semaphore_mock_test',
    executable(
        'sleep_semaphore_mock_test',
        'sleep_semaphore_mock_test.cpp',
        'mockSdEvent.cpp',
        '../event.cpp',
        implicit_include_directories: false,
        include_directories: ['.', '..', '../..'],
        link_args: dynamic_linker + ['-Wl,--allow-multiple-definition'],
        build_rpath: '',
        # Use deps/gtest/gmock directly, NOT tests_deps, to avoid
        # compile_args (COVERAGE_DISABLE_COROUTINES) which replaces
        # co_await/co_return and breaks the real-coroutine D1 mock tests.
        # Also explicitly undefine COVERAGE_DISABLE_COROUTINES in case it is
        # set project-wide (e.g. via meson setup -Dcpp_args=), since this test
        # must use the real coroutine data() path, not the coverage mock path.
        cpp_args: ['-UCOVERAGE_DISABLE_COROUTINES'],
        dependencies: [deps, gtest, gmock],
    ),
    workdir: meson.current_source_dir(),
)

test(
    'dBusHandler_test',
    executable(
        'dBusHandler_test',
        '../dBusHandler.cpp',
        'dBusHandler_test.cpp',
        implicit_include_directories: false,
        link_with: [nsmd_base],
        include_directories: ['.', '..', '../../libnsm'],
        link_args: dynamic_linker,
        build_rpath: '',
        # Undefine COVERAGE_DISABLE_COROUTINES for this target: it tests the
        # production dBusHandler.cpp paths (await_ready=false, await_suspend
        # queues real D-Bus call) and must NOT use the mock coroutine stubs.
        cpp_args: ['-UCOVERAGE_DISABLE_COROUTINES'],
        dependencies: [deps, gtest, gmock],
    ),
    workdir: meson.current_source_dir(),
)
