# nsmNumericSensor tests now use nsmd_test
# nsmNumericSensor is CONDITIONAL - only runs if enabled, in which case it's in nsmd_test
# All sources already compiled into nsmd_test, no need to recompile

tests = [
    'nsmNumeric_test',
    'nsmNumericAggregator_test',
    'nsmNumericSensors_test',
    'nsmNumericAggregatorSensors_test',
    'nsmNumericSensorValueAggregate_test',
    'nsmThresholdValue_test',
    'nsmThresholdAggregator_test',
    'nsmPeakPowerAggregator_test',
    'nsmPowerAggregator_test',
    'nsmEnergyAggregator_test',
    'nsmVoltageAggregator_test',
    'nsmNumericSensorFactory_test',
    'nsmThreshold_test',
    'nsmThresholdFactory_test',
    'nsmNumericSensorComposite_test',
    'nsmNumericSensorBranch_test',
    'nsmNumericSensorsBranch_test',
    'nsmThresholdFactoryBranch_test',
    'nsmNumericSensorFactoryBranch_test',
    'nsmThresholdEvaluator_test',
]

foreach t : tests
    test(
        t,
        executable(
            t.underscorify(),
            t + '.cpp',
            implicit_include_directories: false,
            include_directories: ['.', '..'],
            link_args: dynamic_linker,
            dependencies: tests_deps,
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
