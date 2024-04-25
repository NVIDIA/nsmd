
sources += [
    'nsmGpuChassis/nsmGpuChassis.cpp',
    'nsmGpuChassis/nsmGpuChassisAssembly.cpp',
    'nsmGpuChassis/nsmGpuChassisPCIeDevice.cpp',
    'nsmGpuChassis/nsmInventoryProperty.cpp',
    'nsmGpuChassis/nsmPowerSupplyStatus.cpp',
    'nsmGpuChassis/nsmGpuPresenceAndPowerStatus.cpp',
    'nsmGpuChassis/nsmPCIeFunction.cpp',
    'nsmGpuChassis/nsmPCIeDevice.cpp',
    'nsmGpuChassis/nsmPCIeLTSSMState.cpp',
    'nsmGpuChassis/nsmSensorHelper.cpp',
]

nsmd_headers += ['.', '..']

if get_option('tests').enabled()
    subdir('test')
endif
