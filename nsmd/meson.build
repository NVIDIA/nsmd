sources = [
    '../common/utils.cpp',
    '../common/dBusHandler.cpp',
    '../debug-token/tlv_decoder.cpp',
    '../debug-token/tlv_encoder.cpp',
    '../libnsm/base.c',
    '../libnsm/instance-id.c',
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
    'nsmDbusIfaceOverride',
]

subdir('nsmRawCommand')
subdir('nsmLongRunning')
subdir('nsmEvent')
subdir('nsmSetAsync')
subdir('nsmCommon')

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

valid_nsmd_subdirs = [
    'nsmNumericSensor',
    'nsmFwSwInventory',
    'nsmProcessor',
    'nsmEventConfig',
    'nsmChassis',
    'nsmDebugToken',
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
    'nsmProgressCounters',
]

foreach name : nsmd_subdirs_include
    if not valid_nsmd_subdirs.contains(name)
        warning('Unknown nsmd-subdirs-include: ' + name)
    endif
endforeach

foreach d : valid_nsmd_subdirs
    if (d == 'nsmDebugToken' and not debug_token_enabled) or (d == 'nsmDumpCollection' and not get_option(
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
