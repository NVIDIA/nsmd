sources = [
    '../common/event.cpp',
    '../common/utils.cpp',
    '../debug-token/tlv_decoder.cpp',
    '../debug-token/tlv_encoder.cpp',
    '../dot/nsmDotUtils.cpp',
    '../dot/dotErrorHandler.cpp',
    '../libnsm/base.c',
    '../requester/mctp_endpoint_prober.cpp',
    '../requester/mctp_endpoint_discovery.cpp',
    '../requester/request_timeout_tracker.cpp',
    'sensorManager.cpp',
    'socket_handler.cpp',
    'nsmDevice.cpp',
    'nsmMsghandler.cpp',
    'nsmObjectFactory.cpp',
    'nsmSensorAggregator.cpp',
    'nsmSensor.cpp',
    'eventTypeHandlers.cpp',
    'nsmEvent.cpp',
]

# libnsm conditional includes
libnsm_all = get_option('libnsm-all')
libnsm_include = get_option('libnsm-include')
cx8_features = get_option('cx8-features')
if cx8_features
    libnsm_all = false
    libnsm_include = [
        'device-capability-discovery',
        'device-configuration',
        'platform-environmental',
        'pci-links',
        'network-ports',
    ]
endif

valid_libnsm = [
    'debug-token',
    'firmware-utils',
    'device-capability-discovery',
    'device-configuration',
    'diagnostics',
    'platform-environmental',
    'pci-links',
    'network-ports',
]
foreach name : libnsm_include
    if not valid_libnsm.contains(name)
        warning('Unknown libnsm-include: ' + name)
    endif
endforeach

foreach module : valid_libnsm
    if libnsm_all or libnsm_include.contains(module)
        sources += ['../libnsm/' + module + '.c']
    endif
endforeach

if libnsm_all or libnsm_include.contains('platform-environmental')
    sources += ['../libnsm/powersmoothing-powerprofile-api-v2.c']
endif

nsmd_headers = ['.', '..', '../libnsm', '../requester', '../common', '../dot']

# Subdirectories ALWAYS compiled (core functionality)
subdir('nsmRawCommand')
subdir('nsmLongRunning')
subdir('nsmEvent')
subdir('nsmSetAsync')
subdir('nsmCommon')
subdir('nsmProgressCounters')

# Conditional subdirectories - controlled by nsmd-subdirs-all/nsmd-subdirs-include
nsmd_subdirs_all = get_option('nsmd-subdirs-all')
nsmd_subdirs_include = get_option('nsmd-subdirs-include')
if cx8_features
    nsmd_subdirs_all = false
    nsmd_subdirs_include = [
        'nsmChassis',
        'nsmSensors',
        'nsmDbusIfaceOverride',
        'nsmCommon',
        'nsmPort',
        'nsmDeviceInventory',
    ]
endif
debug_token_enabled = get_option('enable-debug-token').enabled()
dot_enabled = get_option('enable-dot').enabled()

# Valid optional subdirectories (not included by default unless in nsmd-subdirs-all or nsmd-subdirs-include)
valid_nsmd_subdirs = [
    'nsmNumericSensor',
    'nsmFwSwInventory',
    'nsmProcessor',
    'nsmEventConfig',
    'nsmChassis',
    'nsmDebugToken',
    'nsmDot',
    'nsmPort',
    'nsmDbusIfaceOverride',
    'nsmSPI',
    'nsmErrorInjection',
    'nsmMemory',
    'nsmPCIeDevice',
    'nsmDeviceInventory',
    'nsmDumpCollection',
    'nsmSensors',
    'nsmGPIO',
    'nsmGPM',
    'nsmFirmwareUtils',
    'nsmManagers',
    'nsmHistograms',
    'nsmReady',
]

# Validate requested subdirs
foreach name : nsmd_subdirs_include
    if not valid_nsmd_subdirs.contains(name)
        warning('Unknown nsmd-subdirs-include: ' + name)
    endif
endforeach

# Helper function to check if subdir should be skipped due to disabled features
subdir_should_skip = {
    'nsmDebugToken': not debug_token_enabled,
    'nsmDot': not dot_enabled,
    'nsmDumpCollection': not get_option('enable-debug-info').enabled(),
}

# Process subdirs: included if nsmd-subdirs-all=true OR in nsmd-subdirs-include
foreach d : valid_nsmd_subdirs
    if subdir_should_skip.get(d, false)
        if nsmd_subdirs_include.contains(d)
            warning(
                d + ' requested but required feature option is disabled; skipping',
            )
        endif
        continue
    endif
    if nsmd_subdirs_all or nsmd_subdirs_include.contains(d)
        subdir(d)
    endif
endforeach

if get_option('tracing').enabled()
    deps += [liblttng, ldl]
    sources += ['../tracepoints/nsmd-tp.c']
    nsmd_headers += ['../tracepoints']
endif

nsmd_base = static_library(
    'nsmd_base',
    sources,
    implicit_include_directories: false,
    include_directories: include_directories(nsmd_headers),
    dependencies: deps,
    cpp_args: tests_cpp_args,
)

if not get_option('b_coverage')
    nsmd_prod = static_library(
        'nsmd_prod',
        'nsmd.cpp',
        'socketIo.cpp',
        '../common/dBusHandler.cpp',
        implicit_include_directories: false,
        include_directories: include_directories(nsmd_headers),
        dependencies: deps,
    )

    # Build nsmd binary
    executable(
        'nsmd',
        link_whole: nsmd_base,
        link_with: nsmd_prod,
        install: true,
        install_dir: get_option('bindir'),
    )
endif

if get_option('progressCounter').enabled()
    executable(
        'nsmProgressCountersReader',
        'nsmProgressCounters/progressCounterReader.cpp',
        '../common/utils.cpp',
        '../common/dBusHandler.cpp',
        include_directories: include_directories(['../common', '../libnsm']),
        dependencies: [phosphor_logging, sdbusplus, phosphor_dbus_interfaces],
        install: true,
        install_dir: get_option('bindir'),
    )
endif

if get_option('shmem').enabled() and not get_option('cx8-features')
    executable(
        'nsmDump',
        'nsmDump.cpp',
        dependencies: [nvidia_shmem, phosphor_logging],
        install: true,
        install_dir: get_option('bindir'),
    )
endif

# Tests
if get_option('tests').enabled()

    tests_include_dirs = [nsmd_headers, '../common/test', 'test']

    nsmd_test = static_library(
        'nsmd_test',
        '../common/test/mockDBusHandler.cpp',
        '../common/test/mockSocketIo.cpp',
        implicit_include_directories: false,
        include_directories: include_directories(tests_include_dirs),
        dependencies: [deps, gmock],
        cpp_args: tests_cpp_args,
    )

    tests_deps = declare_dependency(
        link_with: [nsmd_base, nsmd_test],
        include_directories: include_directories(tests_include_dirs),
        dependencies: [deps, gtest, gmock],
        compile_args: tests_cpp_args,
    )

    subdir('test')
    subdir('nsmRawCommand/test')
    subdir('nsmEvent/test')
    subdir('nsmLongRunning/test')
    subdir('nsmSetAsync/test')
    subdir('nsmCommon/test')
    subdir('nsmProgressCounters/test')

    # Conditional test subdirs (same logic as subdir processing above)
    foreach d : valid_nsmd_subdirs
        if subdir_should_skip.get(d, false)
            continue
        endif
        if nsmd_subdirs_all or nsmd_subdirs_include.contains(d)
            subdir(d + '/test')
        endif
    endforeach
endif
