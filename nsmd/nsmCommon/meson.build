sources += [
    'nsmCommon/nsmCommon.cpp',
    'nsmCommon/nsmPcieGroup.cpp',
    'nsmCommon/nsmPciePortIntf.cpp',
]

if get_option('shmem').enabled()
    sources += 'nsmCommon/sharedMemCommon.cpp'
endif

nsmd_headers += ['.', '..', 'nsmCommon']

if get_option('tests').enabled()
    subdir('test')
endif
