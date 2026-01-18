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

3. Ensure proper power supply
4. Test with DEBUG mode enabled

Version: 4.1
Last Updated: January 2026
Compatible: ESP32-S3
