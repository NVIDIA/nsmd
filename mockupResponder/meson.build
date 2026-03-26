sources = [
    'nsmMockupResponder.cpp',
    'debugToken.cpp',
    'mockupResponder.cpp',
    'firmwareUtils.cpp',
    '../common/event.cpp',
    '../common/utils.cpp',
    '../common/dBusHandler.cpp',
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
        libnsm_dep,
    ],
    install: true,
    install_dir: get_option('bindir'),
)
