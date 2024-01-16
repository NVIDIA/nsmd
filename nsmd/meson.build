
sources = [
  '../common/utils.cpp',
  '../libnsm/base.c',
  '../libnsm/platform-environmental.c',
  '../libnsm/instance-id.c',
  '../requester/mctp_endpoint_discovery.cpp',
  'deviceManager.cpp',
  'sensorManager.cpp',
  'socket_handler.cpp',
  'nsmDevice.cpp',
  'nsmTemp.cpp',
  'nsmd.cpp',
]

nsmd_headers = ['.', '..', '../libnsm', '../requester', '../common']

executable(
  'nsmd',
  sources,
  implicit_include_directories: false,
  include_directories: include_directories(nsmd_headers),
  dependencies: [
    CLI11_dep,
    nlohmann_json,
    sdbusplus,
    sdeventplus,
    phosphor_dbus_interfaces,
    phosphor_logging,
  ],
  install: true,
  install_dir: get_option('bindir'))
