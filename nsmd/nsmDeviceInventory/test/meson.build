tests = [
    'nsmDeviceInventory_test',
    'nsmNetworkAdapter_test',
    'nsmSwitch_extended_test',
    'nsmPCIeRetimerFabricsDI_test',
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
