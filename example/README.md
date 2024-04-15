# nsmd configuration file for EM

### Creating Device Inventory
Leverage Entity-Manager runtime JSON driven system configuration manager.
Platform developer maintains a json file to expose necessary configuration
data for nsmd. The configuration data can be similar to what properties
defined in oob_manifest_pcie_vulcan.json for nsmd to knows the device
configurations for further initialization.

### usage of EM configuration for nsmd

#### NSM_NVLink
| Configuration Property 	| type   	| Description                                          	                   |
|------------------------	|--------	|------------------------------------------------------------------------- |
| Type                   	| string 	| NSM_NVLink                                                               |
| Name                   	| string 	| Name for EM dbus object                                                  |
| ParentObjPath            	| string 	| The object path of the parent device.                                    |
| InventoryObjPath      	| string 	| The object path for dbus object creation.                                |
| Count                  	| int    	| The total port number of the device.<br>example: if Count=4 and Name="Port", then four state.PortMetric PDIs will be populated to<br>/xyz/openbmc_project/.../Ports/Port_0<br>/xyz/openbmc_project/.../Ports/Port_1<br>/xyz/openbmc_project/.../Ports/Port_2<br>/xyz/openbmc_project/.../Ports/Port_3 	|
| UUID                   	| string 	| The UUID of device. used for lookup the assigned EID                     |
| Priority                  | bool   	| Indicate the sensor updated in priority                	               |

Device inventory will be defined in a seperate EM json configuration file [like in sample json Umbriel_NSM_device.json] and NVLink port topology details will be defined in a seperate EM json file [like in sample config Umbriel_NSM_Topology.json].