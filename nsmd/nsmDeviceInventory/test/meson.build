tests = [
    'nsmDeviceInventory_test',
    'nsmNvSwitchDeviceConfiguration_test',
    'nsmNetworkAdapter_test',
    'nsmSwitch_extended_test',
    'nsmSwitchBranch_test',
    'nsmPCIeRetimerFabricsDI_test',
    'nsmPCIeRetimerSwitchDI_test',
    'nsmPCIeRetimerSwitchDIBranch_test',
    'nsmNetworkAdapterBranch_test',
    'nsmSwitchFactoryBranch_test',
    'nsmSwitchBranch2_test',
    'nsmSwitchBranch5_test',
    'nsmDeviceInventoryBranch_test',
    'nsmPCIeRetimerSwitchDIBranch2_test',
    'nsmNetworkAdapterBranch2_test',
    'nsmPCIeRetimerFabricsDIBranch_test',
    'nsmSwitchBranch6_test',
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
