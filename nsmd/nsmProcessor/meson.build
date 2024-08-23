sources += [
    'nsmProcessor/nsmProcessor.cpp',
    'nsmProcessor/nsmFpgaProcessor.cpp',
    'nsmProcessor/nsmReconfigPermissions.cpp',
    'nsmProcessor/nsmPowerSmoothing.cpp',
    'nsmProcessor/nsmReset.cpp',
]

nsmd_headers += ['.', '..', 'nsmDbusIfaceOverride']

if get_option('tests').enabled()
    subdir('test')
endif