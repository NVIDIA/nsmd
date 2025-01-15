sources += [
    'nsmSetAsync/asyncOperationManager.cpp',
    'nsmSetAsync/nsmSetErrorInjection.cpp',
    'nsmSetAsync/nsmSetMigMode.cpp',
    'nsmSetAsync/nsmSetECCMode.cpp',
    'nsmSetAsync/nsmSetCpuOperatingConfig.cpp',
    'nsmSetAsync/nsmSetWriteProtected.cpp',
    'nsmSetAsync/nsmSetEgmMode.cpp',
    'nsmSetAsync/nsmAsyncSensor.cpp',
]

nsmd_headers += ['.', '..', 'nsmSetAsync']

if get_option('tests').enabled()
    subdir('test')
endif
