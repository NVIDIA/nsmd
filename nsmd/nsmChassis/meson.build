sources += [
    'nsmChassis/nsmNVSwitchAndNVMgmtNICChassis.cpp',
    'nsmChassis/nsmNVSwitchAndNVMgmtNICChassisAssembly.cpp',
    'nsmChassis/nsmPCIeRetimer.cpp',
    'nsmChassis/nsmChassis.cpp',
    'nsmChassis/nsmChassisAssembly.cpp',
    'nsmChassis/nsmChassisPCIeDevice.cpp',
    'nsmChassis/nsmChassisPCIeSlot.cpp',
    'nsmChassis/nsmClockOutputEnableState.cpp',
    'nsmChassis/nsmPowerSupplyStatus.cpp',
    'nsmChassis/nsmGpuPresenceAndPowerStatus.cpp',
    'nsmChassis/nsmPCIeFunction.cpp',
    'nsmChassis/nsmPCIeLTSSMState.cpp',
    'nsmChassis/nsmPowerControl.cpp',
    'nsmChassis/nsmGpuClockControl.cpp',
    'nsmChassis/nsmPowerSubSystem.cpp',
    'nsmChassis/nsmWriteProtectedJumper.cpp',
    'nsmChassis/nsmAERError.cpp',
    'nsmChassis/nsmErot.cpp',
    'nsmChassis/nsmSecurityRBP.cpp',
    'nsmChassis/nsmProcessorModulePowerControl.cpp',
]

nsmd_headers += ['.', '..', 'nsmDbusIfaceOverride', 'nsmFirmwareUtils']

if get_option('tests').enabled()
    subdir('test')
endif