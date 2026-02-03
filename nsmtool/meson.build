openssl_dep = dependency('openssl', required: true)

sources = [
    'cmd_helper.cpp',
    'nsm_config_cmd.cpp',
    'nsm_dbus_client.cpp',
    'nsm_dbus_cmd.cpp',
    'nsm_diag_cmd.cpp',
    'nsm_discovery_cmd.cpp',
    'nsm_firmware_cmd.cpp',
    'nsm_telemetry_cmd.cpp',
    'nsm_passthrough_cmd.cpp',
    'nsmtool.cpp',
    '../dot/nsmDotUtils.cpp',
    '../common/utils.cpp',
    '../common/dBusHandler.cpp',
    '../debug-token/tlv_decoder.cpp',
    '../debug-token/tlv_encoder.cpp',
    '../libnsm/requester/mctp.c',
]

nsmtool_headers = ['.', '..', '../libnsm', '../common', '../dot']

executable(
    'nsmtool',
    sources,
    implicit_include_directories: false,
    include_directories: include_directories(nsmtool_headers),
    dependencies: [
        CLI11_dep,
        nlohmann_json,
        sdbusplus,
        phosphor_logging,
        dependency('dbus-1'),
        openssl_dep,
        libnsm_dep,
    ],
    install: true,
    install_dir: get_option('bindir'),
)
