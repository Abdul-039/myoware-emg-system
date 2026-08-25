# MyoWare EMG Wireless System

This repository contains the complete wireless EMG acquisition setup for a MyoWare-based system, split into two ESP32 firmware projects:

- `receiver/` – the hub that receives EMG packets over ESP-NOW, reads joystick input, and streams CSV data to the PC
- `sender/` – the wireless EMG node that samples the EMG channel and transmits the data to the receiver

Together, these projects form a dual-sensor wireless EMG data acquisition system for live monitoring, logging, and analysis.

## Repository layout

```text
myoware-emg-system/
├── README.md
├── .gitignore
├── Mac Address FreeNove/
│   └── (reference material for FreeNove MAC address lookup)
├── receiver/
│   ├── CMakeLists.txt
│   ├── main/
│   ├── sdkconfig
│   ├── visualize_csv.py
│   ├── README.md
│   └── ...
├── sender/
│   ├── CMakeLists.txt
│   ├── main/
│   ├── sdkconfig
│   ├── README.md
│   └── ...
└── docs/
    └── (optional additional project notes)
```

## Hardware architecture

![EMG Data Acquisition System using ESP-NOW](../architecture.png)

> The image above shows the real setup: two MyoWare sensor boards communicate wirelessly with one FreeNove ESP32 hub, and the hub is connected to the laptop over USB. The joystick is wired directly to the FreeNove ESP32.


## Software running on each device

### Sender ESP32
Software running on the sender device:
- ESP-IDF firmware
- ADC sampling of the EMG analog channel
- timer-based batch acquisition
- ESP-NOW transmission to the receiver
- packet format includes `node_id`, `seq_num`, and 10 EMG samples

### Receiver ESP32
Software running on the receiver device:
- ESP-IDF firmware
- Wi-Fi station initialization
- ESP-NOW receive mode
- dual-node packet routing by `node_id`
- joystick ADC reads
- serial CSV output

### PC / Laptop
Software running on the PC:
- Python script `visualize_csv.py`
- live plotting of EMG signals and joystick data
- optional CSV logging using the `L` key

## Relationship between devices

In the practical setup, two MyoWare shields are connected to separate ESP32 sender boards and communicate wirelessly to a single FreeNove ESP32 board through ESP-NOW. The FreeNove board acts as the central hub. It receives the EMG data from both sensors, reads the joystick connected to the same hub, and sends the combined stream to the laptop over USB serial. The Python script on the laptop then plots the live data and logs it as CSV.

## IDE and toolchain used

This project was developed using:
- Visual Studio Code
- ESP-IDF extension for VS Code
- ESP-IDF toolchain / build system

IDE link:
https://code.visualstudio.com/

ESP-IDF toolchain / documentation:
https://idf.espressif.com/

ESP-IDF VS Code extension:
https://github.com/espressif/vscode-esp-idf-extension

## Hardware used

- ESP32 receiver hub
- ESP32 sender node(s)
- MyoWare EMG sensor(s)
- Joystick / analog input hardware on receiver
- USB connection to PC for serial monitoring and plotting

## Hardware address (MAC address) retrieval

The ESP32 boards expose a unique Wi-Fi MAC address that is used for ESP-NOW communication.

### Method 1: read the MAC from the serial monitor
After flashing the firmware, open the serial monitor:

```bash
cd receiver
idf.py monitor
```

or

```bash
cd sender
idf.py monitor
```

The program prints the board MAC address in startup logs, for example:

```text
FreeNove Hub MAC: xx:xx:xx:xx:xx:xx
Shield MAC: xx:xx:xx:xx:xx:xx
```

The MAC printed by the board should match the address used in the ESP-NOW peer configuration.

### Method 2: use the `Mac Address FreeNove` folder in this repository
The repository includes a folder named `Mac Address FreeNove`, which contains reference material used to identify the hardware MAC address of the FreeNove ESP32 board. This is useful when verifying the board identity before configuring ESP-NOW peers. In this project, the sender uses a fixed target MAC address in the code:

```c
static uint8_t c3_mac[] = {0xD0, 0xEF, 0x76, 0x1F, 0x83, 0x2C};
```

The receiver also reads its own MAC using:

```c
esp_read_mac(mac, ESP_MAC_WIFI_STA);
```

This is the address that should be checked against the board’s actual hardware identifier.

### Important note
For ESP-NOW to work correctly, the sender must be configured with the correct target receiver MAC address, and the receiver must be running and listening on the same Wi-Fi channel.

## Communication protocol

Each EMG packet contains:

- `node_id`
- `seq_num`
- `emg[10]`

The receiver separates packets by `node_id` and stores each stream independently.

## Basic initialization sequence

The current prototype is designed as follows:

1. Connect the ESP32 sender board(s) to the MyoWare EMG sensor(s).
2. Connect the receiver ESP32 to the PC via USB.
3. Set the sender node ID in the firmware:
   - `NODE_ID 1` or `NODE_ID 2`
4. Build and flash the sender firmware:

```bash
cd sender
idf.py build
idf.py flash
idf.py monitor
```

5. Confirm the sender printed its MAC address and is ready.
6. Build and flash the receiver firmware:

```bash
cd receiver
idf.py build
idf.py flash
idf.py monitor
```

7. Confirm the receiver printed its MAC address and is listening for packets.
8. Power on the sender node(s).
9. Verify the receiver receives incoming ESP-NOW data in the serial monitor.
10. Start the Python visualizer on the PC:

```bash
cd receiver
python visualize_csv.py --port COM4
```

11. Press `L` in the visualizer window to start/stop CSV logging.
12. Use the plotted signal for live monitoring or save the CSV file for analysis.

## Current operational note

The current version of the system does not use a dedicated hardware start/stop push button for acquisition. In this implementation:
- the receiver reads joystick data continuously
- the Python visualizer uses the keyboard key `L` to toggle CSV logging
- the joystick button is included in the CSV stream as `btn`

This means the practical workflow is:
- flash sender and receiver
- power the devices
- observe serial output and live plot
- press `L` in the Python window to begin saving data

## Build and run instructions

### Receiver
```bash
cd receiver
idf.py build
idf.py flash
idf.py monitor
```

### Sender
```bash
cd sender
idf.py build
idf.py flash
idf.py monitor
```

## Visualizing live data

From the receiver directory:

```bash
python -m pip install pyserial matplotlib
python visualize_csv.py --port COM4
```

Optional demo mode:

```bash
python visualize_csv.py --demo
```

## Data output format

The receiver prints CSV rows in this format:

```text
seq,timestamp_ms,emg1_0,...,emg1_9,emg2_0,...,emg2_9,joy_x,joy_y,btn
```

## Notes

- The sender and receiver are intentionally kept in separate folders so each project remains independently buildable.
- The receiver script logs CSV files with names like `data_log_YYYYMMDD_HHMMSS.csv`.
- The physical MAC address can be retrieved from the serial monitor or by checking the FreeNove MAC address folder for reference.
- The system is built for prototyping and research use and is intended to be refined further as the project develops.

## License

This project is provided for educational, prototyping, and research use.
