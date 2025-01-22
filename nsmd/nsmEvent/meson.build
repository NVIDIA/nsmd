sources += [
    'nsmEvent/nsmRediscoveryEvent.cpp',
    'nsmEvent/nsmXIDEvent.cpp',
    'nsmEvent/nsmThresholdEvent.cpp',
    'nsmEvent/nsmResetRequiredEvent.cpp',
    'nsmEvent/nsmFabricManagerStateEvent.cpp',
    'nsmEvent/nsmLongRunningEventHandler.cpp',
    'nsmEvent/nsmLongRunningEvent.cpp',
]

nsmd_headers += ['.', '..', 'nsmEvent']

if get_option('tests').enabled()
    subdir('test')
endif