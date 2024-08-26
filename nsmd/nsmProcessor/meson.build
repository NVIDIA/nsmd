sources += [
    'nsmProcessor/nsmProcessor.cpp',
    'nsmProcessor/nsmFpgaProcessor.cpp',
    'nsmProcessor/nsmReconfigPermissions.cpp',
    'nsmProcessor/nsmPowerSmoothing.cpp',
    'nsmProcessor/nsmWorkloadPowerProfile.cpp',
    'nsmProcessor/nsmReset.cpp',
]

nsmd_headers += ['.', '..', 'nsmProcessor']

if get_option('tests').enabled()
    subdir('test')
endif