sources += [
    'nsmSetAsync/asyncOperationManager.cpp',
    'nsmSetAsync/nsmAsyncSensor.cpp',
]

include_all_set_async = get_option('nsm-set-async-all')
include_set_async_ops = get_option('nsm-set-async-include')

valid_set_async_ops = [
    'error-injection',
    'mig-mode',
    'ecc-mode',
    'cpu-operating-config',
    'write-protected',
    'egm-mode',
]
foreach name : include_set_async_ops
    if not valid_set_async_ops.contains(name)
        warning('Unknown nsm-set-async-include: ' + name)
    endif
endforeach

if include_all_set_async or include_set_async_ops.contains('error-injection')
    sources += ['nsmSetAsync/nsmSetErrorInjection.cpp']
endif
if include_all_set_async or include_set_async_ops.contains('mig-mode')
    sources += ['nsmSetAsync/nsmSetMigMode.cpp']
endif
if include_all_set_async or include_set_async_ops.contains('ecc-mode')
    sources += ['nsmSetAsync/nsmSetECCMode.cpp']
endif
if include_all_set_async or include_set_async_ops.contains(
    'cpu-operating-config',
)
    sources += ['nsmSetAsync/nsmSetCpuOperatingConfig.cpp']
endif
if include_all_set_async or include_set_async_ops.contains('write-protected')
    sources += ['nsmSetAsync/nsmSetWriteProtected.cpp']
endif
if include_all_set_async or include_set_async_ops.contains('egm-mode')
    sources += ['nsmSetAsync/nsmSetEgmMode.cpp']
endif

nsmd_headers += ['.', '..', 'nsmSetAsync']

