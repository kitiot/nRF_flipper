/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

#include "config.h"
#include "icon.h"
#include "setting.h"

namespace Analyzer {
  #define NRF24_CONFIG  0x00
  #define NRF24_EN_AA   0x01
  #define NRF24_RF_CH   0x05
  #define NRF24_RF_SETUP 0x06
  #define NRF24_RPD     0x09

  #define N 128
  uint8_t values[N];

  #define CHANNELS 64
  int channel[CHANNELS]; 

  byte getregister(byte r) {
    byte c;
    digitalWrite(NRF_CSN_PIN_A, LOW);
    SPI.transfer(r & 0x1F);
    c = SPI.transfer(0);
    digitalWrite(NRF_CSN_PIN_A, HIGH);
    return c;
  }

  void setregister(byte r, byte v) {
    digitalWrite(NRF_CSN_PIN_A, LOW);
    SPI.transfer((r & 0x1F) | 0x20);
    SPI.transfer(v);
    digitalWrite(NRF_CSN_PIN_A, HIGH);
  }

  void powerup(void) {
    setregister(NRF24_CONFIG, getregister(NRF24_CONFIG) | 0x02);
    delayMicroseconds(130);
  }

  void powerdown(void) {
    setregister(NRF24_CONFIG, getregister(NRF24_CONFIG) & ~0x02);
  }

  void ENable(void) {
    digitalWrite(NRF_CE_PIN_A, HIGH);
  }

  void DIsable(void) {
    digitalWrite(NRF_CE_PIN_A, LOW);
  }

  void setrx(void) {
    setregister(NRF24_CONFIG, getregister(NRF24_CONFIG) | 0x01);
    ENable();
    delayMicroseconds(100);
  }

  void ScanChannels(void) {
    DIsable();
    // Uncomment if needed: for (int j = 0; j < 10; j++) {
      for (int i = 0; i < CHANNELS; i++) {
        setNeoPixelColour("purple");
        RadioA.setChannel((128 * i) / CHANNELS); 
        setrx();
        delayMicroseconds(40);
        DIsable();
        if (getregister(NRF24_RPD) > 0) channel[i]++;
      }
      setNeoPixelColour("0");
    // }
  }

  void writeRegister(uint8_t reg, uint8_t value) {
    digitalWrite(NRF_CSN_PIN_A, LOW);
    SPI.transfer(reg | 0x20);
    SPI.transfer(value);
    digitalWrite(NRF_CSN_PIN_A, HIGH);
  }

  uint8_t readRegister(uint8_t reg) {
    digitalWrite(NRF_CSN_PIN_A, LOW);
    SPI.transfer(reg & 0x1F);
    uint8_t result = SPI.transfer(0x00);
    digitalWrite(NRF_CSN_PIN_A, HIGH);
    return result;
  }

  void setChannel(uint8_t channel) {
    writeRegister(NRF24_RF_CH, channel);
  }

  void powerUP() {
    uint8_t config = readRegister(NRF24_CONFIG);
    writeRegister(NRF24_CONFIG, config | 0x02);
    delay(5);
  }

  void powerDOWN() {
    uint8_t config = readRegister(NRF24_CONFIG);
    writeRegister(NRF24_CONFIG, config & ~0x02);
  }

  void startListening() {
    digitalWrite(NRF_CE_PIN_A, HIGH);
  }

  void stopListening() {
    digitalWrite(NRF_CE_PIN_A, LOW);
  }

  bool carrierDetected() {
    return readRegister(NRF24_RPD) & 0x01;
  }

  void analyzerSetup() {
    Serial.begin(115200);

    esp_bt_controller_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();

    SPI.begin(18, 19, 23, NRF_CSN_PIN_A);
    SPI.setDataMode(SPI_MODE0);
    SPI.setFrequency(10000000);
    SPI.setBitOrder(MSBFIRST);

    digitalWrite(NRF_CSN_PIN_A, HIGH);
    digitalWrite(NRF_CE_PIN_A, LOW);

    RadioA.begin(); 
    DIsable();

    powerUP();
    writeRegister(NRF24_EN_AA, 0x00);
    writeRegister(NRF24_RF_SETUP, 0x03);
  }

  void analyzerLoop() {
    static unsigned long lastDebounceTimeSelect = 0;
    const unsigned long debounceDelay = 200; 

    ScanChannels();

    if (digitalRead(BUTTON_SELECT_PIN) == LOW) {
      unsigned long currentMillis = millis();
      if ((currentMillis - lastDebounceTimeSelect) > debounceDelay) {
        Serial.println("Da nhan phím Select, thoat khoi PHAN TICH PHO");
        lastDebounceTimeSelect = currentMillis;
        return; 
      }
    }

    memset(values, 0, sizeof(uint8_t)*N);

    int n = 200;
    while (n--) {
      if (digitalRead(BUTTON_SELECT_PIN) == LOW) {
        unsigned long currentMillis = millis();
        if ((currentMillis - lastDebounceTimeSelect) > debounceDelay) {
          Serial.println("Da nhan phím Select, thoat khoi PHAN TICH PHO");
          lastDebounceTimeSelect = currentMillis;
          break; 
        }
      }
      int i = N;
      while (i--) {
        if (digitalRead(BUTTON_SELECT_PIN) == LOW) {
          unsigned long currentMillis = millis();
          if ((currentMillis - lastDebounceTimeSelect) > debounceDelay) {
            Serial.println("Da nhan phím Select, thoat khoi PHAN TICH PHO");
            lastDebounceTimeSelect = currentMillis;
            return; 
          }
        }
        setChannel(i);
        startListening();
        delayMicroseconds(128);
        stopListening();
        if (carrierDetected()) {
          ++values[i];
        }
      }
    }
    
    if (digitalRead(BUTTON_SELECT_PIN) == LOW && (millis() - lastDebounceTimeSelect) > debounceDelay) {
      return;
    }

    u8g2.clearBuffer();
    int barWidth = SCREEN_WIDTH / N;
    int x = 0;
    for (int i = 0; i < N; ++i) {
      int v = 63 - values[i] * 3;
      if (v < 0) {
        v = 0;
      }
      u8g2.drawVLine(x, v - 10, 64 - v);
      x += barWidth;
    }

    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setCursor(0, 64);
    u8g2.print("1...5...10...25..50...80...128");
    u8g2.sendBuffer();

    //delay(50);
  }
}

namespace ProtoKill {

  enum OperationMode {WiFi_MODULE, VIDEO_TX_MODULE, RC_MODULE, BLE_MODULE, Bluetooth_MODULE, USB_WIRELESS_MODULE, ZIGBEE_MODULE, NRF24_MODULE};
  OperationMode current_Mode = WiFi_MODULE;

  enum Operation {DEACTIVE_MODE, ACTIVE_MODE};
  Operation current = DEACTIVE_MODE;

  const byte bluetooth_channels[] = {
     2, 28, 54,  4, 30, 56,  6, 32, 58,  8,
    34, 60, 10, 36, 62, 12, 38, 64, 14, 40,
    66, 16, 42, 68, 18, 44, 70, 20, 46, 72,
    22, 48, 74, 24, 50, 76, 26, 52, 78, 80
  };
  const byte ble_channels[] = {2, 26, 80};
  const byte WiFi_channels[] = {12, 17, 22, 27, 32, 37, 42, 47, 52, 57, 62, 67, 72};
  const byte usbWireless_channels[] = {40, 50, 60};
  const byte videoTransmitter_channels[] = {70, 75, 80};
  const byte rc_channels[] = {1, 3, 5, 7};
  const byte zigbee_channels[] = {5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80};
  const byte nrf24_channels[] = {76, 78, 79};

  bool radioAStarted = false;
  bool radioBStarted = false;
  bool radioCStarted = false;
  bool radiosActive  = false;
  size_t sweepIdx    = 0;

  // Biến đọc nút dạng polling (thay cho interrupt)
  bool lastLeftState  = HIGH;
  bool lastRightState = HIGH;
  bool lastUpState    = HIGH;
  unsigned long lastPressTime = 0;
  const unsigned long debounceDelay = 180;


  bool configure_Radio(RF24 &radio, byte initialChannel) {
    if (!radio.begin()) return false;
    radio.setAutoAck(false);
    radio.stopListening();
    radio.setRetries(0, 0);
    radio.setPALevel(RF24_PA_MAX, true);
    radio.setDataRate(RF24_2MBPS);
    radio.setCRCLength(RF24_CRC_DISABLED);
    radio.startConstCarrier(RF24_PA_MAX, initialChannel);
    return true;
  }

  void stopAllRadios() {
    RadioA.stopConstCarrier(); RadioA.powerDown(); digitalWrite(NRF_CE_PIN_A, LOW);
    RadioB.stopConstCarrier(); RadioB.powerDown(); digitalWrite(NRF_CE_PIN_B, LOW);
    RadioC.stopConstCarrier(); RadioC.powerDown(); digitalWrite(NRF_CE_PIN_C, LOW);
    radioAStarted = radioBStarted = radioCStarted = radiosActive = false;
    delay(50);
  }

  void initialize_Radios() {
    if (current == ACTIVE_MODE) {
      if (!radiosActive) {
        radioAStarted = configure_Radio(RadioA, 2);
        radioBStarted = configure_Radio(RadioB, 26);
        radioCStarted = configure_Radio(RadioC, 80);
        radiosActive  = (radioAStarted || radioBStarted || radioCStarted);
      }
    } else {
      stopAllRadios();
    }
  }
  void update_OLED() {
    u8g2.setBitmapMode(1);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tr);

    // Thanh trạng thái trên cùng
    u8g2.setCursor(75, 7);
    u8g2.print(current == ACTIVE_MODE ? "-BAT-" : "-TAT-");
    u8g2.drawHLine(0, 9, 128);

    u8g2.setCursor(0, 7);
    u8g2.print("[");

    // Đẩy mũi tên & icon lên y = 0 (không còn lệch +5)
    u8g2.drawXBMP(0, 0, 128, 64, bitmap_arrow_left);
    u8g2.drawXBMP(0, 0, 128, 64, bitmap_arrow_right);

    const char* infoStr = "";
    switch (current_Mode) {
      case WiFi_MODULE:
        u8g2.drawXBMP(0, 0, 128, 64, bitmap_wifi_jammer);
        u8g2.print("WiFi");
        infoStr = "13 KENH 2412-72";
        break;
      case VIDEO_TX_MODULE:
        u8g2.drawXBMP(0, 0, 128, 64, bitmap_cctv);
        u8g2.print("Video TX");
        infoStr = "3 KENH 2470-80";
        break;
      case RC_MODULE:
        u8g2.drawXBMP(0, 0, 128, 64, bitmap_rc);
        u8g2.print("RC");
        infoStr = "4 KENH 2401-07";
        break;
      case BLE_MODULE:
        u8g2.drawXBMP(0, 0, 128, 64, bitmap_ble_jammer);
        u8g2.print("BLE");
        infoStr = "3 KENH 2402/26/80";
        break;
      case Bluetooth_MODULE:
        u8g2.drawXBMP(0, 0, 128, 64, bitmap_bluetooth_jammer);
        u8g2.print("Bluetooth");
        infoStr = "40 KENH 2402-80";
        break;
      case USB_WIRELESS_MODULE:
        u8g2.drawXBMP(0, 0, 128, 64, bitmap_usb);
        u8g2.print("USB Wireless");
        infoStr = "3 KENH 2440-60";
        break;
      case ZIGBEE_MODULE:
        u8g2.drawXBMP(0, 0, 128, 64, bitmap_zigbee);
        u8g2.print("Zigbee");
        infoStr = "16 KENH 2405-80";
        break;
      case NRF24_MODULE:
        u8g2.drawXBMP(0, 0, 128, 64, bitmap_nrf24);
        u8g2.print("NRF24");
        infoStr = "3 KENH 2476-79";
        break;
    }
    u8g2.print("]");

    int w = u8g2.getUTF8Width(infoStr);
    u8g2.drawStr((128 - w) / 2, 63, infoStr);
    u8g2.sendBuffer();
  }

  void checkButtons() {
    unsigned long now = millis();
    bool left  = digitalRead(BTN_PIN_LEFT);
    bool right = digitalRead(BTN_PIN_RIGHT);
    bool up    = digitalRead(BUTTON_UP_PIN);

    if (now - lastPressTime > debounceDelay) {
      if (left == LOW && lastLeftState == HIGH) {
        lastPressTime = now;
        current_Mode = static_cast<OperationMode>((current_Mode == 0) ? 7 : (current_Mode - 1));
        sweepIdx = 0;
        update_OLED();
      } else if (right == LOW && lastRightState == HIGH) {
        lastPressTime = now;
        current_Mode = static_cast<OperationMode>((current_Mode + 1) % 8);
        sweepIdx = 0;
        update_OLED();
      } else if (up == LOW && lastUpState == HIGH) {
        lastPressTime = now;
        current = (current == DEACTIVE_MODE) ? ACTIVE_MODE : DEACTIVE_MODE;
        initialize_Radios();
        update_OLED();
      }
    }
    lastLeftState  = left;
    lastRightState = right;
    lastUpState    = up;
  }

  void stepRadios(const byte* channels, size_t count) {
    if (count == 0) return;
    sweepIdx = (sweepIdx + 1) % count;
    size_t idxB = (sweepIdx + count / 3) % count;
    size_t idxC = (sweepIdx + (2 * count) / 3) % count;

    if (radioAStarted) RadioA.setChannel(channels[sweepIdx]);
    if (radioBStarted) RadioB.setChannel(channels[idxB]);
    if (radioCStarted) RadioC.setChannel(channels[idxC]);
  }

  void blackoutSetup() {
    Serial.begin(115200);

    esp_bt_controller_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_wifi_disconnect();

    pinMode(BTN_PIN_LEFT, INPUT_PULLUP);
    pinMode(BTN_PIN_RIGHT, INPUT_PULLUP);
    pinMode(BUTTON_UP_PIN, INPUT_PULLUP);

    current = DEACTIVE_MODE;
    sweepIdx = 0;
    lastLeftState  = HIGH;
    lastRightState = HIGH;
    lastUpState    = HIGH;

    stopAllRadios();
    update_OLED();
  }

  void blackoutLoop() {
    checkButtons();

    if (current == DEACTIVE_MODE || !radiosActive) {
      return;
    }

    switch (current_Mode) {
      case WiFi_MODULE:         stepRadios(WiFi_channels, sizeof(WiFi_channels)); break;
      case VIDEO_TX_MODULE:     stepRadios(videoTransmitter_channels, sizeof(videoTransmitter_channels)); break;
      case RC_MODULE:           stepRadios(rc_channels, sizeof(rc_channels)); break;
      case BLE_MODULE:          stepRadios(ble_channels, sizeof(ble_channels)); break;
      case Bluetooth_MODULE:    stepRadios(bluetooth_channels, sizeof(bluetooth_channels)); break;
      case USB_WIRELESS_MODULE: stepRadios(usbWireless_channels, sizeof(usbWireless_channels)); break;
      case ZIGBEE_MODULE:       stepRadios(zigbee_channels, sizeof(zigbee_channels)); break;
      case NRF24_MODULE:        stepRadios(nrf24_channels, sizeof(nrf24_channels)); break;
    }

    delayMicroseconds(35);
  }
}

namespace Scanner {
  #define CHANNELS 64
  int channel[CHANNELS];

  int line;
  char grey[] = " .:-=+*aRW";

  #define _NRF24_CONFIG   0x00
  #define _NRF24_EN_AA    0x01
  #define _NRF24_RF_CH    0x05
  #define _NRF24_RF_SETUP 0x06
  #define _NRF24_RPD      0x09

  #define EEPROM_ADDRESS_SENSOR_ARRAY 2

  byte sensorArray[129];

  unsigned long lastSaveTime = 0;
  const unsigned long saveInterval = 5000;

  byte getRegister(byte r) {
    byte c;
    digitalWrite(NRF_CSN_PIN_A, LOW);
    SPI.transfer(r & 0x1F);
    c = SPI.transfer(0);
    digitalWrite(NRF_CSN_PIN_A, HIGH);
    return c;
  }

  void setRegister(byte r, byte v) {
    digitalWrite(NRF_CSN_PIN_A, LOW);
    SPI.transfer((r & 0x1F) | 0x20);
    SPI.transfer(v);
    digitalWrite(NRF_CSN_PIN_A, HIGH);
  }

  void powerUp(void) {
    setRegister(_NRF24_CONFIG, getRegister(_NRF24_CONFIG) | 0x02);
    delayMicroseconds(130);
  }

  void powerDown(void) {
    setRegister(_NRF24_CONFIG, getRegister(_NRF24_CONFIG) & ~0x02);
  }

  void enable(void) {
    digitalWrite(NRF_CE_PIN_A, HIGH);
  }

  void disable(void) {
    digitalWrite(NRF_CE_PIN_A, LOW);
  }

  void setRX(void) {
    setRegister(_NRF24_CONFIG, getRegister(_NRF24_CONFIG) | 0x01);
    enable();
    delayMicroseconds(100);
  }

  void scanChannels(void) {
    disable();

    memset(channel, 0, sizeof(channel));

    const int samplesPerChannel = 50;

    for (int i = 0; i < CHANNELS; i++) {
      RadioA.setChannel((128 * i) / CHANNELS); // Use RF24 setChannel

      for (int j = 0; j < samplesPerChannel; j++) {
        // Check Select button in inner loop
        static unsigned long lastDebounceTimeSelect = 0;
        const unsigned long debounceDelay = 200;
        
        if (digitalRead(BUTTON_SELECT_PIN) == LOW) {
          unsigned long currentMillis = millis();
          if ((currentMillis - lastDebounceTimeSelect) > debounceDelay) {
            Serial.println("Nhan nut Select, thoat khoi scanChannels");
            lastDebounceTimeSelect = currentMillis;
            return; // Exit immediately
          }
        }
        setRX();
        delayMicroseconds(100);
        disable();
        channel[i] += getRegister(_NRF24_RPD);
      }

      channel[i] = (channel[i] * 100) / samplesPerChannel;
    }
  }

  void outputChannels(void) {
    int norm = 0;

    for (int i = 0; i < CHANNELS; i++) {
      if (channel[i] > norm) {
        norm = channel[i];
      }
    }

    byte drawHeight = map(norm, 0, 64, 0, 64);

    for (byte count = 126; count > 0; count--) {
      sensorArray[count] = sensorArray[count - 1];
    }
    sensorArray[0] = drawHeight;

    u8g2.clearBuffer();

    u8g2.drawLine(0, 0, 0, 63);
    u8g2.drawLine(127, 0, 127, 63);

    for (byte count = 0; count < 64; count += 10) {
      u8g2.drawLine(127, count, 122, count);
      u8g2.drawLine(0, count, 5, count);
    }

    for (byte count = 10; count < 127; count += 10) {
      u8g2.drawPixel(count, 0);
      u8g2.drawPixel(count, 63);
    }

    for (byte count = 0; count < 127; count++) {
      u8g2.drawLine(127 - count, 63, 127 - count, 63 - sensorArray[count]);
      setNeoPixelColour("purple");
    }

    setNeoPixelColour("0");

    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setCursor(12, 12);
    u8g2.print("[");
    u8g2.print(norm);
    u8g2.print("]");

    u8g2.sendBuffer();
  }

  void loadPreviousGraph() {
    EEPROM.begin(128);
    for (byte i = 0; i < 128; i++) {
      sensorArray[i] = EEPROM.read(EEPROM_ADDRESS_SENSOR_ARRAY + i);
    }
    EEPROM.end();
  }

  void saveGraphToEEPROM() {
    EEPROM.begin(128);
    for (byte i = 0; i < 128; i++) {
      EEPROM.write(EEPROM_ADDRESS_SENSOR_ARRAY + i, sensorArray[i]);
    }
    EEPROM.commit();
    EEPROM.end();
  }

  void scannerSetup() {
    Serial.begin(115200);

    esp_bt_controller_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();

    for (byte count = 0; count <= 128; count++) {
      sensorArray[count] = 0;
    }

    SPI.begin(18, 19, 23, NRF_CSN_PIN_A);
    SPI.setDataMode(SPI_MODE0);
    SPI.setFrequency(16000000);
    SPI.setBitOrder(MSBFIRST);

    RadioA.begin(); // Initialize RadioA
    disable();

    powerUp();
    setRegister(_NRF24_EN_AA, 0x0);
    setRegister(_NRF24_RF_SETUP, 0x0F);

    loadPreviousGraph();
  }

  void scannerLoop() {
    static unsigned long lastDebounceTimeSelect = 0;
    const unsigned long debounceDelay = 200; // Match Jammer/Analyzer

    // Check Select button before scanning
    if (digitalRead(BUTTON_SELECT_PIN) == LOW) {
      unsigned long currentMillis = millis();
      if ((currentMillis - lastDebounceTimeSelect) > debounceDelay) {
        Serial.println("Nhan nut Select, thoat khoi Scanner");
        lastDebounceTimeSelect = currentMillis;
        return; // Exit to main loop
      }
    }

    scanChannels();

    // Check Select button after scanning
    if (digitalRead(BUTTON_SELECT_PIN) == LOW) {
      unsigned long currentMillis = millis();
      if ((currentMillis - lastDebounceTimeSelect) > debounceDelay) {
        Serial.println("Nhan nut Select, thoat khoi Scanner");
        lastDebounceTimeSelect = currentMillis;
        return; // Exit to main loop
      }
    }

    outputChannels();

    // Skip EEPROM save if Select was pressed
    if (digitalRead(BUTTON_SELECT_PIN) == LOW && (millis() - lastDebounceTimeSelect) > debounceDelay) {
      return;
    }

    if (millis() - lastSaveTime > saveInterval) {
      saveGraphToEEPROM();
      lastSaveTime = millis();
    }
  }
}

namespace Jammer {

  const int num_channels = 64;
  int value[num_channels];
  int valuesDisplay[32];
  int channels = 1;
  const int num_reps = 50;
  bool jamming = false;
  const byte address[6] = "00001";

  const byte wlanchannels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
  const byte wifi_channels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 1, 2, 3, 4, 5, 6, 7};

  byte wifiGroup1[] = {1, 2, 3, 4};
  byte wifiGroup2[] = {5, 6, 7, 8};
  byte wifiGroup3[] = {9, 10, 11, 12};

  uint8_t dataRateIndex = 0;
  uint8_t paLevelIndex = 0;

  int menuIndex = 0;
  const char* menuItems[] = {"Channel:", "PA Level:", "Data Rate:", "Jamming:"};
  const int menuSize = 4;

  unsigned long lastDebounceTimeBT1 = 0;
  unsigned long lastDebounceTimeBT2 = 0;
  unsigned long lastDebounceTimeBT3 = 0;
  unsigned long lastDebounceTimeBT4 = 0;
  unsigned long debounceDelay = 200;

  void setRadioParameters() {
    switch (dataRateIndex) {
      case 0: RadioA.setDataRate(RF24_250KBPS); RadioB.setDataRate(RF24_250KBPS); RadioC.setDataRate(RF24_250KBPS); break;
      case 1: RadioA.setDataRate(RF24_1MBPS); RadioB.setDataRate(RF24_1MBPS); RadioC.setDataRate(RF24_1MBPS); break;
      case 2: RadioA.setDataRate(RF24_2MBPS); RadioB.setDataRate(RF24_2MBPS); RadioC.setDataRate(RF24_2MBPS); break;
    }

    switch (paLevelIndex) {
      case 0: RadioA.setPALevel(RF24_PA_MIN); RadioB.setPALevel(RF24_PA_MIN); RadioC.setPALevel(RF24_PA_MIN); break;
      case 1: RadioA.setPALevel(RF24_PA_LOW); RadioB.setPALevel(RF24_PA_LOW); RadioC.setPALevel(RF24_PA_LOW); break;
      case 2: RadioA.setPALevel(RF24_PA_HIGH); RadioB.setPALevel(RF24_PA_HIGH); RadioC.setPALevel(RF24_PA_HIGH); break;
      case 3: RadioA.setPALevel(RF24_PA_MAX); RadioB.setPALevel(RF24_PA_MAX); RadioC.setPALevel(RF24_PA_MAX); break;
    }
  }

  void radioSetChannel(int channels) {
    RadioA.setChannel(channels);
    RadioB.setChannel(channels);
    RadioC.setChannel(channels);
  }



  void changeChannelNext() {
    channels = (channels % 14) + 1;
    Serial.println("Channel: " + String(channels));
  }

  void changeChannelPrev() {
    channels = (channels - 2 + 14) % 14 + 1;
    Serial.println("Channel: " + String(channels));
  }

  void toggleJamming() {
    jamming = !jamming;
    Serial.println(jamming ? "Jamming started" : "Jamming stopped");
    setNeoPixelColour(jamming ? "red" : "0");
    if (jamming) {
      RadioA.powerUp();
      RadioB.powerUp();
      RadioC.powerUp();
      setRadioParameters();
    } else {
      RadioA.stopListening();
      RadioA.powerDown();
      RadioB.stopListening();
      RadioB.powerDown();
      RadioC.stopListening();
      RadioC.powerDown();
    }
  }

  void changeDataRateNext() {
    dataRateIndex = (dataRateIndex + 1) % 3;
    setRadioParameters();
    Serial.println("Data rate changed to: " + String(dataRateIndex));
  }

  void changeDataRatePrev() {
    dataRateIndex = (dataRateIndex - 1 + 3) % 3;
    setRadioParameters();
    Serial.println("Data rate changed to: " + String(dataRateIndex));
  }

  void changePALevelNext() {
    paLevelIndex = (paLevelIndex + 1) % 4;
    setRadioParameters();
    Serial.println("PA level changed to: " + String(paLevelIndex));
  }

  void changePALevelPrev() {
    paLevelIndex = (paLevelIndex - 1 + 4) % 4;
    setRadioParameters();
    Serial.println("PA level changed to: " + String(paLevelIndex));
  }

  void navigateUp() {
    menuIndex = (menuIndex - 1 + menuSize) % menuSize;
    Serial.println("Navigate Up: menuIndex = " + String(menuIndex));
  }

  void navigateDown() {
    menuIndex = (menuIndex + 1) % menuSize;
    Serial.println("Navigate Down: menuIndex = " + String(menuIndex));
  }

  void changeOptionRight() {
    if (menuIndex == 0) {
      changeChannelNext();
    } else if (menuIndex == 1) {
      changePALevelNext();
    } else if (menuIndex == 2) {
      changeDataRateNext();
    } else if (menuIndex == 3) {
      toggleJamming();
    }
  }

  void changeOptionLeft() {
    if (menuIndex == 0) {
      changeChannelPrev();
    } else if (menuIndex == 1) {
      changePALevelPrev();
    } else if (menuIndex == 2) {
      changeDataRatePrev();
    } else if (menuIndex == 3) {
      toggleJamming();
    }
  }

  void handleButtonPress(int pin, unsigned long &lastDebounceTime, void (*callback)()) {
    int reading = digitalRead(pin);
    if (reading == LOW) {
      unsigned long currentTime = millis();
      if ((currentTime - lastDebounceTime) > debounceDelay) {
        Serial.println("Button press: Pin=" + String(pin) + ", Callback=" +
                       (pin == BUTTON_UP_PIN ? "navigateUp" :
                        pin == BTN_PIN_RIGHT ? "changeOptionRight" :
                        pin == BUTTON_DOWN_PIN ? "navigateDown" : "changeOptionLeft"));
        callback();
        lastDebounceTime = currentTime;
      }
    }
  }


  void jammer() {
    int methode = 1;

    if (methode == 1) {
      const char text[] = { 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55,
                            0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55 };
      for (int i = ((channels * 5) + 1); i < ((channels * 5) + 23); i++) {
        // Check buttons to allow immediate deactivation
        handleButtonPress(BTN_PIN_RIGHT, lastDebounceTimeBT2, changeOptionRight);
        handleButtonPress(BTN_PIN_LEFT, lastDebounceTimeBT4, changeOptionLeft);
        if (!jamming) break; // Exit loop if jamming is deactivated
        radioSetChannel(i);
        RadioA.write(&text, sizeof(text));
        RadioB.write(&text, sizeof(text));
        RadioC.write(&text, sizeof(text));
      }
    } else if (methode == 2) {
      // Check buttons to allow immediate deactivation
      handleButtonPress(BTN_PIN_RIGHT, lastDebounceTimeBT2, changeOptionRight);
      handleButtonPress(BTN_PIN_LEFT, lastDebounceTimeBT4, changeOptionLeft);
      if (!jamming) return; // Exit early if jamming is deactivated
      radioSetChannel(channels);
      int randomIndex = random(0, sizeof(wlanchannels) / sizeof(wlanchannels[0]));
      bool resultA = RadioA.write(&randomIndex, sizeof(randomIndex));
      bool resultB = RadioB.write(&randomIndex, sizeof(randomIndex));
      bool resultC = RadioC.write(&randomIndex, sizeof(randomIndex));
    } else if (methode == 3) {
      for (int i = 0; i < 22; i++) {
        // Check buttons to allow immediate deactivation
        handleButtonPress(BTN_PIN_RIGHT, lastDebounceTimeBT2, changeOptionRight);
        handleButtonPress(BTN_PIN_LEFT, lastDebounceTimeBT4, changeOptionLeft);
        if (!jamming) break; // Exit loop if jamming is deactivated
        int channelA = ((channels * 5) + 1) + i;
        int channelB = ((channels * 5) + 1) + i + 1;
        int channelC = ((channels * 5) + 1) + i + 2;

        int randomIndex = random(0, sizeof(wlanchannels) / sizeof(wlanchannels[0]));

        RadioA.setChannel(channelA);
        RadioB.setChannel(channelB);
        RadioC.setChannel(channelC);

        RadioA.write(&randomIndex, sizeof(randomIndex));
        RadioB.write(&randomIndex, sizeof(randomIndex));
        RadioC.write(&randomIndex, sizeof(randomIndex));
      }
    }
  }
  

  void updateDisplay() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_profont11_tf);

    u8g2.drawHLine(0, 0, 4); u8g2.drawVLine(0, 0, 4);
    u8g2.drawHLine(124, 0, 4); u8g2.drawVLine(127, 0, 4);
    u8g2.drawHLine(0, 63, 4); u8g2.drawVLine(0, 60, 4);
    u8g2.drawHLine(124, 63, 4); u8g2.drawVLine(127, 60, 4);

    int xshift = 5;
    u8g2.setFont(u8g2_font_profont11_tf);
    for (int i = 0; i < menuSize; i++) {
      int y = (i == 0) ? 10 : (i == 1) ? 25 : (i == 2) ? 40 : 60;
      if (menuIndex == i) {
        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawStr(0 + xshift, y - 1, ">");
        u8g2.setFont(u8g2_font_profont11_tf);
        u8g2.drawStr(8 + xshift, y, menuItems[i]);
      } else {
        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawStr(8 + xshift, y, menuItems[i]);
      }

      u8g2.setCursor(75 + xshift, y);
      u8g2.setFont(u8g2_font_5x7_tf);
      if (i == 0) {
        u8g2.print("[");
        u8g2.print(channels);
        u8g2.print("]");
      } else if (i == 1) {
        switch (paLevelIndex) {
          case 0: u8g2.print("MIN"); break;
          case 1: u8g2.print("LOW"); break;
          case 2: u8g2.print("HIGH"); break;
          case 3: u8g2.print("MAX"); break;
        }
      } else if (i == 2) {
        switch (dataRateIndex) {
          case 0: u8g2.print("250KBPS"); break;
          case 1: u8g2.print("1MBPS"); break;
          case 2: u8g2.print("2MBPS"); break;
        }
      } else {
        u8g2.print(jamming ? "Kich hoat" : "Vo hieu");
      }
    }
    u8g2.sendBuffer();
  }

  void configure(RF24 &radio) {
    radio.begin();
    radio.openWritingPipe(0xFFFFFFFFFF);
    radio.setAutoAck(false);
    radio.stopListening();
    radio.setRetries(0, 0);
    radio.setPALevel(RF24_PA_MAX, true);
    radio.setDataRate(RF24_2MBPS);
    radio.setCRCLength(RF24_CRC_DISABLED);
    radio.printPrettyDetails();
  }

  void jammerSetup() {
    Serial.begin(115200);

    esp_bt_controller_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();

    pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
    pinMode(BTN_PIN_RIGHT, INPUT_PULLUP);
    pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
    pinMode(BTN_PIN_LEFT, INPUT_PULLUP);

    SPI.begin();

    configure(RadioA);
    configure(RadioB);
    configure(RadioC);

    setRadioParameters();

    u8g2.begin();
    updateDisplay();
  }

  void jammerLoop() {
    handleButtonPress(BUTTON_UP_PIN, lastDebounceTimeBT1, navigateUp);
    handleButtonPress(BTN_PIN_RIGHT, lastDebounceTimeBT2, changeOptionRight);
    handleButtonPress(BUTTON_DOWN_PIN, lastDebounceTimeBT3, navigateDown);
    handleButtonPress(BTN_PIN_LEFT, lastDebounceTimeBT4, changeOptionLeft);

    updateDisplay();

    if (jamming) {
      jammer();
    }

    delay(50);
  }
}
