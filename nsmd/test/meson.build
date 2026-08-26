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
    'sensorManager_test',
    'nsmObject_test',
    'nsmTypeNameCoverage_test',
    'nsmMoveInterfacesCoverage_test',
    'nsmObjectFactory_test',
    'requester_test',
    'requesterHandlerBranch_test',
    'sensorQueueMap_test',
    'socket_handler_test',
    'stateChangeLogger_test',
    'nsmMsgTypesSensor_test',
    'nsmServiceReadyInterface_test',
    'nsmNullDeviceBranch_test',
    'sensorManagerBranch_test',
    'requesterDeepBranch_test',
    'mctpEndpointProberBranch_test',
    'stateChangeLoggerBranch_test',
    'stateChangeLoggerBranch2_test',
    'stateChangeLoggerBranch3_test',
    'nsmDeviceDI_test',
    'nsmDeviceDI2_test',
    'nsmDeviceDI3_test',
    'nsmDeviceBranch_test',
    'sensorManagerBranch2_test',
    'nsmDeviceBranch2_test',
    'nsmDeviceBranch3_test',
    'nsmPortHealthMetrics_test',
]

# mctpDiscoveryBranch_test: helper TU isolates mctp_endpoint_discovery.hpp
# from sdbusplus::asio::connection to avoid header interaction crash.
test(
    'mctpDiscoveryBranch_test',
    executable(
        'mctpDiscoveryBranch_test',
        'mctpDiscoveryBranch_test.cpp',
        'mctpDiscoveryBranchHelper.cpp',
        implicit_include_directories: false,
        include_directories: ['.', '..'],
        link_args: dynamic_linker,
        dependencies: tests_deps,
    ),
    workdir: meson.current_source_dir(),
)

# mctpDiscoveryBranch2_test: uses TestableMctpDiscovery (init() overridden
# as no-op) to test all non-D-Bus methods of mctp_endpoint_discovery.cpp.
test(
    'mctpDiscoveryBranch2_test',
    executable(
        'mctpDiscoveryBranch2_test',
        'mctpDiscoveryBranch2_test.cpp',
        'mctpDiscoveryBranch2Helper.cpp',
        implicit_include_directories: false,
        include_directories: ['.', '..'],
        link_args: dynamic_linker,
        dependencies: tests_deps,
    ),
    workdir: meson.current_source_dir(),
)

# mctpDiscoveryBranch3_test: deeper branch coverage for mapNsmDeviceUsingEid,
# mapMctpEIDForNsmDevice, containsValue, handleMctpEndpoints, etc.
test(
    'mctpDiscoveryBranch3_test',
    executable(
        'mctpDiscoveryBranch3_test',
        'mctpDiscoveryBranch3_test.cpp',
        'mctpDiscoveryBranch2Helper.cpp',
        implicit_include_directories: false,
        include_directories: ['.', '..'],
        link_args: dynamic_linker,
        dependencies: tests_deps,
    ),
    workdir: meson.current_source_dir(),
)

# unify_mctp_regression_test: pins observable behaviour of
# requester/mctp_endpoint_discovery.cpp BEFORE the unify-mctp refactor.
# Uses mctpDiscoveryBranch2Helper.cpp to reach private members through the
# MctpDiscoveryTestAccess friend class (same pattern as Branch2/Branch3).
test(
    'unify_mctp_regression_test',
    executable(
        'unify_mctp_regression_test',
        'unify_mctp_regression_test.cpp',
        'mctpDiscoveryBranch2Helper.cpp',
        implicit_include_directories: false,
        include_directories: ['.', '..'],
        link_args: dynamic_linker,
        dependencies: tests_deps,
    ),
    workdir: meson.current_source_dir(),
)

# Test that links against mockSdEvent to enable retry-loop coverage
test(
    'mctpProberRetryBranch_test',
    executable(
        'mctpProberRetryBranch_test',
        'mctpProberRetryBranch_test.cpp',
        '../../common/test/mockSdEvent.cpp',
        implicit_include_directories: false,
        include_directories: ['.', '..'],
        link_args: dynamic_linker + ['-Wl,--allow-multiple-definition'],
        dependencies: tests_deps,
    ),
    workdir: meson.current_source_dir(),
)

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
