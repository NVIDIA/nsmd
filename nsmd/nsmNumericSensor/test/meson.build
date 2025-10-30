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
    '../../nsmSensor.cpp',
    '../../nsmEvent.cpp',
    '../../nsmProgressCounters/progressCounters.cpp',
    '../../nsmProgressCounters/counterProducer.cpp',
    '../../nsmEvent/nsmLongRunningEventHandler.cpp',
    '../../../common/utils.cpp',
    '../../../common/test/mockDBusHandler.cpp',
    '../../../libnsm/base.c',
    '../../../libnsm/platform-environmental.c',
    '../../../libnsm/requester/mctp.c',
    '../../../requester/request_timeout_tracker.cpp',
    '../../nsmFwSwInventory/GPUSWInventory.cpp',
    '../../nsmDbusIfaceOverride/nsmAssetIntf.cpp',
    '../../../requester/mctp_endpoint_discovery.cpp',
]

if get_option('shmem').enabled()
    dep_src_files += '../../nsmCommon/sharedMemCommon.cpp'
endif

dep_src_headers = [
    '.',
    '..',
    '../../',
    '../../nsmEvent',
    '../../nsmCommon',
    '../../test/',
    '../../nsmDbusIfaceOverride',
    '../../nsmProgressCounters',
    '../../../',
    '../../../common',
    '../../../libnsm',
]

test_src = declare_dependency(
    sources: dep_src_files,
    include_directories: dep_src_headers,
)

tests = [
    'nsmNumeric_test',
    'nsmNumericAggregator_test',
    'nsmNumericSensors_test',
    'nsmNumericAggregatorSensors_test',
    'nsmNumericSensorValueAggregate_test',
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

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            implicit_include_directories: false,
            link_args: dynamic_linker,
            build_rpath: '',
            dependencies: tests_deps,
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
