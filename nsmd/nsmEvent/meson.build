sources += [
    'nsmEvent/nsmLongRunningEventHandler.cpp',
    'nsmEvent/nsmLongRunningEvent.cpp',
]

include_all_events = get_option('nsm-event-all')
include_event_list = get_option('nsm-event-include')

valid_events = [
    'rediscovery',
    'xid',
    'threshold',
    'reset-required',
    'fabric-manager-state',
    'gpio-state',
]
foreach name : include_event_list
    if not valid_events.contains(name)
        warning('Unknown nsm-event-include: ' + name)
    endif
endforeach

if include_all_events or include_event_list.contains('rediscovery')
    sources += ['nsmEvent/nsmRediscoveryEvent.cpp']
endif
if include_all_events or include_event_list.contains('xid')
    sources += ['nsmEvent/nsmXIDEvent.cpp']
endif
if include_all_events or include_event_list.contains('threshold')
    sources += ['nsmEvent/nsmThresholdEvent.cpp']
endif
if include_all_events or include_event_list.contains('reset-required')
    sources += ['nsmEvent/nsmResetRequiredEvent.cpp']
endif
if include_all_events or include_event_list.contains('fabric-manager-state')
    sources += ['nsmEvent/nsmFabricManagerStateEvent.cpp']
endif
if include_all_events or include_event_list.contains('gpio-state')
    sources += ['nsmEvent/nsmGPIOStateChangeEvent.cpp']
endif

nsmd_headers += ['.', '..', 'nsmEvent']

if get_option('tests').enabled()
    subdir('test')
endif
