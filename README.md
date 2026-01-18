# 🚁 Drone Detector System for ESP32-S3

![Drone Detector Banner](https://img.shields.io/badge/ESP32--S3-Drone_Detector-blue)
![License](https://img.shields.io/badge/License-MIT-green)
![Version](https://img.shields.io/badge/Version-4.1-red)

Advanced RF spectrum analyzer for drone detection using ESP32-S3, nRF24L01+, and OLED display. Detects FPV drones, RC controllers, Bluetooth devices, and WiFi networks in the 2.4GHz band.

## 📋 Features

### ✅ **Detection Capabilities**
- **FPV Video Links** - RaceBand channel detection (32, 40, 48, 56...)
- **RC Control Links** - Narrow-band control signal detection
- **Bluetooth/BLE** - Advertising channel detection (37, 38, 39)
- **WiFi Networks** - 2.4GHz network scanning and analysis
- **Vendor OUI Detection** - DJI, Autel, Parrot, Yuneec, etc.
- **Frequency Hopping** - Pattern recognition for FPV systems

### ✅ **Advanced Analysis**
- Signal consistency scoring
- Burst activity detection
- Channel spread analysis
- Noise floor calculation
- Real-time waterfall display
- Multi-sensor data fusion

### ✅ **Visualization**
- **OLED Display** (128x64) with:
  - Real-time spectrum analyzer
  - Waterfall history display
  - Signal type color coding
  - Alert status and confidence scores
- **Serial Monitor** for detailed debugging

## 🛠️ Hardware Requirements

### **Components:**
1. **ESP32-S3 Dev Board**
2. **nRF24L01+** RF module
3. **SSD1306 OLED Display** (128x64, I2C)
4. **Buzzer** (passive)
5. **Breadboard & Jumper Wires**

### **Pin Connections:**

| Component | ESP32-S3 Pin | Notes |
|-----------|--------------|-------|
| nRF24 CE | GPIO 9 | Chip Enable |
| nRF24 CSN | GPIO 10 | Chip Select |
| nRF24 MOSI | GPIO 35 | SPI Data |
| nRF24 MISO | GPIO 37 | SPI Data |
| nRF24 SCK | GPIO 36 | SPI Clock |
| OLED SDA | GPIO 17 | I2C Data |
| OLED SCL | GPIO 18 | I2C Clock |
| Buzzer | GPIO 8 | Active HIGH |

### **Power Requirements:**
- **ESP32-S3**: USB 5V, 500mA minimum
- **nRF24L01+**: 3.3V, 15-20mA peak
- **OLED**: 3.3V, 20mA

⚠️ **Important**: Use a stable 3.3V power supply for nRF24L01+ to avoid brownout issues.

# ESP32-S3 Drone Detector

## INSTALLATION GUIDE

### 1. Install ESP32 Board Support

**Arduino IDE:**
1. Open Arduino IDE
2. Go to File → Preferences
3. Add this URL to Additional Boards Manager URLs:
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
4. Go to Tools → Board → Boards Manager
5. Search for "esp32"
6. Install "ESP32 by Espressif Systems"
7. Select: ESP32S3 Dev Module

**PlatformIO:**
Open VS Code → Extensions → Search "PlatformIO" → Install

Create new project:
pio project init --board esp32-s3-devkitc-1

### 2. Install Required Libraries

**Arduino IDE:**
Tools → Manage Libraries → Search and install:
- RF24 by TMRh20
- Adafruit GFX Library
- Adafruit SSD1306

**PlatformIO:**
Add to platformio.ini:
lib_deps = 
    nRF24/RF24
    adafruit/Adafruit GFX Library
    adafruit/Adafruit SSD1306

### 3. Board Configuration

**Arduino IDE Settings:**
Board: ESP32S3 Dev Module
Flash Mode: QIO
Flash Size: 16MB
Partition Scheme: Default 16MB
CPU Frequency: 240MHz
Upload Speed: 921600

### 4. Pin Configuration

**Default ESP32-S3 Pins:**
nRF24 CE → GPIO 9
nRF24 CSN → GPIO 10
nRF24 MOSI → GPIO 35
nRF24 MISO → GPIO 37
nRF24 SCK → GPIO 36
OLED SDA → GPIO 17
OLED SCL → GPIO 18
Buzzer → GPIO 8

### 5. Upload Code

1. Connect ESP32-S3 via USB
2. Select correct COM port
3. Click Upload in Arduino IDE
4. Open Serial Monitor (115200 baud)

## TROUBLESHOOTING

**Brownout/Restart:**
- Use external 3.3V regulator for nRF24
- Add 100µF capacitor on 3.3V line
- Disable brownout detector in code

**No RF Detection:**
- Check SPI connections
- Verify 3.3V power to nRF24
- Ensure antenna is connected

**OLED Not Working:**
- Check I2C connections
- Try address 0x3D instead of 0x3C
- Verify OLED power (3.3V)

## CODE STRUCTURE

Main Files:
- drone_detector.ino (main sketch)
- Configurable settings at top
- Detection logic in loop()

Key Functions:
- performRF24Scan() - Scans 2.4GHz spectrum
- performWiFiScan() - Detects WiFi networks
- analyzeRFPattern() - Signal analysis
- drawDashboard() - OLED display update

## SUPPORT

For issues:
1. Check Serial Monitor output
2. Verify all connections
3. Ensure proper power supply
4. Test with DEBUG mode enabled

Version: 4.1
Last Updated: January 2024
Compatible: ESP32-S3
