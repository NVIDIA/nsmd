sources += [
    'nsmProcessor/nsmProcessor.cpp',
    'nsmProcessor/nsmFpgaProcessor.cpp',
    'nsmProcessor/nsmGpuOperationalStatus.cpp',
    'nsmProcessor/nsmReconfigPermissions.cpp',
    'nsmProcessor/nsmPowerSmoothing.cpp',
    'nsmProcessor/nsmWorkloadPowerProfile.cpp',
    'nsmProcessor/nsmReset.cpp',
    'nsmProcessor/nsmOemResetStatistics.cpp',
]

nsmd_headers += ['.', '..', 'nsmProcessor']

if get_option('tests').enabled()
    subdir('test')
endif
