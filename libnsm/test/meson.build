dep_src_files = [
  '../base.c',
  '../device-capability-discovery.c',
  '../platform-environmental.c',
  '../network-ports.c',
]

dep_src_headers = [
  '.',
  '..',
  '../../common',
  '../../libnsm',
]

test_src = declare_dependency(sources: dep_src_files,
                              include_directories: dep_src_headers)

tests = [
  'libnsm_base_test',
  'libnsm_device_capability_discovery_test',
  'libnsm_platform_environmental_test',
  'libnsm_network_ports_test'
]

libnsm_headers = ['.', '..', '../../common']

foreach t : tests
  test(t, executable(t.underscorify(), t + '.cpp',
                     implicit_include_directories: false,
                     link_args: dynamic_linker,
                     build_rpath: '',
                     dependencies: [
                         test_src,
                         gtest,
                         gmock]),
       workdir: meson.current_source_dir())
endforeach