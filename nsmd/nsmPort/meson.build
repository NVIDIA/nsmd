
sources += [
    '../libnsm/network-ports.c',
    '../libnsm/pci-links.c',
    'nsmPort/nsmPort.cpp',
    'nsmPort/nsmRetimerPort.cpp',
    'nsmPort/nsmFpgaPort.cpp',
    'nsmPort/nsmGpuPciePort.cpp',
    'nsmPort/nsmZone.cpp',
    'nsmPort/nsmEndpoint.cpp'
]

nsmd_headers += ['.', '..']

if get_option('tests').enabled()
    subdir('test')
endif
