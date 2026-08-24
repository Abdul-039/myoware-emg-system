# Sender Module

This folder contains the ESP32 sender firmware for the MyoWare EMG system.

## What it does

The sender reads EMG analog data from the sensor, buffers it in batches, and transmits those batches to the receiver over ESP-NOW.

## Build and run

```bash
idf.py build
idf.py flash
idf.py monitor
```
