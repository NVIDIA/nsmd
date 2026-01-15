# SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# nsmDot async handler integration tests
# These tests require extensive dependencies because they test full async handler flow
# including D-Bus operations, MCTP communication, and nsmd internal state management

dep_src_files = [
    '../nsmDot.cpp',
    '../../../dot/nsmDotUtils.cpp',
    '../../nsmDevice.cpp',
    '../../nsmSensor.cpp',
    '../../sensorManager.cpp',
    '../../nsmObjectFactory.cpp',
    '../../nsmEvent.cpp',
    '../../nsmProgressCounters/progressCounters.cpp',
    '../../nsmProgressCounters/counterProducer.cpp',
    '../../nsmEvent/nsmLongRunningEventHandler.cpp',
    '../../nsmEvent/nsmLongRunningEvent.cpp',
    '../../nsmSetAsync/asyncOperationManager.cpp',
    '../../nsmLongRunning/nsmLongRunningSensor.cpp',
    '../../../common/utils.cpp',
    '../../../common/test/mockDBusHandler.cpp',
    '../../../libnsm/base.c',
    '../../../libnsm/firmware-utils.c',
    '../../../libnsm/device-configuration.c',
    '../../../libnsm/platform-environmental.c',
    '../../../libnsm/diagnostics.c',
    '../../../libnsm/requester/mctp.c',
    '../../../requester/request_timeout_tracker.cpp',
    '../../nsmFwSwInventory/GPUSWInventory.cpp',
    '../../nsmDbusIfaceOverride/nsmAssetIntf.cpp',
    '../../../requester/mctp_endpoint_discovery.cpp',
    '../../../requester/mctp_endpoint_prober.cpp',
]

# Source files for DOT Blob tests
dep_src_files_dotblob = [
    '../nsmDotBlob.cpp',
    '../../../common/utils.cpp',
    '../../../common/test/mockDBusHandler.cpp',
]

if get_option('shmem').enabled()
    dep_src_files += '../../nsmCommon/sharedMemCommon.cpp'
endif

dep_src_headers = [
    '..',
    '../../',
    '../../nsmEvent',
    '../../nsmSetAsync/',
    '../../nsmLongRunning/',
    '../../nsmDbusIfaceOverride',
    '../../nsmProgressCounters',
    '../../../',
    '../../../common',
    '../../../libnsm',
    '../../../dot',
]

test_src = declare_dependency(
    sources: dep_src_files,
    include_directories: dep_src_headers,
)

test_src_dotblob = declare_dependency(
    sources: dep_src_files_dotblob,
    include_directories: dep_src_headers,
)

tests = ['nsmDot_test']
tests_dotblob = ['nsmDotBlob_test']

openssl_dep = dependency('openssl', required: true)

tests_deps = [
    test_src,
    CLI11_dep,
    nlohmann_json,
    sdbusplus,
    sdeventplus,
    phosphor_logging,
    gtest,
    gmock,
    openssl_dep,
]

if get_option('shmem').enabled()
    tests_deps += nvidia_shmem
endif

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            implicit_include_directories: false,
            link_args: dynamic_linker,
            build_rpath: '',
            dependencies: tests_deps,
            cpp_args: '-DMOCK_DBUS_ASYNC_UTILS',
        ),
        workdir: meson.current_source_dir(),
    )
endforeach

# DOT Blob tests (simpler dependencies, no async/device management)
tests_deps_dotblob = [
    test_src_dotblob,
    nlohmann_json,
    sdbusplus,
    sdeventplus,
    phosphor_logging,
    gtest,
    gmock,
]

foreach t : tests_dotblob
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            implicit_include_directories: false,
            link_args: dynamic_linker,
            build_rpath: '',
            dependencies: tests_deps_dotblob,
            cpp_args: '-DMOCK_DBUS_ASYNC_UTILS',
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
