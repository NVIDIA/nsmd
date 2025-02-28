project(
    'libnsm',
    ['c', 'cpp'],
    version: '0.1',
    meson_version: '>=0.58.0',
    default_options: ['warning_level=3', 'werror=true', 'cpp_std=c++20'],
)

# Enable support for the C++ coroutines extension
add_project_arguments('-fcoroutines', language: ['cpp'])

nlohmann_json = dependency('nlohmann_json')
sdbusplus = dependency('sdbusplus')
sdeventplus = dependency('sdeventplus')
phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')
phosphor_logging = dependency('phosphor-logging')

if get_option('tests').enabled()
    gtest = dependency(
        'gtest',
        main: true,
        disabler: true,
        required: false,
        version: '>=1.14.0',
    )
    gmock = dependency('gmock', disabler: true, required: false)
    if not gtest.found() or not gmock.found()
        googletest = import('cmake').subproject('googletest', required: true)
        gtest = declare_dependency(
            dependencies: [
                googletest.get_variable('gtest_dep'),
                googletest.get_variable('gtest_main_dep'),
            ],
        )
        gmock = googletest.get_variable('gmock_dep')
    endif
    gtest = gtest.as_system('system')
    gmock = gmock.as_system('system')
endif

if get_option('system-guid').enabled()
    add_project_arguments('-DENABLE_SYSTEM_GUID', language: ['cpp', 'c'])
endif

if get_option('mctp-eid0-filtering')
    add_project_arguments('-DFILTER_NULL_MCTP_EID=true', language: ['cpp', 'c'])
else
    add_project_arguments('-DFILTER_NULL_MCTP_EID=false', language: ['cpp', 'c'])
endif

if get_option('grace-spi-operations').enabled()
    add_project_arguments('-DENABLE_GRACE_SPI_OPERATIONS', language: ['cpp', 'c'])
endif

if get_option('grace-spi-operations-raw-debug-dump').enabled()
    add_project_arguments('-DENABLE_GRACE_SPI_OPERATION_RAW_DEBUG_DUMP', language: ['cpp', 'c'])
endif

cpp = meson.get_compiler('cpp')
if cpp.has_header('CLI/CLI.hpp')
    CLI11_dep = declare_dependency()
else
    CLI11_dep = dependency('CLI11')
endif

conf_data = configuration_data()
conf_data.set('SENSOR_POLLING_TIME', get_option('sensor-polling-time'))
conf_data.set('GPU_PWR_GD_GPIO', '"' + get_option('gpio-name') + '"')
conf_data.set(
    'SENSOR_POLLING_TIME_LONG_RUNNING',
    get_option('sensor-polling-time-long-running'),
)
conf_data.set(
    'NUM_GPU_PER_MODULE',
    get_option('num-gpu-per-module')
)
conf_data.set(
    'INSTANCE_ID_EXPIRATION_INTERVAL',
    get_option('instance-id-expiration-interval'),
)
conf_data.set(
    'NUMBER_OF_REQUEST_RETRIES',
    get_option('number-of-request-retries'),
)
conf_data.set('RESPONSE_TIME_OUT', get_option('response-time-out'))
conf_data.set(
    'RESPONSE_TIME_OUT_LONG_RUNNING',
    get_option('response-time-out-long-running'),
)
conf_data.set(
    'DEFAULT_RR_REFRESH_LIMIT_IN_MS',
    get_option('rr-refresh-limit'),
)
conf_data.set(
    'ALLOWED_BUFFER_IN_MS',
    get_option('allowed-buffer-in-ms-for-polling'),
)
conf_data.set(
    'INACTIVE_SLEEP_TIME_IN_MS',
    get_option('inactive-sleep-time-in-ms'),
)
if get_option('shmem').enabled()
    conf_data.set('NVIDIA_SHMEM', 0)
endif
if get_option('stanbyToDC').enabled()
    conf_data.set('NVIDIA_STANDBYTODC', 0)
endif
if get_option('reset-metrics').enabled()
    conf_data.set('NVIDIA_RESET_METRICS', 0)
endif

if get_option('enable-histogram').enabled()
    conf_data.set('NVIDIA_HISTOGRAM', 1)
endif

if get_option('aer-error-status-priority')
    conf_data.set('AER_ERR_SENSOR_PRIORITY', 1)
else
    conf_data.set('AER_ERR_SENSOR_PRIORITY', 0)
endif

if get_option('per-lan-error-count-priority')
    conf_data.set('PER_LANE_ERROR_COUNT_PRIORITY', 1)
else
    conf_data.set('PER_LANE_ERROR_COUNT_PRIORITY', 0)
endif

if get_option('error-injection-priority')
    conf_data.set('ERROR_INJECTION_PRIORITY', 1)
else
    conf_data.set('ERROR_INJECTION_PRIORITY', 0)
endif

if get_option('confidential-compute-mode')
    conf_data.set('CONFIDENTIAL_COMPUTE_MODE_PRIORITY', 1)
endif

if get_option('confidential-compute-devtool-mode')
    conf_data.set('CONFIDENTIAL_COMPUTE_DEVTOOL_MODE_PRIORITY', 1)
endif

if get_option('accelerator-dbus').allowed()
    conf_data.set('ACCELERATOR_DBUS', 0)
endif

conf_data.set('LOCAL_EID', get_option('local-eid'))

if get_option('enable-in-kernel-mctp').enabled()
    conf_data.set('MCTP_IN_KERNEL', 0)
endif

conf_data.set(
    'DELAY_BETWEEN_CONCURRENT_REQUESTS',
    get_option('delay-between-concurrent-requests'),
)

if get_option('fixup-missing-event-notification').enabled()
    conf_data.set('FIXUP_MISSING_EVENT_NOTIFICATION', 1)
endif

configure_file(output: 'config.h', configuration: conf_data)

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
    nvidia_shmem = dependency('nvidia-tal', required: true)
    deps += nvidia_shmem
endif

systemd = dependency('systemd')
systemd_system_unit_dir = systemd.get_variable(
    pkgconfig: 'systemdsystemunitdir',
)

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

install_subdir(
    'libnsm/instance-db',
    install_mode: 'r--r--r--',
    install_dir: get_option('datadir') / meson.project_name(),
)
