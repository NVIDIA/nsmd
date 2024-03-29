
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
  'nsmNumericAggregator.cpp',
  'nsmObjectFactory.cpp',
  'nsmd.cpp',
]

nsmd_headers = ['.', '..', '../libnsm', '../requester', '../common']

subdir('nsmTemp')
subdir('nsmPower')
subdir('nsmFwSwInventory')

executable(
  'nsmd',
  sources,
  implicit_include_directories: false,
  include_directories: include_directories(nsmd_headers),
  dependencies: deps,
  install: true,
  install_dir: get_option('bindir'))
