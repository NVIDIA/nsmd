dep_src_files = [
    '../nsmTemp.cpp',
    '../nsmPower.cpp',
    '../nsmPeakPower.cpp',
    '../nsmEnergy.cpp',
    '../nsmVoltage.cpp',
    '../nsmThreshold.cpp',
    '../nsmThresholdValue.cpp',
    '../nsmAltitudePressure.cpp',
    '../nsmTempAggregator.cpp',
    '../nsmPowerAggregator.cpp',
    '../nsmPeakPowerAggregator.cpp',
    '../nsmEnergyAggregator.cpp',
    '../nsmVoltageAggregator.cpp',
    '../nsmThresholdAggregator.cpp',
    '../nsmNumericSensorFactory.cpp',
    '../nsmNumericSensorComposite.cpp',
    '../nsmThresholdFactory.cpp',
    '../nsmNumericSensor.cpp',
    '../nsmNumericAggregator.cpp',
    '../../nsmDevice.cpp',
    '../../nsmObjectFactory.cpp',
    '../../nsmSensorAggregator.cpp',
    '../../sensorManager.cpp',
    '../../deviceManager.cpp',
    '../../nsmSensor.cpp',
    '../../nsmEvent.cpp',
    '../../deviceManager.cpp',
    '../../nsmEvent/nsmLongRunningEventDispatcher.cpp',
    '../../../common/utils.cpp',
    '../../../common/test/mockDBusHandler.cpp',
    '../../../libnsm/base.c',
    '../../../libnsm/platform-environmental.c',
    '../../../libnsm/instance-id.c',
    '../../../libnsm/requester/mctp.c',
    '../../../requester/request_timeout_tracker.cpp',
]

dep_src_headers = [
    '.',
    '..',
    '../../',
    '../../nsmEvent',
    '../../test/',
    '../../../',
    '../../../common',
    '../../../libnsm',
]

test_src = declare_dependency(
    sources: dep_src_files,
    include_directories: dep_src_headers,
)

tests_files = [
    'nsmNumeric_test.cpp',
    'nsmNumericAggregator_test.cpp',
    'nsmNumericSensors_test.cpp',
    'nsmNumericAggregatorSensors_test.cpp',
]

tests_deps = [
    test_src,
    CLI11_dep,
    nlohmann_json,
    sdbusplus,
    sdeventplus,
    phosphor_logging,
    gtest,
    gmock,
]

if get_option('shmem').enabled()
    tests_deps += nvidia_shmem
endif

test(
    'nsmNumericSensors_test',
    executable(
        'nsmNumericSensors_test',
        tests_files,
        implicit_include_directories: false,
        link_args: dynamic_linker,
        build_rpath: '',
        dependencies: tests_deps,
    ),
    workdir: meson.current_source_dir(),
)