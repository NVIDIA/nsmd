# Build configuration for GPU telemetry tools

# Server executable
gpu_server = executable('gpu-server',
    'gpu-server.cpp',
    dependencies: [
        gpu_telemetry_dep,
        stdexec_dep,
        threads_dep
    ],
    install: true,
    install_dir: get_option('bindir')
)

# Client command line tool
gpu_client = executable('gpu-client',
    'gpu-client.cpp',
    dependencies: [
        gpu_telemetry_dep,
        stdexec_dep,
        threads_dep
    ],
    install: true,
    install_dir: get_option('bindir')
)

# Set permissions for installed executables
meson.add_install_script('sh', '-c',
    'chmod 755 "$DESTDIR/@0@/@1@"'.format(
        get_option('prefix'),
        get_option('bindir')
    )
)
