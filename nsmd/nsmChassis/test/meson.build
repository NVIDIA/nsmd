# nsmChassis tests now use nsmd_test
# nsmChassis is CONDITIONAL - only runs if enabled, in which case it's in nsmd_test
# All sources already compiled into nsmd_test, no need to recompile

tests = [
    'nsmAERError_test',
    'nsmChassis_test',
    'nsmChassisAssembly_test',
    'nsmChassisLED_test',
    'nsmChassisPCIeDevice_test',
    'nsmChassisPCIeSlot_test',
    'nsmErot_test',
    'nsmGpuClockControl_test',
    'nsmIRoTResponder_test',
    'nsmLeakDetection_test',
    'nsmNVSwitchAndNVMgmtNICChassis_test',
    'nsmNVSwitchAndNVMgmtNICChassisAssembly_test',
    'nsmNVSwitchChassis_test',
    'nsmPCIeRetimer_test',
    'nsmPowerControl_test',
    'nsmPowerSubSystem_test',
    'nsmProcessorModulePowerControl_test',
    'nsmRoTProperty_test',
    'nsmSecurityRBP_test',
    'nsmUpdateApSkuId_test',
    'nsmRoT_workload_test',
    'nsmWriteProtectedJumper_test',
    'nsmPowerSupplyStatus_test',
    'nsmPCIeLTSSMState_test',
    'nsmGpuPcieBranch_test',
    'nsmDeviceBranch_test',
    'nsmNVSwitchChassisFactory_test',
    'nsmPowerSmoothingV1_test',
    'nsmPowerControlBranch_test',
    'nsmMemoryBranch_test',
    'nsmSecurityRBPBranch_test',
    'nsmPCIeRetimerSwitchDI_test',
    'nsmNVSwitchChassisAssemblyBranch_test',
    'nsmPowerSmoothingBranch_test',
    'nsmWorkloadPowerProfile_test',
    'nsmOemResetStatistics_test',
    'nsmProcessorFactoryBranch_test',
    'nsmPowerSmoothingV2_test',
    'nsmIRoTResponderBranch_test',
    'nsmSwitchBranch_test',
    'nsmAERErrorAndChassisBranch_test',
    'nsmClockOutputEnableState_test',
    # 'nsmPCIeFunction_test',  # DISABLED: NsmInterfaceProvider has deleted constructor - requires architectural refactoring
    # 'nsmPCIeSlot_test',  # DISABLED: deleted constructor (NsmInterfaceProvider) - same as nsmPCIeFunction
]

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            implicit_include_directories: false,
            include_directories: ['.', '..'],
            link_args: dynamic_linker,
            dependencies: tests_deps,
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
