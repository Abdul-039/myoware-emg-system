# Sender Module

This folder contains the ESP32 sender firmware for the MyoWare EMG system.

## What it does

The sender reads EMG analog data from the sensor, buffers it in batches, and transmits those batches to the receiver over ESP-NOW.

## Project files

- `main/main.c` – sender firmware
- `CMakeLists.txt` – ESP-IDF project definition
- `sdkconfig` – build configuration

## Sender behavior

The sender firmware:

- initializes Wi-Fi and ESP-NOW
- adds the receiver as an ESP-NOW peer
- samples the EMG channel using ADC
- collects 10 samples per batch
- sends each batch as a packed struct containing:
  - `node_id`
  - `seq_num`
  - `emg[10]`

## Key configuration

The sender uses:

- `NODE_ID` to identify the transmitting device
- a fixed receiver MAC address for the ESP-NOW peer
- a 1 ms timer tick for EMG sampling
- a 10-sample batch to reduce wireless overhead

## Build and run

From this folder:

```bash
idf.py build
idf.py flash
idf.py monitor
```

## Notes

- Set `NODE_ID` to `1` or `2` depending on which sensor node this device represents.
- Make sure the receiver is running and its MAC address matches the peer address configured in the sender firmware.
