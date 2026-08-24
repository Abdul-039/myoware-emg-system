# Receiver Module

This folder contains the ESP32 receiver hub for the MyoWare wireless EMG system.

## What it does

The receiver listens for EMG packets from one or more sender nodes over ESP-NOW, reads joystick inputs, and prints a combined CSV data stream to serial. The stream is then suitable for real-time plotting or offline analysis.

## Project files

- `main/main.c` – receiver firmware
- `CMakeLists.txt` – ESP-IDF project definition
- `sdkconfig` – build configuration
- `visualize_csv.py` – live visualizer for EMG and joystick signals

## Receiver behavior

The firmware:

- initializes Wi-Fi in station mode
- starts ESP-NOW receive mode
- listens for packets with `node_id == 1` and `node_id == 2`
- stores the latest EMG batch for each sender node
- reads joystick X/Y values from ADC channels
- reads the joystick button from GPIO32
- outputs one CSV row every ~10 ms

## Output format

```csv
seq,timestamp_ms,emg1_0,...,emg1_9,emg2_0,...,emg2_9,joy_x,joy_y,btn
```

## Build and run

From this folder:

```bash
idf.py build
idf.py flash
idf.py monitor
```

## Plot the live stream

Install dependencies:

```bash
python -m pip install pyserial matplotlib
```

Run the visualizer:

```bash
python visualize_csv.py --port COM4
```

Optional demo mode:

```bash
python visualize_csv.py --demo
```

Press `L` while the UI is open to start or stop CSV logging.

## Hardware mapping

- GPIO34 → joystick X ADC input
- GPIO35 → joystick Y ADC input
- GPIO32 → joystick button input
