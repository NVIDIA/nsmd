sources += []

include_all_chassis = get_option('nsm-chassis-all')
include_chassis_list = get_option('nsm-chassis-include')
if get_option('cx8-features')
    include_all_chassis = false
    include_chassis_list = [
        'nv-nic-chassis',
        'nv-nic-chassis-assembly',
        'pcie-function',
        'pcie-device',
    ]
endif

valid_chassis = [
    'chassis-core',
    'nv-nic-chassis',
    'nv-nic-chassis-assembly',
    'pcie-retimer',
    'led',
    'chassis-assembly',
    'pcie-device',
    'pcie-slot',
    'clock-output-enable',
    'power-supply-status',
    'gpu-presence-power',
    'pcie-function',
    'pcie-ltssm',
    'power-control',
    'gpu-clock-control',
    'power-subsystem',
    'write-protected-jumper',
    'aer-error',
    'leak-detection',
    'erot',
    'processor-module-power-control',
    'irot-responder',
]
if include_all_chassis or include_chassis_list.contains('chassis-core')
    sources += [
        'nsmChassis/nsmChassis.cpp',
        'nsmChassis/nsmApSkuId.cpp',
        'nsmChassis/nsmUpdateApSkuId.cpp',
    ]
endif
foreach name : include_chassis_list
    if not valid_chassis.contains(name)
        warning('Unknown nsm-chassis-include: ' + name)
    endif
endforeach

if include_all_chassis or include_chassis_list.contains('nv-nic-chassis')
    sources += ['nsmChassis/nsmNVSwitchAndNVMgmtNICChassis.cpp']
endif
if include_all_chassis or include_chassis_list.contains(
    'nv-nic-chassis-assembly',
)
    sources += ['nsmChassis/nsmNVSwitchAndNVMgmtNICChassisAssembly.cpp']
endif
if include_all_chassis or include_chassis_list.contains('pcie-retimer')
    sources += ['nsmChassis/nsmPCIeRetimer.cpp']
endif
if include_all_chassis or include_chassis_list.contains('led')
    sources += ['nsmChassis/nsmChassisLED.cpp']
endif
if include_all_chassis or include_chassis_list.contains('chassis-assembly')
    sources += ['nsmChassis/nsmChassisAssembly.cpp']
endif
if include_all_chassis or include_chassis_list.contains('pcie-device')
    sources += ['nsmChassis/nsmChassisPCIeDevice.cpp']
endif
if include_all_chassis or include_chassis_list.contains('pcie-slot')
    sources += ['nsmChassis/nsmChassisPCIeSlot.cpp']
endif
if include_all_chassis or include_chassis_list.contains('clock-output-enable')
    sources += ['nsmChassis/nsmClockOutputEnableState.cpp']
endif
if include_all_chassis or include_chassis_list.contains('power-supply-status')
    sources += ['nsmChassis/nsmPowerSupplyStatus.cpp']
endif
if include_all_chassis or include_chassis_list.contains('gpu-presence-power')
    sources += ['nsmChassis/nsmGpuPresenceAndPowerStatus.cpp']
endif
if include_all_chassis or include_chassis_list.contains('pcie-function')
    sources += ['nsmChassis/nsmPCIeFunction.cpp']
endif
if include_all_chassis or include_chassis_list.contains('pcie-ltssm')
    sources += ['nsmChassis/nsmPCIeLTSSMState.cpp']
endif
if include_all_chassis or include_chassis_list.contains('power-control')
    sources += ['nsmChassis/nsmPowerControl.cpp']
endif
if include_all_chassis or include_chassis_list.contains('gpu-clock-control')
    sources += ['nsmChassis/nsmGpuClockControl.cpp']
endif
if include_all_chassis or include_chassis_list.contains('power-subsystem')
    sources += ['nsmChassis/nsmPowerSubSystem.cpp']
endif
if include_all_chassis or include_chassis_list.contains('write-protected-jumper')
    sources += ['nsmChassis/nsmWriteProtectedJumper.cpp']
endif
if include_all_chassis or include_chassis_list.contains('aer-error')
    sources += ['nsmChassis/nsmAERError.cpp']
endif
if include_all_chassis or include_chassis_list.contains('leak-detection')
    sources += ['nsmChassis/nsmLeakDetection.cpp']
endif
if include_all_chassis or include_chassis_list.contains('erot')
    sources += ['nsmChassis/nsmErot.cpp', 'nsmChassis/nsmRoTProperty.cpp']
endif
if include_all_chassis or include_chassis_list.contains(
    'processor-module-power-control',
)
    sources += ['nsmChassis/nsmProcessorModulePowerControl.cpp']
endif
if include_all_chassis or include_chassis_list.contains('irot-responder')
    sources += ['nsmChassis/nsmIRoTResponder.cpp']
endif

nsmd_headers += ['.', '..', 'nsmDbusIfaceOverride', 'nsmFirmwareUtils']
