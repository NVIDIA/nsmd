import pandas as pd
import json
import sys

#########################################################################
# Excel config and mapping of row and col
# Define constants for link details parsing
LINK_DETAILS_COLUMNS = "A:F"
LINK_DETAILS_HEADER_ROW = 1

# Define constants for device details parsing
DEVICE_DETAILS_COLUMNS = "H:J"
DEVICE_DETAILS_HEADER_ROW = 1

# Define constants for prefix details parsing
PREFIX_DETAILS_COLUMNS = "L:N"
PREFIX_DETAILS_HEADER_ROW = 1

#########################################################################
# Store device details in dictionary
device_inventory_paths = {}
device_probe = {}

# Store link connection details in dictionary
topology_data = {}

# Keeping track of logical port ID [1 based] and physical port ID
device_physical_to_logical_port = {}

# Generating final EM json config
final_config = []

# Store prefix details in a dictionary
prefix_data = {
    "GPU": {
        "GPU": "None",
        "SWITCH": "None",
        "PCIE_BRIDGE": "None",
        "CPU": "None",
        "EXTERNAL": "None"
    },
    "SWITCH": {
        "GPU": "None",
        "SWITCH": "None",
        "PCIE_BRIDGE": "None",
        "CPU": "None",
        "EXTERNAL": "None"
    },
    "PCIE_BRIDGE": {
        "GPU": "None",
        "SWITCH": "None",
        "PCIE_BRIDGE": "None",
        "CPU": "None",
        "EXTERNAL": "None"
    },
    "CPU": {
        "GPU": "None",
        "SWITCH": "None",
        "PCIE_BRIDGE": "None",
        "CPU": "None",
        "EXTERNAL": "None"
    },
    "EXTERNAL": {
        "GPU": "None",
        "SWITCH": "None",
        "PCIE_BRIDGE": "None",
        "CPU": "None",
        "EXTERNAL": "None"
    }}

#########################################################################
def getDataFromExcel(input_file):
    device_details_df = pd.read_excel(input_file, engine='openpyxl', usecols=DEVICE_DETAILS_COLUMNS, skiprows=DEVICE_DETAILS_HEADER_ROW-1)
    prefix_details_df = pd.read_excel(input_file, engine='openpyxl', usecols=PREFIX_DETAILS_COLUMNS, skiprows=PREFIX_DETAILS_HEADER_ROW-1)
    link_details_df = pd.read_excel(input_file, engine='openpyxl', usecols=LINK_DETAILS_COLUMNS, skiprows=LINK_DETAILS_HEADER_ROW-1)

    # Get device details
    for index, row in device_details_df.iterrows():
        device_type = str(row['DeviceType']).upper()

        if "NAN" not in (device_type):
            device_inventory_paths[device_type] = str(row['DbusInventoryPath'])
            device_probe[device_type] = str(row['Probe']).upper()

    # Get prefix details
    for index, row in prefix_details_df.iterrows():
        device_1 = str(row['Device1']).upper()
        device_2 = str(row['Device2']).upper()

        if "NAN" not in (device_1, device_2):
            prefix_data[device_1][device_2] = str(row['LinkPrefix'])

    # Get link details
    for index, row in link_details_df.iterrows():
        left_device_id = int(row['LeftDeviceInstanceID']) if row['LeftDeviceInstanceID'] != '-' else None
        left_device_type = str(row['LeftDeviceType']).upper() if row['LeftDeviceType'] != '-' else "EXTERNAL"
        left_port_int = int(row['LeftDevicePortID']) if row['LeftDevicePortID'] != '-' else None

        right_device_id = int(row['RightDeviceInstanceID']) if row['RightDeviceInstanceID'] != '-' else None
        right_device_type = str(row['RightDeviceType']).upper() if row['RightDeviceType'] != '-' else "EXTERNAL"
        right_port_int = int(row['RightDevicePortID']) if row['RightDevicePortID'] != '-' else None

        if left_device_type != "EXTERNAL" and left_device_type not in topology_data:
            topology_data[left_device_type] = {}
        if right_device_type != "EXTERNAL" and right_device_type not in topology_data:
            topology_data[right_device_type] = {}

        # Generate link data for the left side device of the link
        if left_port_int is not None:
            link_data_for_left_device = {
                'connected_device_id': right_device_id,
                'connected_device_type': right_device_type,
                'connected_device_port_id': right_port_int,
                'self_port_id': left_port_int
            }
            topology_data[left_device_type].setdefault(left_device_id, []).append(link_data_for_left_device)

        # Generate link data for the right side device of the link
        if right_port_int is not None:
            link_data_for_right_device = {
                'connected_device_id': left_device_id,
                'connected_device_type': left_device_type,
                'connected_device_port_id': left_port_int,
                'self_port_id': right_port_int
            }
            topology_data[right_device_type].setdefault(right_device_id, []).append(link_data_for_right_device)

    # For debug purpose only---------------------------------------------
    # json_data = json.dumps(topology_data, indent=4)
    # with open('topology_data.json', 'w', encoding='utf-8') as json_file:
    #     json_file.write(json_data)

    # json_data = json.dumps(prefix_data, indent=4)
    # with open('prefix_data.json', 'w', encoding='utf-8') as json_file:
    #     json_file.write(json_data)
    # For debug purpose only---------------------------------------------

#########################################################################
def sortTopologyData():
    # Sort the link data based on the physical port Id [self_port_id]
    for device_type, all_device_data in topology_data.items():
        for device_id in all_device_data.keys():
            all_device_data[device_id] = sorted(all_device_data[device_id], key=lambda x: x['self_port_id'])

    # For debug purpose only---------------------------------------------
    # json_data = json.dumps(topology_data, indent=4)
    # with open('sorted_topology_data.json', 'w', encoding='utf-8') as json_file:
    #     json_file.write(json_data)
    # For debug purpose only---------------------------------------------

#########################################################################
def getPhysicalToLogicalMapping():
    for self_device_type, all_device_data in topology_data.items():
        port_mapping = {}
        for self_device_id in all_device_data.keys():
            device_map = {}
            port_count_tracker = {"GPU": 0, "SWITCH": 0, "PCIE_BRIDGE": 0, "CPU": 0, "EXTERNAL": 0}
            logical_id = 1

            for link_data in all_device_data[self_device_id]:
                # Get the port ID and assign the logical_id to it
                self_port_id = link_data['self_port_id']
                connected_device_type = link_data['connected_device_type']

                device_map[self_port_id] = (
                    logical_id,
                    f"{prefix_data[self_device_type][connected_device_type]}_{port_count_tracker[connected_device_type]}"
                )

                port_count_tracker[connected_device_type] += 1
                logical_id += 1

            port_mapping[self_device_id] = device_map
        device_physical_to_logical_port[self_device_type] = port_mapping

    # For debug purpose only---------------------------------------------
    # json_data_next = json.dumps(device_physical_to_logical_port, indent=4)
    # with open('device_physical_to_logical_port.json', 'w', encoding='utf-8') as json_file:
    #     json_file.write(json_data_next)
    # For debug purpose only---------------------------------------------

#########################################################################
def getAssociations(self_device_type, connected_device_type, connected_device_path, connected_port_path):
    association = []
    if self_device_type == "GPU":
        if connected_device_type == "SWITCH":
            association.extend((
                "associated_switch",
                "connected_port",
                connected_device_path
            ))
            association.extend((
                "switch_port",
                "processor_port",
                connected_port_path
            ))
    elif self_device_type == "SWITCH":
        if connected_device_type == "GPU":
            association.extend((
                "associated_endpoint",
                "connected_port",
                connected_device_path
            ))
            association.extend((
                "processor_port",
                "switch_port",
                connected_port_path
            ))
        elif connected_device_type == "SWITCH":
            association.extend((
                "associated_switch",
                "connected_port",
                connected_device_path
            ))
            association.extend((
                "switch_port",
                "switch_port",
                connected_port_path
            ))
        elif connected_device_type == "PCIE_BRIDGE":
            association.extend((
                "associated_pcie_bridge",
                "connected_port",
                connected_device_path
            ))
            association.extend((
                "pcie_bridge_port",
                "switch_port",
                connected_port_path
            ))
    elif self_device_type == "PCIE_BRIDGE":
        if connected_device_type == "SWITCH":
            association.extend((
                "associated_switch",
                "connected_port",
                connected_device_path
            ))
            association.extend((
                "switch_port",
                "pcie_bridge_port",
                connected_port_path
            ))
    return association

#########################################################################
def generateConfig():
    for self_device_type, all_device_data in topology_data.items():
        device_expose = []
        for self_device_id, self_link_data in all_device_data.items():
            device_all_ports = []
            port_count = 0
            for link_data in self_link_data:
                self_port_id = link_data['self_port_id']
                connected_device_id = link_data['connected_device_id']
                connected_device_type = link_data['connected_device_type']
                connected_port_id = link_data['connected_device_port_id']

                association = []
                self_logical_id, self_port_name = device_physical_to_logical_port[self_device_type][self_device_id][self_port_id]
                self_device_path = device_inventory_paths[self_device_type].replace("$INSTANCE_NUMBER", str(self_device_id))
                self_port_path = f"{self_device_path}/Ports/{self_port_name}"

                if connected_device_type != "EXTERNAL":
                    connected_logical_id, connected_port_name = device_physical_to_logical_port[connected_device_type][connected_device_id][connected_port_id]
                    connected_device_path = device_inventory_paths[connected_device_type].replace("$INSTANCE_NUMBER", str(connected_device_id))
                    connected_port_path = f"{connected_device_path}/Ports/{connected_port_name}"

                    association = getAssociations(self_device_type, connected_device_type, connected_device_path, connected_port_path)
                else:
                    association.extend(("","",""))

                device_all_ports.append({
                    'InventoryObjPath': self_port_path,
                    'LogicalPortNumber': self_logical_id,
                    'Associations': association
                })
                port_count += 1

            device_name = self_device_path.split('/')[-1]
            device_expose.append({
                'Name': device_name,
                'Type': "NVLinkTopology",
                'Count': port_count,
                'Topology': device_all_ports
            })

        # Add the device expose list to the dictionary by device type
        final_config.append({
            'Exposes': device_expose,
            'Probe': device_probe[self_device_type],
            'Name': self_device_type,
            'Type': "LinkTopology"
        })

#########################################################################
def main(input_file, output_file):
    # Step:1 Read excel file and get all the data into structure
    getDataFromExcel(input_file)
    # Step:2 Sort the link data based on the physical port Id [self_port_id]
    sortTopologyData()
    # Step:3 Create mapping between logical and physical port Id
    getPhysicalToLogicalMapping()
    # Step:4 Generate final configuration
    generateConfig()

    # Convert generated config to JSON format
    json_data_next = json.dumps(final_config, indent=4)
    with open(output_file, 'w', encoding='utf-8') as json_file:
        json_file.write(json_data_next)

    print("##### JSON data written to " + output_file + " #####")

#########################################################################
if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python nvlink_topology_processor.py <input_mapping_file> <output_json_file>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    main(input_file, output_file)