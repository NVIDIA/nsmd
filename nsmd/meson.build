sources = [
    '../common/utils.cpp',
    '../common/dBusHandler.cpp',
    '../libnsm/base.c',
    '../libnsm/device-capability-discovery.c',
    '../libnsm/platform-environmental.c',
    '../libnsm/pci-links.c',
    '../libnsm/instance-id.c',
    '../libnsm/requester/mctp.c',
    '../requester/mctp_endpoint_discovery.cpp',
    'deviceManager.cpp',
    'sensorManager.cpp',
    'socket_handler.cpp',
    'nsmDevice.cpp',
    'nsmObjectFactory.cpp',
    'nsmd.cpp',
    'nsmSensorAggregator.cpp',
    'nsmSensor.cpp',
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
subdir('nsmMemory')
subdir('nsmPCIeDevice')
subdir('nsmDeviceInventory')

executable(
    'nsmd',
    sources,
    implicit_include_directories: false,
    include_directories: include_directories(nsmd_headers),
    dependencies: deps,
    install: true,
    install_dir: get_option('bindir'),
)
