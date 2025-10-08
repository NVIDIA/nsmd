sources += []

include_all_ports = get_option('nsm-port-all')
include_port_list = get_option('nsm-port-include')
if get_option('cx8-features')
    include_all_ports = false
    include_port_list = ['retimer', 'nvlink-port', 'pcie-port']
endif

valid_ports = [
    'retimer',
    'fpga',
    'gpu-pcie',
    'nvlink-port',
    'pcie-port',
    'zone',
    'endpoint',
    'port-disable',
]
foreach name : include_port_list
    if not valid_ports.contains(name)
        warning('Unknown nsm-port-include: ' + name)
    endif
endforeach

if include_all_ports or include_port_list.contains('retimer')
    sources += ['nsmPort/nsmRetimerPort.cpp']
    sources += ['nsmPort/nsmPortConfigurationInfo.cpp']
endif
if include_all_ports or include_port_list.contains('nvlink-port')
    sources += ['nsmPort/nsmPort.cpp']
endif
if include_all_ports or include_port_list.contains('port-disable')
    sources += ['nsmPort/nsmPortDisableFuture.cpp']
endif
if include_all_ports or include_port_list.contains('fpga')
    sources += ['nsmPort/nsmFpgaPort.cpp']
endif
if include_all_ports or include_port_list.contains('gpu-pcie')
    sources += ['nsmPort/nsmGpuPciePort.cpp']
endif
if include_all_ports or include_port_list.contains('pcie-port')
    sources += ['nsmPort/nsmPCIeErrors.cpp']
    sources += ['nsmPort/nsmPCIePort.cpp']
endif
if include_all_ports or include_port_list.contains('zone')
    sources += ['nsmPort/nsmZone.cpp']
endif
if include_all_ports or include_port_list.contains('endpoint')
    sources += ['nsmPort/nsmEndpoint.cpp']
endif

nsmd_headers += ['.', '..']

if get_option('tests').enabled()
    subdir('test')
endif
