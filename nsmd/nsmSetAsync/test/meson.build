dep_src_files = [
  '../asyncOperationManager.cpp',
  '../../nsmDevice.cpp',
  '../../nsmSensor.cpp',
  '../../sensorManager.cpp',
  '../../nsmObjectFactory.cpp',
  '../../../common/utils.cpp',
  '../../../common/test/mockDBusHandler.cpp',
  '../../../libnsm/instance-id.c',
  '../../../libnsm/requester/mctp.c',
  '../../../requester/request_timeout_tracker.cpp'
]

dep_src_headers = [
  '..',
  '../../',
  '../../../',
  '../../../common',
  '../../../libnsm',
]

test_src = declare_dependency(sources: dep_src_files,
                              include_directories: dep_src_headers)

tests_files = [
  'asyncOperationManager_test.cpp',
]

test('asyncOperationManager_test', 
                   executable('asyncOperationManager_test',
                   tests_files,
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
