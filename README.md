# KY-039_cardputer
**[`日本語`](README_jp.md)**

This is an application that uses the M5Stack Cardputer and a KY-039 heart rate sensor to display real-time heart rate (BPM) and pulse waveform.

## Main Features

*   **Real-time Measurement & Display:** Measures and displays heart rate (BPM) and pulse waveform in real-time.
*   **Real-time Waveform Plotting:** Plots the raw analog values from the sensor in real-time. The vertical axis of the graph is auto-scaled for optimal waveform visibility.
*   **Dual Display Modes:** Switch between two display modes depending on your needs.
    *   **Plot Mode:** Displays a large waveform graph with a compact BPM value at the top of the screen.
    *   **BPM Mode:** Displays the BPM value prominently in the center of the screen for focused measurement.
*   **Rich Configuration Options:**
    *   **Screen Brightness:** Finely adjustable from 0 (off) to 255.
    *   **Low Battery Warning:** Displays a warning and automatically shuts down to protect the battery when the level falls below a specified threshold (5% to 95%) for a certain period.
    *   **Language Switching:** Switch the display language between English and Japanese.
*   **Persistent Settings:** Modified settings are automatically saved to the device's non-volatile storage (NVS) and are retained on the next startup.
*   **SD Updater Support:** By pressing the 'a' key during startup, you can launch 'menu.bin' from the SD card.

## Required Hardware

*   M5Stack Cardputer
*   KY-039 Heart Rate Sensor Module

## Connections

Connect the KY-039 sensor to the Cardputer's Grove port (Port.A).
KY-039 Singal level must be down to 3.3Volts .  

| KY-039 | Cardputer (Grove Port.A) |
| :----: | :----------------------: |
|   S    |            G1 (use Analog Input)   |
|  VCC   |           5.0V           |
|  GND   |           GND            |

## Installation

#### Method 1: Using the SD Updater (Recommended)
1.  Copy the `.bin` file from the `BINS` folder of this repository to the root of your SD card.
2.  Use the `menu.bin` from M5Stack-SD-Updater to flash the firmware from the SD card.

#### Method 2: Using PlatformIO
1.  Clone or download this repository.
2.  Install Visual Studio Code and the PlatformIO extension.
3.  Open the project in PlatformIO, connect your Cardputer, and upload.

## How to Use

### Key Assignments

Access various functions using the number keys on the keyboard.

| Key | Function                                                              |
| :--: | :---------------------------------------------------------------- |
| `0`  | Toggles the display mode between "BPM Mode" and "Plot Mode". |
| `1`  | Enters the screen brightness setting mode.           |
| `2`  | Enters the low battery warning threshold setting mode. |
| `3`  | Enters the display language setting mode.             |

### Settings Mode

When you enter a settings mode with keys `1` through `3`, the current setting item and its value will be displayed at the top of the screen.
Use the arrow keys (`;` `.` `,` `/`) to change the values.

| Key | Function                                                              |
| :--: | :---------------------------------------------------------------- |
| `;`  | Increases the value in large steps. (Up key)                                  |
| `.`  | Decreases the value in large steps. (Down key)                                  |
| `/`  | Increases the value in small steps. (Right key)                                  |
| `,`  | Decreases the value in small steps. (Left key)                                  |
| `` ` ``  | Exits settings mode and returns to normal mode. (Backquote key) |

### Startup Options
*   **Pressing 'a' while booting:** Launches `menu.bin` from the SD card.

## Technical Details

### Heart Rate Calculation
*   Analog values from the sensor are averaged over a 20ms period to eliminate 50Hz power line noise.
*   A moving average filter is then applied to generate a smooth, low-noise waveform.
*   It detects the rising edge of the waveform to calculate the interval between heartbeats.
*   A weighted average of the last three beat intervals (current: 0.4, previous: 0.3, second previous: 0.3) is used to calculate a stable BPM value.
*   The validity of the measurements (waveform amplitude, beat interval, sudden BPM changes) is verified, and outliers are rejected to enhance reliability.

### Power Saving Features
*   Wi-Fi and Bluetooth, which are not used in this application, are disabled at startup to reduce power consumption.

## License

This software is released under the MIT License.

## Author

*   NoRi