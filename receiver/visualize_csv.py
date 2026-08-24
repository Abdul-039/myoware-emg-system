#!/usr/bin/env python3
import argparse
import math
import os
import sys
import time
import csv
from datetime import datetime
from collections import deque

import matplotlib
import matplotlib as mpl

if os.environ.get("MPLBACKEND") is None and os.name == "nt":
    # Default to a GUI-capable backend on Windows when no backend is forced.
    try:
        matplotlib.use("TkAgg")
    except Exception:
        pass

import matplotlib.pyplot as plt
import serial
import serial.tools.list_ports

# --- Disable Matplotlib's default 'L' shortcut for Log Scale ---
try:
    if 'l' in mpl.rcParams['keymap.yscale']:
        mpl.rcParams['keymap.yscale'].remove('l')
    if 'L' in mpl.rcParams['keymap.xscale']:
        mpl.rcParams['keymap.xscale'].remove('L')
except Exception:
    pass

class CsvLivePlotter:
    def __init__(self, port=None, baudrate=115200, window_size=200):
        self.port = port
        self.baudrate = baudrate
        self.window_size = window_size
        self.ser = None
        self.sample_count = 0
        self.draw_interval = 0.05  # seconds between redraws (20 Hz)
        self.last_draw_time = 0.0

        # --- Logging Variables ---
        self.is_logging = False
        self.csv_file = None
        self.csv_writer = None

        self.timestamps = deque(maxlen=window_size)
        
        # Dual EMG queues
        self.emg1_series = [deque(maxlen=window_size) for _ in range(10)]
        self.emg2_series = [deque(maxlen=window_size) for _ in range(10)]
        
        self.joy_series = [deque(maxlen=window_size) for _ in range(3)]

        plt.ion()
        # Expanded to 3 subplots for Dual Node + Joystick
        self.fig, self.axes = plt.subplots(3, 1, figsize=(12, 10), sharex=True)
        self.fig.suptitle("Live Dual EMG + Joystick Visualizer")
        
        self.axes[0].set_title("EMG Node 1")
        self.axes[1].set_title("EMG Node 2")
        self.axes[2].set_title("Joystick & Button")

        # Lines for Node 1
        self.emg1_lines = []
        for i in range(10):
            line, = self.axes[0].plot([], [], label=f"Node1 {i}")
            self.emg1_lines.append(line)

        # Lines for Node 2
        self.emg2_lines = []
        for i in range(10):
            line, = self.axes[1].plot([], [], label=f"Node2 {i}")
            self.emg2_lines.append(line)

        # Lines for Joystick
        self.joy_lines = []
        for label, color in [("joy_x", "tab:blue"), ("joy_y", "tab:orange"), ("btn", "tab:red")]:
            line, = self.axes[2].plot([], [], label=label, color=color)
            self.joy_lines.append(line)

        self.axes[0].legend(loc="upper right", ncol=5, fontsize='small')
        self.axes[1].legend(loc="upper right", ncol=5, fontsize='small')
        self.axes[2].legend(loc="upper right")
        self.axes[2].set_xlabel("Sample")
        
        # UI Element for Logging Status 
        self.status_text = self.fig.text(0.02, 0.96, "Logging: OFF (Press 'L' to toggle)", 
                                         color='red', fontsize=12, weight='bold')

        # Bind Keyboard Event 
        self.fig.canvas.mpl_connect('key_press_event', self.on_key)

        plt.tight_layout(rect=[0, 0, 1, 0.95]) # Adjust layout to make room for suptitle and status
        plt.show(block=False)
        self.fig.canvas.draw_idle()
        self.fig.canvas.flush_events()

    def on_key(self, event):
        """Handle keyboard presses, specifically 'L' for logging."""
        if event.key is not None and event.key.lower() == 'l':
            self.toggle_logging()

    def toggle_logging(self):
        """Start or stop the CSV logging."""
        if not self.is_logging:
            timestamp_str = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"data_log_{timestamp_str}.csv"
            
            try:
                self.csv_file = open(filename, mode='w', newline='')
                self.csv_writer = csv.writer(self.csv_file)
                
                # Write CSV Header with dual nodes
                header = (["seq", "timestamp_ms"] + 
                          [f"emg1_{i}" for i in range(10)] + 
                          [f"emg2_{i}" for i in range(10)] + 
                          ["joy_x", "joy_y", "btn"])
                self.csv_writer.writerow(header)
                
                self.is_logging = True
                self.status_text.set_text(f"Logging: ON ({filename})")
                self.status_text.set_color('green')
                print(f"Started logging to {filename}")
                
            except Exception as e:
                print(f"Failed to start logging: {e}")
                self._close_log_file()
        else:
            self._close_log_file()
            self.is_logging = False
            self.status_text.set_text("Logging: OFF (Press 'L' to toggle)")
            self.status_text.set_color('red')
            print("Stopped logging.")
            
        self.fig.canvas.draw_idle()
        self.fig.canvas.flush_events()

    def _close_log_file(self):
        if self.csv_file is not None and not self.csv_file.closed:
            self.csv_file.close()
            self.csv_file = None
            self.csv_writer = None

    def _list_ports(self):
        return [p.device for p in serial.tools.list_ports.comports()]

    def connect(self):
        candidates = [self.port] if self.port else self._list_ports()
        if not candidates:
            print("No serial ports detected. Connect the ESP32 and try again.")
            raise SystemExit(1)

        if self.port is None:
            print("Scanning available ports...")

        last_error = None
        for port_name in candidates:
            try:
                self.ser = serial.Serial(port_name, self.baudrate, timeout=0.1)
                print(f"Connected to {port_name} at {self.baudrate} baud")
                return port_name
            except serial.SerialException as exc:
                last_error = exc
                print(f"Port {port_name} unavailable: {exc}")

        available_ports = self._list_ports()
        print("This usually means another app is already using the port or Windows is blocking access.")
        print("Try the following:")
        print("  1. Close any serial monitor or debugger that may already have the port open.")
        print("  2. Disconnect and reconnect the ESP32.")
        print("  3. Use a specific COM port, for example --port COM4.")
        if available_ports:
            print("Available ports:")
            for candidate in available_ports:
                print(f"  - {candidate}")
        else:
            print("No serial ports were detected.")
        raise SystemExit(1) from last_error

    def parse_line(self, line):
        cleaned = line.strip()
        if not cleaned:
            return None
        parts = [p.strip() for p in cleaned.split(",")]
        
        # Now expecting 25 elements: 2 (headers) + 10 (emg1) + 10 (emg2) + 3 (joystick)
        if len(parts) < 25:
            return None

        seq_num = int(parts[0])
        timestamp_ms = int(parts[1])
        emg1_values = [int(x) for x in parts[2:12]]
        emg2_values = [int(x) for x in parts[12:22]]
        joy_x = int(parts[22])
        joy_y = int(parts[23])
        btn = int(parts[24])
        
        return {
            "seq": seq_num,
            "timestamp_ms": timestamp_ms,
            "emg1": emg1_values,
            "emg2": emg2_values,
            "joy_x": joy_x,
            "joy_y": joy_y,
            "btn": btn,
        }

    def update(self, row):
        # Handle Logging
        if self.is_logging and self.csv_writer is not None:
            csv_row = ([row["seq"], row["timestamp_ms"]] + 
                       row["emg1"] + row["emg2"] + 
                       [row["joy_x"], row["joy_y"], row["btn"]])
            self.csv_writer.writerow(csv_row)

        self.timestamps.append(self.sample_count)
        self.sample_count += 1

        for idx, value in enumerate(row["emg1"]):
            self.emg1_series[idx].append(value)
            
        for idx, value in enumerate(row["emg2"]):
            self.emg2_series[idx].append(value)

        self.joy_series[0].append(row["joy_x"])
        self.joy_series[1].append(row["joy_y"])
        self.joy_series[2].append(row["btn"])

        now = time.time()
        if now - self.last_draw_time >= self.draw_interval:
            self._draw()

    def _draw(self):
        x = list(self.timestamps)
        if len(x) < 2:
            return

        xmin = max(0, x[-1] - self.window_size)
        xmax = x[-1]

        # Update Node 1 Lines
        for idx, line in enumerate(self.emg1_lines):
            y = list(self.emg1_series[idx])
            line.set_data(x, y)

        # Autoscale Node 1 y-axis
        all_emg1 = [v for dq in self.emg1_series for v in dq]
        if all_emg1:
            ymin = min(all_emg1)
            ymax = max(all_emg1)
            if ymin == ymax:
                margin = max(1, abs(ymin) * 0.05)
                ymin -= margin
                ymax += margin
            self.axes[0].set_ylim(ymin, ymax)
        self.axes[0].set_xlim(xmin, xmax)

        # Update Node 2 Lines
        for idx, line in enumerate(self.emg2_lines):
            y = list(self.emg2_series[idx])
            line.set_data(x, y)

        # Autoscale Node 2 y-axis
        all_emg2 = [v for dq in self.emg2_series for v in dq]
        if all_emg2:
            ymin = min(all_emg2)
            ymax = max(all_emg2)
            if ymin == ymax:
                margin = max(1, abs(ymin) * 0.05)
                ymin -= margin
                ymax += margin
            self.axes[1].set_ylim(ymin, ymax)
        self.axes[1].set_xlim(xmin, xmax)

        # Update joystick lines
        for idx, line in enumerate(self.joy_lines):
            y = list(self.joy_series[idx])
            line.set_data(x, y)

        # Autoscale joystick y-axis
        all_joy = [v for dq in self.joy_series for v in dq]
        if all_joy:
            ymin = min(all_joy)
            ymax = max(all_joy)
            if ymin == ymax:
                ymin -= 1
                ymax += 1
            self.axes[2].set_ylim(ymin, ymax)
        self.axes[2].set_xlim(xmin, xmax)

        self.fig.canvas.draw_idle()
        self.fig.canvas.flush_events()
        self.last_draw_time = time.time()

    def run(self):
        self.connect()
        try:
            while True:
                line = self.ser.readline()
                if not line:
                    time.sleep(0.01)
                    continue
                row = self.parse_line(line.decode("utf-8", errors="ignore"))
                if row is None:
                    continue
                self.update(row)
        except KeyboardInterrupt:
            print("\nStopped.")
        except serial.SerialException as exc:
            print(f"Serial communication error: {exc}")
            raise SystemExit(1) from exc
        finally:
            self._close_log_file()
            if self.ser is not None and self.ser.is_open:
                self.ser.close()
                plt.close(self.fig)

    def run_demo(self, duration_seconds=3.0):
        start = time.time()
        try:
            while time.time() - start < duration_seconds:
                sample = self.sample_count
                row = {
                    "seq": sample,
                    "timestamp_ms": int(time.time() * 1000),
                    "emg1": [int(300 + 100 * math.sin(sample / 8 + i * 0.6)) for i in range(10)],
                    "emg2": [int(300 + 100 * math.cos(sample / 8 + i * 0.6)) for i in range(10)],
                    "joy_x": int(2048 + 500 * math.sin(sample / 20)),
                    "joy_y": int(2048 + 400 * math.cos(sample / 25)),
                    "btn": 1 if sample % 20 == 0 else 0,
                }
                self.update(row)
                time.sleep(0.05)
        except KeyboardInterrupt:
            print("\nDemo Stopped.")
        finally:
            self._close_log_file()
            plt.close(self.fig)


def main():
    parser = argparse.ArgumentParser(description="Plot ESP32 Dual Node CSV data in real time")
    parser.add_argument("--port", help="Serial port, e.g. COM4 or /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate")
    parser.add_argument("--window", type=int, default=200, help="Number of samples to keep on screen")
    parser.add_argument("--demo", action="store_true", help="Generate synthetic data instead of reading from a serial port")
    parser.add_argument("--duration", type=float, default=60.0, help="Demo duration in seconds")
    args = parser.parse_args()

    plotter = CsvLivePlotter(port=args.port, baudrate=args.baud, window_size=args.window)

    if args.demo:
        plotter.run_demo(duration_seconds=args.duration)
    else:
        plotter.run()


if __name__ == "__main__":
    main()