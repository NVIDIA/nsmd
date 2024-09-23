sources += [
    '../libnsm/network-ports.c',
    '../libnsm/pci-links.c',
    'nsmPort/nsmPCIeErrors.cpp',
    'nsmPort/nsmPort.cpp',
    'nsmPort/nsmPortDisableFuture.cpp',
    'nsmPort/nsmRetimerPort.cpp',
    'nsmPort/nsmFpgaPort.cpp',
    'nsmPort/nsmGpuPciePort.cpp',
    'nsmPort/nsmPCIePort.cpp',
    'nsmPort/nsmZone.cpp',
    'nsmPort/nsmEndpoint.cpp',
]

nsmd_headers += ['.', '..', 'nsmPort']

if get_option('tests').enabled()
    subdir('test')
endif