sources = [
    '../common/utils.cpp',
    '../common/dBusHandler.cpp',
    '../libnsm/base.c',
    '../libnsm/firmware-utils.c',
    '../libnsm/device-capability-discovery.c',
    '../libnsm/device-configuration.c',
    '../libnsm/diagnostics.c',
    '../libnsm/platform-environmental.c',
    '../libnsm/pci-links.c',
    '../libnsm/instance-id.c',
    '../libnsm/requester/mctp.c',
    '../requester/mctp_endpoint_discovery.cpp',
    '../requester/request_timeout_tracker.cpp',
    'deviceManager.cpp',
    'sensorManager.cpp',
    'socket_handler.cpp',
    'nsmDevice.cpp',
    'nsmObjectFactory.cpp',
    'nsmd.cpp',
    'nsmSensorAggregator.cpp',
    'nsmSensor.cpp',
    'nsmNumericSensor/nsmNumericSensorComposite.cpp',
    'eventType3Handler.cpp',
    'nsmEvent.cpp'
]

nsmd_headers = [
    '.',
    '..',
    '../libnsm',
    '../requester',
    '../common',
]

subdir('nsmNumericSensor')
subdir('nsmFwSwInventory')
subdir('nsmProcessor')
subdir('nsmEventConfig')
subdir('nsmChassis')
subdir('nsmPort')
subdir('nsmDbusIfaceOverride')
subdir('nsmSetAsync')
subdir('nsmMemory')
subdir('nsmCommon')
subdir('nsmPCIeDevice')
subdir('nsmDeviceInventory')
subdir('nsmSensors')
subdir('nsmEvent')
subdir('nsmGPM')

executable(
    'nsmd',
    sources,
    implicit_include_directories: false,
    include_directories: include_directories(nsmd_headers),
    dependencies: deps,
    install: true,
    install_dir: get_option('bindir'),
)