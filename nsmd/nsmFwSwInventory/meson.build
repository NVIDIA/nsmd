sources += [
    'nsmFwSwInventory/NVLinkManagementNICSWInventory.cpp',
    'nsmFwSwInventory/GPUSWInventory.cpp',
    'nsmFwSwInventory/nsmFirmwareInventory.cpp',
    'nsmFwSwInventory/nsmWriteProtectedControl.cpp',
]

nsmd_headers += ['.', '..','nsmDbusIfaceOverride']

if get_option('tests').enabled()
    subdir('test')
endif