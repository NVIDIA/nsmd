sources = [
    '../common/utils.cpp',
    '../common/dBusHandler.cpp',
    '../debug-token/tlv_decoder.cpp',
    '../debug-token/tlv_encoder.cpp',
    '../dot/nsmDotUtils.cpp',
    '../libnsm/base.c',
    '../libnsm/requester/mctp.c',
    '../requester/mctp_endpoint_discovery.cpp',
    '../requester/request_timeout_tracker.cpp',
    'sensorManager.cpp',
    'socket_handler.cpp',
    'nsmDevice.cpp',
    'nsmObjectFactory.cpp',
    'nsmd.cpp',
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

if libnsm_all or libnsm_include.contains('debug-token')
    sources += ['../libnsm/debug-token.c']
endif
if libnsm_all or libnsm_include.contains('firmware-utils')
    sources += ['../libnsm/firmware-utils.c']
endif
if libnsm_all or libnsm_include.contains('network-ports')
    sources += ['../libnsm/network-ports.c']
endif
if libnsm_all or libnsm_include.contains('device-capability-discovery')
    sources += ['../libnsm/device-capability-discovery.c']
endif
if libnsm_all or libnsm_include.contains('device-configuration')
    sources += ['../libnsm/device-configuration.c']
endif
if libnsm_all or libnsm_include.contains('diagnostics')
    sources += ['../libnsm/diagnostics.c']
endif
if libnsm_all or libnsm_include.contains('platform-environmental')
    sources += ['../libnsm/platform-environmental.c']
    sources += ['../libnsm/powersmoothing-powerprofile-api-v2.c']
endif
if libnsm_all or libnsm_include.contains('pci-links')
    sources += ['../libnsm/pci-links.c']
endif
if libnsm_all or libnsm_include.contains('network-ports')
    sources += ['../libnsm/network-ports.c']
endif

nsmd_headers = [
    '.',
    '..',
    '../libnsm',
    '../requester',
    '../common',
    '../dot',
    'nsmDbusIfaceOverride',
]

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

# Process subdirs: included if nsmd-subdirs-all=true OR in nsmd-subdirs-include
foreach d : valid_nsmd_subdirs
    if (d == 'nsmDebugToken' and not debug_token_enabled) or (d == 'nsmDot' and not dot_enabled) or (d == 'nsmDumpCollection' and not get_option(
            'enable-debug-info',
        ).enabled())
        if nsmd_subdirs_include.contains(d)
            warning(
                d + ' requested but required feature option is disabled; skipping',
            )
        endif
        continue
    endif
    if nsmd_subdirs_all or nsmd_subdirs_include.contains(d)
        subdir(d)
        if d == 'nsmNumericSensor'
            sources += ['nsmNumericSensor/nsmNumericSensorComposite.cpp']
        elif d == 'nsmFwSwInventory'
            sources += ['nsmFwSwInventory/GPUSWInventory.cpp']
        elif d == 'nsmDot'
            sources += ['nsmDot/nsmDot.cpp']
        endif
    endif
endforeach

if get_option('tracing').enabled()
    deps += [liblttng, ldl]
    sources += ['../tracepoints/nsmd-tp.c']
    nsmd_headers += ['../tracepoints']
endif

executable(
    'nsmd',
    sources,
    implicit_include_directories: false,
    include_directories: include_directories(nsmd_headers),
    dependencies: deps,
    install: true,
    install_dir: get_option('bindir'),
)

if get_option('progressCounter').enabled()
    executable(
        'nsmProgressCountersReader',
        'nsmProgressCounters/progressCounterReader.cpp',
        '../common/utils.cpp',
        '../common/dBusHandler.cpp',
        include_directories: include_directories(['../common', '../libnsm']),
        dependencies: [phosphor_logging, sdbusplus],
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
