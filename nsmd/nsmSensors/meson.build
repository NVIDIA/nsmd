sources += []


include_all_sensors = get_option('nsm-sensors-all')
include_sensors_list = get_option('nsm-sensors-include')
if get_option('cx8-features')
    include_all_sensors = false
    include_sensors_list = ['inventory-property', 'pcie-link-speed']
endif

valid_sensors = ['inventory-property', 'pcie-link-speed']
foreach name : include_sensors_list
    if not valid_sensors.contains(name)
        warning('Unknown nsm-sensors-include: ' + name)
    endif
endforeach

if include_all_sensors or include_sensors_list.contains('inventory-property')
    sources += ['nsmSensors/nsmInventoryProperty.cpp']
endif
if include_all_sensors or include_sensors_list.contains('pcie-link-speed')
    sources += ['nsmSensors/nsmPCIeLinkSpeed.cpp']
endif


nsmd_headers += ['.', '..', 'nsmSensors', 'nsmPort']
