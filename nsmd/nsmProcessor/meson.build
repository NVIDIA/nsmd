sources += [
    'nsmProcessor/nsmProcessor.cpp',
    'nsmProcessor/nsmFpgaProcessor.cpp',
    'nsmProcessor/nsmReconfigPermissions.cpp',
    'nsmProcessor/nsmPowerSmoothing.cpp',
    'nsmProcessor/nsmReset.cpp',
]

nsmd_headers += ['.', '..']

if get_option('tests').enabled()
    subdir('test')
endif