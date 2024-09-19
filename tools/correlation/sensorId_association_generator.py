import json
import sys

import pandas as pd

DETAILS_COLUMNS = "A:D"
DETAILS_HEADER_ROW = 1


def generateConfig(input_file):
    details_df = pd.read_excel(
        input_file,
        engine="openpyxl",
        usecols=DETAILS_COLUMNS,
        skiprows=DETAILS_HEADER_ROW - 1,
    )

    count = 0
    generatedConfig = []
    # Get data from excel
    for index, row in details_df.iterrows():
        sensorID = int(row["SensorID"])
        portType = str(row["PortType"])
        deviceType = str(row["DeviceType"])
        inventoryObjPath = str(row["DbusInventoryPath"])

        exposeName = ""
        if deviceType == "SWITCH":
            exposeName = "NVSwitch"
        elif deviceType == "PCIE_BRIDGE":
            exposeName = "NVLinkManagementNIC"

        portName = inventoryObjPath.split("/")[-1]

        data = [
            {
                "Name": f"{exposeName}_$INSTANCE_NUMBER_Port_{count}_AUX",
                "Type": "SensorAuxName",
                "SensorId": sensorID,
                "AuxNames": [f"{portName}"],
            },
            {
                "Name": f"{exposeName}_$INSTANCE_NUMBER_Port_{count}_Info",
                "Type": "SensorPortInfo",
                "SensorId": sensorID,
                "PortProtocol": "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.NVLink",
                "PortType": f"xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.{portType}",
                "MaxSpeedMBps": 50000,
                "Association": [
                    "associated_port",
                    "associated_port",
                    f"{inventoryObjPath}",
                ],
            },
        ]
        generatedConfig.extend(data)
        count = count + 1

    return generatedConfig


#########################################################################
def main(input_file, output_file):

    output_config = generateConfig(input_file)

    # Convert generated config to JSON format
    json_data_next = json.dumps(output_config, indent=4)
    with open(output_file, "w", encoding="utf-8") as json_file:
        json_file.write(json_data_next)

    print("##### JSON data written to " + output_file + " #####")
    print(
        "##### Copy the corresponding config from "
        + output_file
        + " to hgxb_cx_chassis.json and hgxb_nvlink_chassis.json in EM for NVSwicth and NIC manually #####"
    )


#########################################################################
if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(
            "Usage: python sensorId_association_generator.py <input_mapping_file> <output_json_file>"
        )
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    main(input_file, output_file)
