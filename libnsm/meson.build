# libnsm static library
# This library contains all the common C sources used by multiple binaries
# (nsmtool, nsmMockupResponder)

libnsm_sources = files(
    'base.c',
    'debug-token.c',
    'device-capability-discovery.c',
    'device-configuration.c',
    'diagnostics.c',
    'firmware-utils.c',
    'network-ports.c',
    'pci-links.c',
    'platform-environmental.c',
    'powersmoothing-powerprofile-api-v2.c',
    'requester/mctp.c',
)

libnsm_headers = ['.', '..', '../common']

libnsm_static = static_library(
    'nsm',
    libnsm_sources,
    include_directories: include_directories(libnsm_headers),
)

# Export as dependency for other meson.build files
libnsm_dep = declare_dependency(
    link_with: libnsm_static,
    include_directories: include_directories(libnsm_headers),
)

