
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
  'nsmNumericSensor.cpp',
  'nsmTemp.cpp',
  'nsmPower.cpp',
  'nsmNumericAggregator.cpp',
  'nsmTempAggregator.cpp',
  'nsmPowerAggregator.cpp',
  'nsmd.cpp',
]

nsmd_headers = ['.', '..', '../libnsm', '../requester', '../common']

executable(
  'nsmd',
  sources,
  implicit_include_directories: false,
  include_directories: include_directories(nsmd_headers),
  dependencies: deps,
  install: true,
  install_dir: get_option('bindir'))
