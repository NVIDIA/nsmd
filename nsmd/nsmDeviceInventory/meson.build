sources += []

include_all_dev_inv = get_option('nsm-device-inventory-all')
include_dev_inv_list = get_option('nsm-device-inventory-include')
if get_option('cx8-features')
    include_all_dev_inv = false
    include_dev_inv_list = [
        'network-adapter',
        'pcie-retimer-switch',
        'pcie-retimer-fabrics',
    ]
endif

valid_dev_inv = [
    'switch',
    'pcie-retimer-fabrics',
    'pcie-retimer-switch',
    'network-adapter',
]
foreach name : include_dev_inv_list
    if not valid_dev_inv.contains(name)
        warning('Unknown nsm-device-inventory-include: ' + name)
    endif
endforeach

# nsmNvSwitchDeviceConfiguration needs libnsm device-configuration (encode/decode
# 0x10/0x11) — enable via libnsm-all or libnsm-include in the nsmd recipe.
if include_all_dev_inv or include_dev_inv_list.contains('switch')
    sources += [
        'nsmDeviceInventory/nsmSwitch.cpp',
        'nsmDeviceInventory/nsmNvSwitchDeviceConfiguration.cpp',
    ]
endif
if include_all_dev_inv or include_dev_inv_list.contains('pcie-retimer-fabrics')
    sources += ['nsmDeviceInventory/nsmPCIeRetimerFabricsDI.cpp']
endif
if include_all_dev_inv or include_dev_inv_list.contains('pcie-retimer-switch')
    sources += ['nsmDeviceInventory/nsmPCIeRetimerSwitchDI.cpp']
endif
if include_all_dev_inv or include_dev_inv_list.contains('network-adapter')
    sources += ['nsmDeviceInventory/nsmNetworkAdapter.cpp']
endif

nsmd_headers += ['.', '..', 'nsmDebugToken', 'nsmDumpCollection']
