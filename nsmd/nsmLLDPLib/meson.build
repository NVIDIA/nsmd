sources += []

include_all_ports = get_option('nsm-port-all')
include_port_list = get_option('nsm-port-include')
if get_option('cx8-features')
    include_all_ports = false
endif

if include_all_ports or include_port_list.contains('lldp')
    sources += [
        'nsmLLDPLib/nsmLldpMode.cpp',
        'nsmLLDPLib/nsmLldpPacket.cpp',
        'nsmLLDPLib/nsmLldpPort.cpp',
    ]
endif

nsmd_headers += ['.', '..', 'nsmLLDPLib']
