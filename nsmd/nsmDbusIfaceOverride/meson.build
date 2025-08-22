sources += []

include_all_dbus_override = get_option('nsm-dbus-override-all')
include_dbus_override = get_option('nsm-dbus-override-include')
if get_option('cx8-features')
    include_all_dbus_override = false
    include_dbus_override = ['asset']
endif

valid_dbus_override = ['asset', 'nvlink-topology']
foreach name : include_dbus_override
    if not valid_dbus_override.contains(name)
        warning('Unknown nsm-dbus-override-include: ' + name)
    endif
endforeach

if include_all_dbus_override or include_dbus_override.contains('asset')
    sources += ['nsmDbusIfaceOverride/nsmAssetIntf.cpp']
endif
if include_all_dbus_override or include_dbus_override.contains('nvlink-topology')
    sources += ['nsmDbusIfaceOverride/nsmMNNVLinkTopologyIntf.cpp']
endif

nsmd_headers += ['.', '..', 'nsmDbusIfaceOverride']
