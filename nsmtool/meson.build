
sources = [
  'cmd_helper.cpp',
  'nsm_discovery_cmd.cpp',
  'nsm_telemetry_cmd.cpp',
  'nsmtool.cpp',
  '../libnsm/platform-environmental.c',
  '../libnsm/base.c',
  '../libnsm/requester/mctp.c',
  '../common/utils.cpp',
]

nsmtool_headers = ['.', '..', '../libnsm', '../common']

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
  ],
  install: true,
  install_dir: get_option('bindir'))
