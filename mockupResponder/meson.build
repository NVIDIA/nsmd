sources = [
    'nsmMockupResponder.cpp',
    'debugToken.cpp',
    'mockupResponder.cpp',
    'firmwareUtils.cpp',
    '../common/utils.cpp',
    '../common/dBusHandler.cpp',
    '../libnsm/base.c',
    '../libnsm/debug-token.c',
    '../libnsm/device-capability-discovery.c',
    '../libnsm/device-configuration.c',
    '../libnsm/diagnostics.c',
    '../libnsm/platform-environmental.c',
    '../libnsm/pci-links.c',
    '../libnsm/network-ports.c',
    '../libnsm/firmware-utils.c',
]

nsmMockupResponder_headers = ['.', '..', '../libnsm', '../common']

executable(
    'nsmMockupResponder',
    sources,
    implicit_include_directories: false,
    include_directories: include_directories(nsmMockupResponder_headers),
    dependencies: [
        CLI11_dep,
        nlohmann_json,
        sdbusplus,
        sdeventplus,
        phosphor_dbus_interfaces,
        phosphor_logging,
    ],
    install: true,
    install_dir: get_option('bindir'),
)