# NSM device and PLDM sensor association

### Overview
Some telemetry in case of NVSwitch and NetworkAdapter is coming from PLDM and some from NSM. Hence there is a requirement to associate/correlate NSM device with PLDM terminus. Once devices are correlated, then association between PLDM sensorId must be set with NSM objects. To achieve this requirement we have below mentioned configuration PDI exposed from EM.

### Device association flow
1. Expose Configuration.NSMDeviceAssociation PDI with UUID of the device. Which will help PLDM to correlate NSM device and PLDM terminus based on UUID.
2. Expose Configuration.SensorAuxName PDI with sensorId and AuxNames to get desired name in dbus object in PLDM. Once device correlation has happened this PDI can be exposed to provide auxillary name for any sensorID dbus object.
3. Expose Configuration.SensorPortInfo PDI with sensorId and other port information which is to be derived from EM congifguration json.

#### NSMDeviceAssociation PDI
| Configuration Property 	| type   	| Description                                          	            |
|------------------------	|--------	|-----------------------------------------------------------------	|
| Type                   	| string 	| NsmDeviceAssociation                                              |
| Name                   	| string 	| Name for EM dbus object.                                         	|
| UUID                  	| string   	| UUID of the device which will be used by PLDM for correlation.    |

Example:
```
{
    "Name": "HGX_NVSwitch_$INSTANCE_NUMBER_Association",
    "Type": "NsmDeviceAssociation",
    "UUID": "$UUID"
}
```

#### SensorAuxName PDI
| Configuration Property 	| type   	| Description                                          	            |
|------------------------	|--------	|-----------------------------------------------------------------	|
| Type                   	| string 	| SensorAuxName                                                     |
| Name                   	| string 	| Name for EM dbus object                                          	|
| SensorId                 	| int    	| Sensor Id in PLDM for which auxillary name is provided.           |
| AuxNames                 	| strings  	| Auxillary name to used on dbus object for Sensor Id in PLDM.      |

Example:
```
{
    "Name": "HGX_NVSwitch_$INSTANCE_NUMBER_AUX",
    "Type": "SensorAuxName",
    "SensorId": 10,
    "AuxNames": [
        "HGX_NVSwitch_$INSTANCE_NUMBER"
    ]
}
```

#### SensorPortInfo PDI
| Configuration Property 	| type   	| Description                                          	            |
|------------------------	|--------	|-----------------------------------------------------------------	|
| Type                   	| string 	| SensorPortInfo                                                    |
| Name                   	| string 	| Name for EM dbus object                                          	|
| SensorId                 	| int    	| Sensor Id in PLDM for which auxillary name is provided.           |
| PortType                 	| string 	| Indicating the type of port, value must be from enum in PDI "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType"   |
| MaxSpeedMBps             	| int    	| Max speed in MBps for the particular port.                        |
| Association              	| strings 	| Association with the NSM port object, array of string in format 'forward', 'reverse' and 'association_path'.              |

Example:
```
{
    "Name": "NVSwitch_$INSTANCE_NUMBER_Port_0_Info",
    "Type": "SensorPortInfo",
    "SensorId": 5500,
    "PortType": "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.BidirectionalPort",
    "MaxSpeedMBps": 50000,
    "Association": [
        "associated_port",
        "associated_port",
        "/xyz/openbmc_project/inventory/system/fabrics/HGX_NVLinkFabric_0/Switches/NVSwitch_$INSTANCE_NUMBER/Ports/NVLink_0"
    ]
}
```

#### Important to note
1. For SensorAuxName and SensorPortInfo, there is a simple python script which will generate the json based on a excel sheet. This helps in avoiding writting the json for so many sensorID in PLDM. Excel sheet contails a table [column A:D] with below fields:
    - SensorID      : Sensor ID as per the PLDM_DB_MODEL for NVSwitch & NIC device [Column A]
    - PortType      : Port type of the port object with above sensorId [Column B]
    - DeviceType    : Device type of current device [Column C]
    - DbusInventoryPath    : Corresponding NSM port object for the given sensorId in PLDM [Column D]

    [NOTE:
        - DeviceType is all-caps and possible values are "SWITCH" and "PCIE_BRIDGE", since PLDM is only supported on these 2 devices.
        - After we have generated json, copy the config to hgxb_cx_chassis.json and hgxb_nvlink_chassis.json for NVSwicth and NIC manually]

2. For PLDM device association to work, ensure below points in regards to EM dbus object:
    - Parent dbus object must implement "xyz.openbmc_project.Inventory.Item.Board" PDI.
    - Parent object must also implement "xyz.openbmc_project.Inventory.Decorator.Instance" PDI with "InstanceNumber" as 1.
    - "Type" of parent object must be "chassis" {case-sensitive}.
    - "Name" of parent object must be same as NSM chassis object name, which means name in expose with type as NSM_NVLinkMgmtNic_Chassis or NSM_NVSwitch_Chassis must be same as parent name.
    - Entity manager object for HGX_NVSwitch_INSTANCE_NUMBER and HGX_NVLinkManagementNIC_INSTANCE_NUMBER should not contain Asset, chassis, UUID, etc information since all such information will be available from NSM object.

Example:
```
[
  {
    "Exposes": [
      {
        "Name": "HGX_NVLinkManagementNIC_0",
        "Type": "NSM_NVLinkMgmtNic_Chassis",
        "UUID": "$UUID",
        "Asset": {
          "Type": "NSM_Asset",
          "Manufacturer": "NVIDIA",
          "Model": "$MARKETING_NAME",
          "SKU": "SKU_000123",
          "SerialNumber": "$SERIAL_NUMBER",
          "PartNumber": "$BOARD_PART_NUMBER"
        },
        "Location": {
          "Type": "NSM_Location",
          "LocationType": "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"
        },
        "Chassis": {
          "Type": "NSM_Chassis",
          "ChassisType": "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Component"
        },
        "Health": {
          "Type": "NSM_Health",
          "Health": "xyz.openbmc_project.State.Decorator.Health.HealthType.OK"
        }
      },
      {
        "ChassisName": "HGX_NVLinkManagementNIC_0",
        "Name": "Assembly1",
        "Type": "NSM_NVLinkMgmtNic_ChassisAssembly",
        "UUID": "$UUID",
        "Asset": {
          "Type": "NSM_Asset",
          "Name": "NVLinkManagementNIC Board Assembly",
          "Model": "$MARKETING_NAME",
          "Vendor": "NVIDIA",
          "SKU": "SKU_000123",
          "SerialNumber": "$SERIAL_NUMBER",
          "PartNumber": "$BOARD_PART_NUMBER",
          "ProductionDate": "$BUILD_DATE"
        },
        "Health": {
          "Type": "NSM_Health",
          "Health": "xyz.openbmc_project.State.Decorator.Health.HealthType.OK"
        },
        "Location": {
          "Type": "NSM_Location",
          "LocationType": "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"
        }
      },
      {
        "Name": "HGX_NVLinkManagementNIC_0_Association",
        "Type": "NsmDeviceAssociation",
        "UUID": "$UUID"
      },
      {
        "Name": "HGX_NVLinkManagementNIC_0_AUX",
         "Type": "SensorAuxName",
         "SensorId": 5,
         "AuxNames": [
            "HGX_NVLinkManagementNIC_0"
         ]
      },
      {
        "Name": "NVLinkManagementNIC_0_AUX",
         "Type": "SensorAuxName",
         "SensorId": 60,
         "AuxNames": [
            "NVLinkManagementNIC_0"
         ]
      }
    ],
    "Name": "HGX_NVLinkManagementNIC_0",
    "Type": "chassis",
    "Parent_Chassis": "/xyz/openbmc_project/inventory/system/chassis/HGX_Chassis_0",
    "Probe": "xyz.openbmc_project.FruDevice({'DEVICE_TYPE': 2})",
    "xyz.openbmc_project.Inventory.Decorator.Location": {
      "LocationType": "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"
    },
    "xyz.openbmc_project.State.Decorator.Health": {
      "Health": "xyz.openbmc_project.State.Decorator.Health.HealthType.OK"
    },
    "xyz.openbmc_project.Inventory.Item.Board": {},
    "xyz.openbmc_project.Inventory.Decorator.Instance": {
      "InstanceNumber": 1
    }
  }
]
```

### How to run the script

Format: python sensorId_association_generator.py <input_mapping_file> <output_json_file>
```
python3 sensorId_association_generator.py HGXB_PLDM_sensorID_mapping.xlsx hgxb_pldmd_sensorID_association.json
```