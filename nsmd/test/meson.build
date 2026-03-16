tests = [
    'circularQueue_test',
    'eventHandler_test',
    'eventTypeHandlers_test',
    'instance_id_test',
    'interfaceWrapper_test',
    'nsmDeviceAndSwitchPowerSmoothing_test',
    'nsmSensorManagerAndClockControl_test',
    'nsmProcessorAndSwitchPower_test',
    'nsmKeyMgmtAndDebugToken_test',
    'nsmDeviceAddSensorTemplate_test',
    'nsmSensorManagerAndDeviceIO_test',
    'nsmLongRunningSensorAndDebugInfo_test',
    'nsmPortAndNetworkAdapterConfig_test',
    'nsmSensorAggregatorAndThreshold_test',
    'nsmGroupSensorAndMemoryCapacity_test',
    'nsmAsyncSensorAndEgmMode_test',
    'nsmSensorEquality_test',
    'nsmSensorAggregatorHandleResponse_test',
    'nsmDevice_test',
    'nsmObject_test',
    'nsmTypeNameCoverage_test',
    'nsmMoveInterfacesCoverage_test',
    'nsmObjectFactory_test',
    'requester_test',
    'sensorQueueMap_test',
    # 'socket_handler_test',  # DISABLED: Requires event loop mock infrastructure (blocked, see SOCKET-MOCKING-BLOCKED-TESTS.md)
    'stateChangeLogger_test',
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
            build_rpath: '',
            dependencies: tests_deps,
        ),
        workdir: meson.current_source_dir(),
    )
endforeach
