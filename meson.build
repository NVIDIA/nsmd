project('nsmd', ['c','cpp'],
        version: '0.1', meson_version: '>=0.57.0',
        default_options: [
          'warning_level=3',
          'werror=true',
          'cpp_std=c++20'
        ])

# Enable support for the C++ coroutines extension
add_project_arguments('-fcoroutines',language : ['cpp'])

nlohmann_json = dependency('nlohmann_json')
sdbusplus = dependency('sdbusplus')
sdeventplus = dependency('sdeventplus')
phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')
phosphor_logging = dependency('phosphor-logging')

if get_option('tests').enabled()
  gtest = dependency('gtest', main: true, disabler: true, required: false)
  gmock = dependency('gmock', disabler: true, required: false)
endif

cpp = meson.get_compiler('cpp')
if cpp.has_header('CLI/CLI.hpp')
  CLI11_dep = declare_dependency()
else
  CLI11_dep = dependency('CLI11')
endif

conf_data = configuration_data()
conf_data.set('SENSOR_POLLING_TIME', get_option('sensor-polling-time'))
configure_file(output: 'config.h',
  configuration: conf_data
)

dynamic_linker = []

deps = [
  nlohmann_json,
  sdbusplus,
  sdeventplus,
  phosphor_dbus_interfaces,
  phosphor_logging,
]

systemd = dependency('systemd')
systemd_system_unit_dir = systemd.get_variable(pkgconfig: 'systemdsystemunitdir')

subdir('services')

if get_option('tests').enabled()
  subdir('libnsm/test')
endif
