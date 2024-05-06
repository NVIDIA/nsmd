# nsmd link topology generation

### Processing Link Topology
Leverage Entity-Manager runtime JSON driven system configuration manager.
Device inventory will be defined in a seperate EM json configuration file and link topology details will be defined in a seperate EM json file [like in sample config json hgxb_nvlink_topology.json].

### Topology generation flow
1. Have a link connection mapping in an excel sheet.
2. Use the nvlink_topology_processor.py to generate Entity manager configuration json file from the provided Excel.
3. NSMD uses the dbus objects exposed by Entity manager while creating objects for links.
4. The python script expects two parameters: input mapping excel and output json file name.

### Mapping Excel Requirement
Link mapping excel is expected in below mentioned format. NOTE: Any variation from the mentioned format may give error while running the nvlink_topology_processor.py
A sample mapping excel is given in tools/ (HGXB_NVLink_Mapping.xlsx)

1. Table 1: For each link there are 2 endpoints, for both the endpoints below mentioned data is required. (in column A:F)
    - LeftDeviceInstanceID      : ID of first connected device [Column A]
    - LeftDeviceType    : Type of the fist connected device [Column B]
    - LeftDevicePortID  : Physical port ID of the first connected device [Column C]
    - RightDeviceInstanceID     : ID of the second connected device [Column D]
    - RightDeviceType   : Type of the second connected device [Column E]
    - RightDevicePortID : Physical port ID of the second connected device [Column F]

    [NOTE:
        - DeviceType is all-caps and possible values are "GPU", "SWITCH", "PCIE_BRIDGE" and "CPU".
        - Entry in this mapping table is per link (having 2 devices at each end) not per device port.]

2. Table 2: To give information about each device type is required. (in column H:J)
    - DeviceType        : Indicating type of device GPU, Switch, PCIe Bridge or CPU [Column H]
    - DbusInventoryPath : Dbus object path of the device (on which port objects are created) [Column I]
    - Probe          : Probe for each device type, based on this probe EM will expose dbus topology object for corresponding device [Column J]

    [NOTE:
        - DbusInventoryPath should have $INSTANCE_NUMBER which will be replaced with the device ID available in column A & D.]

3. Table 3: To give information about link prefix used on dbus object. (in column L:N)
    - Device1         : For a link, type of first connected device [Column L]
    - Device2         : For a link, type of second connected device [Column M]
    - LinkPrefix      : Prefix to be used while creating the dbus object names for the link ports [Column N]

    [NOTE:
        - Each link has 2 ends, so when device type in col:L is connected to device type in col:M link prefix in col:N is to be used.]

### Possible connections between devices
1. GPU <--link--> Switch
2. Switch <--link--> PCIe Bridge
3. Switch <--link--> Switch
4. GPU <--link--> GPU
5. GPU <--link--> CPU
6. GPU <--link--> '-' This csae is when GPU is connected to an external device.
7. and other connections if possible are handled by the script

[NOTE:
    - When we have external device connected on one end then put the placeholder '-' in column D, E & F.]

### usage of EM configuration for nsmd
#### Comman field in EM config for each device
| Configuration Property 	| type   	| Description                                          	            |
|------------------------	|--------	|-----------------------------------------------------------------	|
| Type                   	| string 	| NVLinkTopology                                                    |
| Name                   	| string 	| Name for EM dbus object, which is same as device name.           	|
| Count                  	| int    	| The total port count on the device.<br>example: if Count=4 and LinkPrefix="Port", then four state.PortMetric PDIs will be populated to<br>/xyz/openbmc_project/.../Ports/Port_0<br>/xyz/openbmc_project/.../Ports/Port_1<br>/xyz/openbmc_project/.../Ports/Port_2<br>/xyz/openbmc_project/.../Ports/Port_3 	|
| Topology                  | -        	| Defining details about each port on device. [0 to count-1]   	     |

#### Topology
| Configuration Property 	| type   	| Description                                          	                    |
|------------------------	|--------	|-------------------------------------------------------------------------	|
| InventoryObjPath        	| string 	| Dbus object path of the current device port.                                   |
| LogicalPortNumber         | int    	| Logical port Id of the device as created by the script.                                           |
| Associations       	    | array of string 	| Associations {forward, backward, absolutePath} to be added on InventoryObjPath.   |

### How to run the script

Format: python nvlink_topology_processor.py <input_mapping_file> <output_json_file>
```
python3 nvLink_topology_processor.py HGXB_NVLink_Mapping.xlsx hgxb_nvlink_topology.json
```