dep_src_files = [
  '../nsmTemp.cpp',
  '../nsmPower.cpp',
  '../nsmEnergy.cpp',
  '../nsmVoltage.cpp',
  '../nsmTempAggregator.cpp',
  '../nsmPowerAggregator.cpp',
  '../nsmEnergyAggregator.cpp',
  '../nsmVoltageAggregator.cpp',
  '../nsmNumericSensorFactory.cpp',
  '../nsmNumericSensor.cpp',
  '../nsmNumericAggregator.cpp',
  '../../nsmDevice.cpp',
  '../../nsmObjectFactory.cpp',
  '../../nsmSensorAggregator.cpp',
  '../../sensorManager.cpp',
  '../../../common/utils.cpp',
  '../../../libnsm/base.c',
  '../../../libnsm/platform-environmental.c',
  '../../../libnsm/instance-id.c',
  '../../../libnsm/requester/mctp.c',
]

dep_src_headers = [
  '.',
  '..',
  '../../',
  '../../test/',
  '../../../',
  '../../../common',
  '../../../libnsm',
]

test_src = declare_dependency(sources: dep_src_files,
                              include_directories: dep_src_headers)

tests_files = [
  'nsmNumeric_test.cpp',
  'nsmNumericAggregator_test.cpp',
  'nsmNumericSensors_test.cpp',
  'nsmNumericAggregatorSensors_test.cpp',
]

test('nsmNumericSensors_test', executable('nsmNumericSensors_test', tests_files,
                   implicit_include_directories: false,
                   link_args: dynamic_linker,
                   build_rpath: '',
                   dependencies: [
                       test_src,
                       CLI11_dep,
                       nlohmann_json,
                       sdbusplus,
                       sdeventplus,
                       phosphor_logging,
                       nvidia_shmem,
                       gtest,
                       gmock]),
                   workdir: meson.current_source_dir()
)
