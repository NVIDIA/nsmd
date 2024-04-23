
sources += [
    'nsmGpuChassis/nsmGpuChassis.cpp',
    'nsmGpuChassis/nsmGpuChassisPCIeDevice.cpp',
    'nsmGpuChassis/nsmInventoryProperty.cpp',
    'nsmGpuChassis/nsmPowerSupplyStatus.cpp',
    'nsmGpuChassis/nsmGpuPresenceAndPowerStatus.cpp',
    'nsmGpuChassis/nsmPCIeFunction.cpp',
    'nsmGpuChassis/nsmSensorHelper.cpp',
]

nsmd_headers += ['.', '..']

if get_option('tests').enabled()
    subdir('test')
endif
