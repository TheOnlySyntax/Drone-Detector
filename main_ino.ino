#include <SPI.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>

// ================= PINY ESP32-S3 =================
#define CE_PIN     9
#define CSN_PIN    10
#define BUZZER_PIN 8

// SPI piny pro nRF24
#define SCK_PIN   36
#define MISO_PIN  37
#define MOSI_PIN  35

// I2C piny pro OLED SSD1306
#define I2C_SDA   17
#define I2C_SCL   18

// ================= OLED DISPLAY =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ================= RF24 =================
RF24 radio(CE_PIN, CSN_PIN);

// ================= NASTAVENÍ =================
#define DEBUG 1
#define RPD_DELAY_US 180
#define SCAN_INTERVAL_MS 50
#define MAX_CHANNELS 126
#define HISTORY_SIZE 64

// Wi-Fi kanály 2.4GHz
const int wifi24Channels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
const int wifi24Count = 13;

// BLE advertising kanály
const int bleChannels[] = {37, 38, 39};
const int bleCount = 3;

// Známé DRONE OUI (MAC prefixes)
const char* droneOUIs[] = {
  "94:B9:7E",  // DJI
  "60:60:1F",  // DJI
  "34:AB:95",  // Autel
  "AC:E0:10",  // Parrot
  "90:03:B7",  // Yuneec
  "A0:14:3D",  // Holybro
  "48:7B:39",  // Skydio
  "7C:76:35",  // BetaFPV
  "D4:3A:65",  // Walkera
  "00:12:1C",  // E-flite
  "00:26:7E"   // FrSky
};

// ================= STRUKTURY =================
struct FrameInfo {
  uint8_t mac[6];
  int8_t rssi;
  uint32_t timestamp;
  uint16_t channel;
  uint8_t frameType;
  uint8_t vendorScore;
  char ssid[33];
};

struct ChannelStats {
  int activity;
  int rssiSum;
  int rssiCount;
  uint32_t lastSeen;
  int noiseFloor;
};

struct DeviceProfile {
  uint8_t mac[6];
  int8_t avgRSSI;
  uint32_t firstSeen;
  uint32_t lastSeen;
  uint16_t channels[10];
  int channelCount;
  int hopRate;
  int frameCount;
  int vendorMatch;
  int droneScore;
  char nickname[20];
};

struct DetectionMetrics {
  int droneConfidence;
  int wifiAPScore;
  int bleScore;
  int fpvScore;
  int noiseLevel;
  int channelSpread;
  int burstActivity;
  int consistency;
  uint32_t detectionStart;
  bool alertActive;
};

// ================= PROMĚNNÉ =================
ChannelStats channelStats[MAX_CHANNELS + 1];
DetectionMetrics metrics;

uint8_t waterfall[SCREEN_WIDTH][32];
uint16_t spectrumData[MAX_CHANNELS + 1];
uint32_t channelHistory[HISTORY_SIZE];
int historyIndex = 0;

unsigned long lastDisplayUpdate = 0;
unsigned long lastBeep = 0;
unsigned long lastFullScan = 0;
unsigned long lastWiFiScan = 0;
unsigned long scanCount = 0;

// Proměnné pro pattern analýzu
int burstCounter = 0;
uint32_t lastBurstTime = 0;
int hopSequence[20];
int hopIndex = 0;
int deviceCount = 0;

// Buffer pro WiFi MAC adresy
String recentMACs[20];
int macIndex = 0;

// ================= DEBUG =================
#if DEBUG
  #define DBG(x)   Serial.print(x)
  #define DBGL(x)  Serial.println(x)
  #define DBG_HEX(x) Serial.print(x, HEX)
#else
  #define DBG(x)
  #define DBGL(x)
  #define DBG_HEX(x)
#endif

// ================= BEEPER FUNKCE =================
void beep(int freq, int dur, bool async = false) {
  if (freq > 0) {
    tone(BUZZER_PIN, freq, dur);
    if (!async) {
      delay(dur + 20);
    }
  }
}

// ================= SIMPLIFIED WIFI SCANNER =================
void performWiFiScan() {
  int n = WiFi.scanNetworks(false, true); // Fast scan, hidden networks
  
  if (n == 0) {
    metrics.wifiAPScore = max(0, metrics.wifiAPScore - 5);
  } else {
    metrics.wifiAPScore = min(100, metrics.wifiAPScore + (n * 2));
    
    for (int i = 0; i < n && i < 10; i++) {
      // Update channel stats
      int channel = WiFi.channel(i);
      if (channel >= 1 && channel <= 13) {
        channelStats[channel].activity++;
        channelStats[channel].rssiSum += WiFi.RSSI(i);
        channelStats[channel].rssiCount++;
        channelStats[channel].lastSeen = millis();
        
        // Check for drone SSIDs
        String ssid = WiFi.SSID(i);
        if (ssid.indexOf("DJI") >= 0 || ssid.indexOf("FPV") >= 0 || 
            ssid.indexOf("Drone") >= 0 || ssid.indexOf("Quad") >= 0) {
          metrics.droneConfidence = min(100, metrics.droneConfidence + 20);
        }
        
        // Extract MAC (first 3 bytes for OUI check)
        String bssid = WiFi.BSSIDstr(i);
        checkMACVendor(bssid);
        
        // Burst detection
        uint32_t now = millis();
        if (now - lastBurstTime < 100) {
          burstCounter++;
          if (burstCounter > 3) {
            metrics.burstActivity = min(100, metrics.burstActivity + 10);
          }
        } else {
          burstCounter = 0;
        }
        lastBurstTime = now;
      }
    }
  }
  
  // Clear scan results to free memory
  WiFi.scanDelete();
}

void checkMACVendor(String mac) {
  // Extract OUI (first 8 chars: "XX:XX:XX")
  String oui = mac.substring(0, 8);
  oui.toUpperCase();
  
  for (int i = 0; i < sizeof(droneOUIs)/sizeof(droneOUIs[0]); i++) {
    if (oui == droneOUIs[i]) {
      metrics.droneConfidence = min(100, metrics.droneConfidence + 40);
      DBG("[VENDOR] Drone MAC detected: ");
      DBGL(mac);
      return;
    }
  }
  
  // Track unique MACs
  bool found = false;
  for (int i = 0; i < 20; i++) {
    if (recentMACs[i] == mac) {
      found = true;
      break;
    }
  }
  
  if (!found) {
    recentMACs[macIndex] = mac;
    macIndex = (macIndex + 1) % 20;
    deviceCount = min(deviceCount + 1, 20);
  }
}

// ================= RF24 ANALÝZA =================
void performRF24Scan() {
  int activeChannels[40];
  int activeCount = 0;
  
  // Fast channel scan
  for (int ch = 0; ch <= MAX_CHANNELS; ch++) {
    radio.setChannel(ch);
    radio.startListening();
    delayMicroseconds(RPD_DELAY_US);
    
    if (radio.testRPD()) {
      if (activeCount < 40) {
        activeChannels[activeCount++] = ch;
      }
      spectrumData[ch] = min(1023, spectrumData[ch] + 50);
      
      // Update channel stats
      channelStats[ch].activity++;
      channelStats[ch].lastSeen = millis();
    } else {
      spectrumData[ch] = max(0, spectrumData[ch] - 10);
    }
    
    radio.stopListening();
  }
  
  // Analýza aktivních kanálů
  if (activeCount > 0) {
    analyzeRFPattern(activeChannels, activeCount);
  }
  
  // Decay spectrum data
  for (int i = 0; i <= MAX_CHANNELS; i++) {
    spectrumData[i] = max(0, spectrumData[i] - 5);
    // Decay channel stats
    if (millis() - channelStats[i].lastSeen > 5000) {
      channelStats[i].activity = max(0, channelStats[i].activity - 1);
    }
  }
}

void analyzeRFPattern(int channels[], int count) {
  // Spočítej channel spread
  int minCh = channels[0];
  int maxCh = channels[count-1];
  int spread = maxCh - minCh;
  metrics.channelSpread = spread;
  
  // Detekce hopping patternu (typické pro FPV)
  if (count >= 3) {
    bool isHopping = true;
    int lastDiff = abs(channels[1] - channels[0]);
    
    for (int i = 2; i < count; i++) {
      int diff = abs(channels[i] - channels[i-1]);
      if (abs(diff - lastDiff) > 5) {
        isHopping = false;
        break;
      }
      lastDiff = diff;
    }
    
    if (isHopping && spread > 20) {
      metrics.fpvScore = min(100, metrics.fpvScore + 15);
      
      // Track hop sequence
      for (int i = 0; i < min(count, 20); i++) {
        hopSequence[hopIndex] = channels[i];
        hopIndex = (hopIndex + 1) % 20;
      }
    }
  }
  
  // Burst activity detection (FPV video)
  if (count > 5 && spread < 15) {
    metrics.burstActivity = min(100, metrics.burstActivity + 20);
  }
  
  // FPV band detection (RaceBand: 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120)
  int fpvMatches = 0;
  for (int i = 0; i < count; i++) {
    int ch = channels[i];
    if ((ch >= 32 && ch <= 88 && ch % 8 == 0) || 
        (ch >= 96 && ch <= 120 && ch % 8 == 0)) {
      fpvMatches++;
    }
  }
  
  if (fpvMatches >= 2) {
    metrics.fpvScore = min(100, metrics.fpvScore + 25);
  }
  
  // BLE detection (channels 37, 38, 39)
  int bleMatches = 0;
  for (int i = 0; i < count; i++) {
    if (channels[i] >= 37 && channels[i] <= 39) {
      bleMatches++;
    }
  }
  
  if (bleMatches > 0) {
    metrics.bleScore = min(100, metrics.bleScore + (bleMatches * 10));
  }
  
  // Update consistency history
  uint32_t now = millis();
  channelHistory[historyIndex] = count;
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;
  
  int consistency = 0;
  for (int i = 0; i < HISTORY_SIZE; i++) {
    if (channelHistory[i] > 0) consistency++;
  }
  metrics.consistency = (consistency * 100) / HISTORY_SIZE;
}

// ================= METRICS CALCULATION =================
void calculateMetrics() {
  // Calculate noise level
  int totalActivity = 0;
  for (int i = 0; i <= MAX_CHANNELS; i++) {
    totalActivity += channelStats[i].activity;
  }
  metrics.noiseLevel = min(100, totalActivity / 10);
  
  // Combine scores
  int combinedScore = 0;
  combinedScore += metrics.fpvScore * 0.4;
  combinedScore += (100 - metrics.wifiAPScore) * 0.3; // Inverse - less WiFi = more drone-like
  combinedScore += metrics.consistency * 0.2;
  combinedScore += metrics.burstActivity * 0.1;
  
  metrics.droneConfidence = min(100, combinedScore);
  
  // Alert logic
  bool wasAlertActive = metrics.alertActive;
  
  // Complex alert conditions
  bool fpvCondition = metrics.fpvScore > 60 && metrics.consistency > 70;
  bool burstCondition = metrics.burstActivity > 50 && metrics.channelSpread < 30;
  bool vendorCondition = metrics.droneConfidence > 70;
  
  metrics.alertActive = fpvCondition || burstCondition || vendorCondition;
  
  if (metrics.alertActive && !wasAlertActive) {
    metrics.detectionStart = millis();
    DBGL("[ALERT] Drone-like activity detected!");
  }
}

// ================= DISPLAY FUNCTIONS =================
void updateWaterfall() {
  // Shift waterfall up
  for (int y = 31; y > 0; y--) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      waterfall[x][y] = waterfall[x][y-1];
    }
  }
  
  // Nový řádek z spectrum data
  for (int x = 0; x < SCREEN_WIDTH; x++) {
    int ch = map(x, 0, SCREEN_WIDTH-1, 0, MAX_CHANNELS);
    int intensity = min(255, spectrumData[ch] / 4);
    
    // Color coding
    if (ch >= 37 && ch <= 39) {
      // BLE kanály - nižší intenzita
      waterfall[x][0] = intensity / 2;
    } else if (ch >= 1 && ch <= 13) {
      // WiFi kanály - střední intenzita
      waterfall[x][0] = intensity;
    } else if ((ch >= 32 && ch <= 88 && ch % 8 == 0) || 
               (ch >= 96 && ch <= 120 && ch % 8 == 0)) {
      // FPV kanály - vysoká intenzita
      waterfall[x][0] = min(255, intensity * 3 / 2);
    } else {
      waterfall[x][0] = intensity;
    }
  }
}

void drawSpectrumGraph() {
  int barWidth = SCREEN_WIDTH / 40;
  
  for (int i = 0; i < 40; i++) {
    int startCh = i * 3;
    int endCh = startCh + 2;
    int activity = 0;
    
    for (int ch = startCh; ch <= endCh && ch <= MAX_CHANNELS; ch++) {
      activity += channelStats[ch].activity;
    }
    
    int barHeight = constrain(activity / 5, 0, 30);
    
    if (barHeight > 0) {
      int xPos = i * barWidth;
      
      // Barva podle typu aktivity
      bool isWiFi = (startCh >= 1 && endCh <= 13);
      bool isBLE = (startCh >= 37 && endCh <= 39);
      bool isFPV = false;
      
      // Check for FPV channels in this range
      for (int ch = startCh; ch <= endCh; ch++) {
        if ((ch >= 32 && ch <= 88 && ch % 8 == 0) || 
            (ch >= 96 && ch <= 120 && ch % 8 == 0)) {
          isFPV = true;
          break;
        }
      }
      
      if (isFPV && barHeight > 10) {
        // FPV - plný obdélník
        display.fillRect(xPos, 31 - barHeight, barWidth-1, barHeight, SSD1306_WHITE);
      } else if (isWiFi) {
        // WiFi - obrys obdélníku
        display.drawRect(xPos, 31 - barHeight, barWidth-1, barHeight, SSD1306_WHITE);
      } else if (isBLE) {
        // BLE - čárkovaná čára
        display.drawFastVLine(xPos + barWidth/2, 31 - barHeight, barHeight, SSD1306_WHITE);
      } else {
        // Ostatní - tenká čára
        display.drawFastVLine(xPos + 1, 31 - barHeight, barHeight, SSD1306_WHITE);
      }
    }
  }
}

void drawDashboard() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Horní status bar
  display.setCursor(0, 0);
  display.print("D:");
  display.print(metrics.droneConfidence);
  display.print("% F:");
  display.print(metrics.fpvScore);
  display.print("%");
  
  display.setCursor(70, 0);
  display.print("C:");
  display.print(metrics.consistency);
  display.print("%");
  
  display.setCursor(100, 0);
  display.print("B:");
  display.print(metrics.burstActivity);
  
  // Spectrum graph
  drawSpectrumGraph();
  
  // Separator
  display.drawFastHLine(0, 31, SCREEN_WIDTH, SSD1306_WHITE);
  
  // Waterfall
  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      if (waterfall[x][y] > 40) {
        display.drawPixel(x, y + 32, SSD1306_WHITE);
      }
    }
  }
  
  // Bottom status line
  display.setCursor(0, 56);
  
  if (metrics.alertActive) {
    uint32_t alertDuration = (millis() - metrics.detectionStart) / 1000;
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    
    if (metrics.fpvScore > 70 && metrics.consistency > 80) {
      display.print(">>> FPV VIDEO LINK <<<");
    } else if (metrics.droneConfidence > 80) {
      display.print(">>> DRONE DETECTED <<<");
    } else if (metrics.droneConfidence > 60) {
      display.print(">> SUSPECTED DRONE <<");
    } else {
      display.print(">> RF ACTIVITY <<");
    }
    
    if (alertDuration > 0) {
      display.setCursor(100, 56);
      display.print(alertDuration);
      display.print("s");
    }
  } else if (metrics.fpvScore > 30) {
    display.print("FPV-like signal detected");
  } else if (metrics.wifiAPScore > 30) {
    display.print("WiFi Networks: ");
    display.print(deviceCount);
  } else if (metrics.bleScore > 20) {
    display.print("BLE Devices Active");
  } else {
    display.print("Scanning 2.4GHz...");
  }
  
  display.display();
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  DBGL("\n==========================================");
  DBGL("   ADVANCED DRONE DETECTOR v4.1");
  DBGL("   Compatible ESP32-S3 Version");
  DBGL("==========================================");
  
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Startup sequence
  beep(1500, 80);
  delay(60);
  beep(2000, 80);
  delay(60);
  beep(2500, 120);
  
  // Initialize OLED
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      DBGL("[OLED] Not found!");
      // Continue without OLED
    }
  } else {
    DBGL("[OLED] Initialized OK");
  }
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 10);
  display.println("DRONE");
  display.setCursor(20, 30);
  display.println("DETECTOR");
  display.setTextSize(1);
  display.setCursor(30, 50);
  display.println("ESP32-S3 v4.1");
  display.display();
  delay(1500);
  
  // Initialize RF24
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CSN_PIN);
  
  if (!radio.begin()) {
    DBGL("[ERROR] nRF24 not found!");
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(10, 25);
    display.println("nRF24 ERROR");
    display.display();
    
    // Alarm beeps
    for (int i = 0; i < 3; i++) {
      beep(500, 300);
      delay(300);
    }
  } else {
    DBGL("[RF24] Initialized OK");
    
    radio.setAutoAck(false);
    radio.disableCRC();
    radio.setDataRate(RF24_1MBPS);
    radio.setPALevel(RF24_PA_LOW);
    radio.stopListening();
  }
  
  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  DBGL("[WiFi] Scanner ready");
  DBGL("[SYS] All systems ready");
  
  // Successful start beep
  beep(3200, 200);
  
  display.clearDisplay();
  display.display();
}

// ================= MAIN LOOP =================
void loop() {
  unsigned long loopStart = millis();
  scanCount++;
  
  // 1. RF24 Spectrum Scan (každých 50ms)
  performRF24Scan();
  
  // 2. WiFi Scan (každých 2 sekundy)
  if (loopStart - lastWiFiScan >= 2000) {
    performWiFiScan();
    calculateMetrics();
    lastWiFiScan = loopStart;
  }
  
  // 3. Update Display (každých 150ms)
  if (loopStart - lastDisplayUpdate >= 150) {
    updateWaterfall();
    drawDashboard();
    lastDisplayUpdate = loopStart;
  }
  
  // 4. Alert Sounds
  if (metrics.alertActive) {
    uint32_t alertDuration = (loopStart - metrics.detectionStart);
    int beepInterval;
    
    if (metrics.fpvScore > 70) {
      // FPV video link - fast beeping
      beepInterval = map(constrain(alertDuration, 0, 10000), 0, 10000, 500, 100);
    } else if (metrics.droneConfidence > 80) {
      // High confidence - medium beeping
      beepInterval = 300;
    } else {
      // Low confidence - slow beeping
      beepInterval = 1000;
    }
    
    if (loopStart - lastBeep >= beepInterval) {
      int freq = 3500;
      if (metrics.fpvScore > 70) freq = 4000;
      beep(freq, 100, true);
      lastBeep = loopStart;
    }
  } else if (metrics.fpvScore > 40) {
    // Periodic beep for FPV-like signals
    if (loopStart - lastBeep >= 2000) {
      beep(2800, 50, true);
      lastBeep = loopStart;
    }
  }
  
  // 5. Debug Output
  #if DEBUG
  if (scanCount % 50 == 0) {
    DBG("[STATS] Scan: ");
    DBG(scanCount);
    DBG(" | Drone: ");
    DBG(metrics.droneConfidence);
    DBG("% | FPV: ");
    DBG(metrics.fpvScore);
    DBG("% | WiFi: ");
    DBG(deviceCount);
    DBG(" | BLE: ");
    DBG(metrics.bleScore);
    DBG(" | Noise: ");
    DBG(metrics.noiseLevel);
    DBG("% | Cons: ");
    DBG(metrics.consistency);
    DBG("%");
    
    // Show RF activity
    int totalRF = 0;
    for (int i = 0; i <= MAX_CHANNELS; i++) {
      if (spectrumData[i] > 100) totalRF++;
    }
    
    if (totalRF > 0) {
      DBG(" | RF Channels: ");
      DBG(totalRF);
    }
    
    DBGL("");
    
    // Show FPV channel activity
    if (metrics.fpvScore > 30) {
      DBG("[FPV] Active FPV channels: ");
      for (int ch = 32; ch <= 120; ch += 8) {
        if (spectrumData[ch] > 200) {
          DBG(ch);
          DBG(" ");
        }
      }
      DBGL("");
    }
  }
  
  if (scanCount % 200 == 0) {
    DBG("[MEMORY] Free: ");
    DBG(ESP.getFreeHeap());
    DBGL(" bytes");
  }
  #endif
  
  // 6. Dynamic Delay
  unsigned long loopTime = millis() - loopStart;
  if (loopTime < SCAN_INTERVAL_MS) {
    delay(SCAN_INTERVAL_MS - loopTime);
  }
}