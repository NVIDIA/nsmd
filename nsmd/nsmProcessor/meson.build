sources += [
    'nsmProcessor/nsmProcessor.cpp',
    'nsmProcessor/nsmFpgaProcessor.cpp',
    'nsmProcessor/nsmReconfigPermissions.cpp',
    'nsmProcessor/nsmReset.cpp',
]

nsmd_headers += ['.', '..']

if get_option('tests').enabled()
    subdir('test')
endif