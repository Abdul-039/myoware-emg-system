# MyoWare Receiver 2.0

This project is an ESP32-based receiver hub that collects dual MyoWare EMG sensor data from two wireless sensor nodes over ESP-NOW, reads joystick input, and streams the combined sample stream to serial output for logging and live plotting.

It is designed for a two-node EMG acquisition setup where each node sends a compact EMG packet and the hub consolidates the data into a single CSV-friendly stream.

## Overview

The system includes:

- An ESP32 receiver running ESP-IDF
- ESP-NOW wireless reception from two EMG sender nodes
- ADC joystick input readings
- Real-time CSV output over serial
- A Python live visualizer for plotting the EMG channels and joystick state
- Sample CSV logs generated from the receiver stream

## Project layout

- `main/main.c` – ESP32 firmware entry point and receiver logic
- `main/CMakeLists.txt` – component registration for the firmware
- `CMakeLists.txt` – top-level ESP-IDF project definition
- `visualize_csv.py` – Python real-time plotter and CSV logger
- `data_log_*.csv` – example captured datasets
- `sdkconfig` – ESP-IDF configuration
- `build/` – generated build artifacts

## Hardware and data flow

The receiver acts as a hub:

1. Each EMG sender node transmits a packed structure containing:
   - `node_id`
   - `seq_num`
   - `emg[10]`
2. The ESP32 hub listens for ESP-NOW packets.
3. Incoming packets are routed by `node_id` into separate buffers.
4. Every 10 ms, the hub reads:
   - joystick X (ADC channel on GPIO34)
   - joystick Y (ADC channel on GPIO35)
   - button state (GPIO32)
5. The receiver prints a CSV row in this format:

```text
seq,timestamp_ms,emg1_0,...,emg1_9,emg2_0,...,emg2_9,joy_x,joy_y,btn
```

That output is suitable for both live analysis and file logging.

## Pin configuration

The firmware currently uses:

- GPIO34 → joystick X ADC input (`JOY_X_CHANNEL`)
- GPIO35 → joystick Y ADC input (`JOY_Y_CHANNEL`)
- GPIO32 → joystick button input (`JOY_BTN_GPIO`)

The ADC is configured using `adc_oneshot` on ADC unit 1 with 12-bit resolution and 12 dB attenuation.

## Firmware behavior

The receiver firmware does the following:

- initializes NVS flash
- initializes Wi-Fi in station mode
- starts ESP-NOW receive mode
- initializes ADC for the joystick
- configures the button GPIO
- creates a background task that loops every 10 ms
- prints one sample line per loop to serial

The output loop frequency is approximately 100 Hz.

## Building and flashing

This project uses the ESP-IDF build system.

### Prerequisites

Install the ESP-IDF toolchain and configure your environment according to the official ESP-IDF setup instructions.

Typical flow:

```bash
cd "E:\myoware receiver 2.0"
idf.py build
idf.py flash
idf.py monitor
```

If the project was already built, you can also use the generated build directory directly.

## Running the live visualizer

The Python script in the project root reads the serial stream and plots the EMG and joystick signals in real time.

### Install Python dependencies

From the project folder:

```bash
python -m pip install pyserial matplotlib
```

### Run the visualizer

```bash
python visualize_csv.py --port COM4
```

On Linux or macOS, the port may look like `/dev/ttyUSB0` or `/dev/ttyACM0`.

Optional controls:

```bash
python visualize_csv.py --demo
python visualize_csv.py --port COM4 --window 300
```

### Keyboard controls

While the plot is running:

- Press `L` to start or stop CSV logging
- Log files are saved as `data_log_YYYYMMDD_HHMMSS.csv`

## Data format

The CSV rows contain the following columns:

- `seq` – running sample sequence number
- `timestamp_ms` – millisecond timestamp from `esp_timer_get_time()`
- `emg1_0` ... `emg1_9` – 10 EMG channels from node 1
- `emg2_0` ... `emg2_9` – 10 EMG channels from node 2
- `joy_x` – joystick X ADC value
- `joy_y` – joystick Y ADC value
- `btn` – button state

Example row:

```csv
6143,63195,1635,1585,1590,1599,1603,1604,1616,1622,1629,1637,2521,2295,2390,2335,2285,2240,2199,2170,2133,2112,3107,3061,1
```

## Notes

- The firmware assumes both sender nodes use the same packet structure and the same `node_id` values.
- The current receiver handles `node_id == 1` and `node_id == 2` only.
- The output is intentionally plain CSV for easy import into Python, MATLAB, or spreadsheet tooling.
- The demo plotter can generate synthetic data if no hardware is connected.

## Typical workflow

1. Flash the firmware to the ESP32 hub.
2. Power the two EMG sender nodes and confirm they are transmitting over ESP-NOW.
3. Open a serial monitor if needed.
4. Run `python visualize_csv.py --port <COM_PORT>`.
5. Press `L` to log live samples to CSV.
6. Analyze the resulting data files or use the live plot for signal inspection.

## Troubleshooting

If no serial device is detected:

- Ensure the ESP32 is connected and enumerated by the OS
- Close any serial monitor, debugger, or IDE port viewer using the same COM port
- Try specifying the port explicitly, for example `--port COM4`

If no EMG data appears:

- Verify both sender nodes are configured with the same packet layout and valid `node_id`s
- Check that ESP-NOW is enabled on both devices
- Confirm the hub has Wi-Fi initialized correctly in station mode

## License

This project is provided as-is for research, prototyping, and development use.
