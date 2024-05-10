sources += [
    'nsmChassis/nsmNVSwitchAndNVMgmtNICChassis.cpp',
    'nsmChassis/nsmNVSwitchAndNVMgmtNICChassisAssembly.cpp',
    'nsmChassis/nsmPCIeRetimer.cpp',
    'nsmChassis/nsmChassis.cpp',
    'nsmChassis/nsmChassisAssembly.cpp',
    'nsmChassis/nsmChassisPCIeDevice.cpp',
    'nsmChassis/nsmInventoryProperty.cpp',
    'nsmChassis/nsmPowerSupplyStatus.cpp',
    'nsmChassis/nsmGpuPresenceAndPowerStatus.cpp',
    'nsmChassis/nsmPCIeFunction.cpp',
    'nsmChassis/nsmPCIeDevice.cpp',
    'nsmChassis/nsmPCIeLTSSMState.cpp',
]

nsmd_headers += ['.', '..']

if get_option('tests').enabled()
    subdir('test')
endif
