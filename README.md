# MyoWare EMG Wireless System

This repository contains the complete wireless EMG acquisition setup for the MyoWare system, split into two ESP32 firmware projects:

- `receiver/` – hub that receives dual EMG packets over ESP-NOW, reads joystick inputs, and outputs CSV data
- `sender/` – wireless EMG sender node that samples the EMG signal and transmits packets to the hub

Together, these projects form a dual-sensor EMG data collection system for live monitoring, logging, and analysis.

## Repository layout

```text
myoware_project/
├── README.md
├── .gitignore
├── receiver/
│   ├── CMakeLists.txt
│   ├── main/
│   ├── sdkconfig
│   ├── visualize_csv.py
│   └── README.md
├── sender/
│   ├── CMakeLists.txt
│   ├── main/
│   └── sdkconfig
└── data/
    └── (optional captured CSV logs)
```

## System overview

### Sender side
The sender firmware runs on an ESP32 board connected to a MyoWare EMG sensor. It samples EMG data at a fixed interval, packs the samples into a batch, and sends them over ESP-NOW to the receiver.

### Receiver side
The receiver firmware runs on a second ESP32 board, listens for incoming ESP-NOW packets from both sender nodes, reads joystick state, and streams combined CSV output to serial. The Python script in the receiver folder can plot the signal in real time and save logs to CSV.

## Communication protocol

Each EMG packet contains:

- `node_id`
- `seq_num`
- `emg[10]`

The receiver separates packets by `node_id` and stores each stream independently.

## Hardware setup

- One ESP32 device for the receiver hub
- One or more ESP32 sender devices with MyoWare EMG sensors
- Receiver uses joystick ADC inputs on GPIO34 and GPIO35
- Receiver reads the joystick button on GPIO32
- Data is transmitted using ESP-NOW

## Build instructions

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

From the receiver folder:

```bash
python -m pip install pyserial matplotlib
python visualize_csv.py --port COM4
```

Optional demo mode:

```bash
python visualize_csv.py --demo
```

## Notes

- The sender and receiver are intentionally kept in separate folders so each project remains independently buildable.
- The receiver script logs CSV files with names like `data_log_YYYYMMDD_HHMMSS.csv`.
- If you are working from a fresh clone, ensure ESP-IDF is installed and active before running the build commands.

## Git workflow

This repository is ready to be initialized as a normal Git repo and pushed to GitHub.

```bash
git init
git add .
git commit -m "Initial combined sender + receiver repo"
git branch -M main
git remote add origin <your-github-repo-url>
git push -u origin main
```

## License

This project is provided as-is for prototyping and research use.
