project('libnsm', ['c','cpp'],
        version: '0.1', meson_version: '>=0.58.0',
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
conf_data.set('INSTANCE_ID_EXPIRATION_INTERVAL', get_option('instance-id-expiration-interval'))
conf_data.set('NUMBER_OF_REQUEST_RETRIES', get_option('number-of-request-retries'))
conf_data.set('RESPONSE_TIME_OUT', get_option('response-time-out'))
if get_option('shmem').enabled()
  conf_data.set('NVIDIA_SHMEM', 0)
endif
configure_file(output: 'config.h',
  configuration: conf_data
)

dynamic_linker = []

deps = [
  CLI11_dep,
  nlohmann_json,
  sdbusplus,
  sdeventplus,
  phosphor_dbus_interfaces,
  phosphor_logging,
]

if get_option('shmem').enabled()
  nvidia_shmem = dependency('nvidia-shmem', required : true)
  deps += nvidia_shmem
endif

systemd = dependency('systemd')
systemd_system_unit_dir = systemd.get_variable(pkgconfig: 'systemdsystemunitdir')

subdir('services')
subdir('nsmtool')
subdir('nsmd')

if get_option('mockup-responder').enabled()
  subdir('mockupResponder')
endif

if get_option('tests').enabled()
  subdir('common/test')
  subdir('libnsm/test')
  subdir('mockupResponder/test')
  subdir('nsmtool/test')
  subdir('nsmd/test')
endif

install_subdir('libnsm/instance-db',
               install_mode: 'r--r--r--',
               install_dir: get_option('datadir') / meson.project_name())
