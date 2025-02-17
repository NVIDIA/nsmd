# GPU Telemetry build configuration
project_name = 'gpu-telemetry'

# Dependencies
stdexec_dep = dependency('stdexec', required: true)
threads_dep = dependency('threads', required: true)
libnsm_dep = dependency('libnsm', required: true)

# Compiler flags
cpp_args = [
    '-DVERSION_MAJOR=1',
    '-DVERSION_MINOR=0',
    '-DVERSION_PATCH=0',
]

if get_option('buildtype') == 'debug'
    cpp_args += ['-DDEBUG']
endif

# Library sources
lib_sources = [
    'src/client/client.cpp',
    'src/client/ipc.cpp',
    'src/server/server.cpp',
    'src/server/mock_device.cpp',
]

# Library headers
lib_headers = [
    'include/gpu-telemetry/client.h',
    'include/gpu-telemetry/types.h',
    'include/gpu-telemetry/error.h',
]

# Build shared library
gpu_telemetry_lib = shared_library(project_name,
    lib_sources,
    include_directories: include_directories('include'),
    dependencies: [
        stdexec_dep,
        threads_dep,
        libnsm_dep,
    ],
    cpp_args: cpp_args,
    version: '1.0.0',
    install: true,
)

# Create dependency object for internal use
gpu_telemetry_dep = declare_dependency(
    link_with: gpu_telemetry_lib,
    include_directories: include_directories('include'),
    dependencies: [
        stdexec_dep,
        threads_dep,
        libnsm_dep,
    ],
)

# Install headers
install_headers(lib_headers, subdir: project_name)

# Build tools
subdir('tools')

# Build examples if enabled
if get_option('build_examples')
    subdir('examples')
endif

# Build and run tests if enabled
if get_option('build_tests')
    subdir('test')
endif
