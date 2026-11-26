/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

#include "setting.h"
#include "icon.h"
#include "config.h"

RF24 RadioA(NRF_CE_PIN_A, NRF_CSN_PIN_A);
RF24 RadioB(NRF_CE_PIN_B, NRF_CSN_PIN_B);
RF24 RadioC(NRF_CE_PIN_C, NRF_CSN_PIN_C);

void setRadiosNeutralState() {
  RadioA.stopListening();
  RadioA.setAutoAck(false);
  RadioA.setRetries(0, 0);
  RadioA.powerDown(); 
  digitalWrite(NRF_CE_PIN_A, LOW); 

  RadioB.stopListening();
  RadioB.setAutoAck(false);
  RadioB.setRetries(0, 0);
  RadioB.powerDown(); 
  digitalWrite(NRF_CE_PIN_B, LOW); 

  RadioC.stopListening();
  RadioC.setAutoAck(false);
  RadioC.setRetries(0, 0);
  RadioC.powerDown(); 
  digitalWrite(NRF_CE_PIN_C, LOW); 
}

void configureNrf(RF24 &radio) {
  radio.begin();
  radio.setAutoAck(false);
  radio.stopListening();
  radio.setRetries(0, 0);
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_2MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
}

void setupRadioA() {
  configureNrf(RadioA);
}

void setupRadioB() {
  configureNrf(RadioB);
}

void setupRadioC() {
  configureNrf(RadioC);
}

void initAllRadios() {
  setupRadioA();
  setupRadioB();
  setupRadioC();
}

void Str(uint8_t x, uint8_t y, const uint8_t* asciiArray, size_t len) {
  char buf[64]; 
  for (size_t i = 0; i < len && i < sizeof(buf) - 1; i++) {
    buf[i] = (char)asciiArray[i];
  }
  buf[len] = '\0';

  u8g2.drawStr(x, y, buf);
}

void CenteredStr(uint8_t screenWidth, uint8_t y, const uint8_t* asciiArray, size_t len, const uint8_t* font) {
  char buf[64];
  for (size_t i = 0; i < len && i < sizeof(buf) - 1; i++) {
    buf[i] = (char)asciiArray[i];
  }
  buf[len] = '\0';

  u8g2.setFont((const uint8_t*)font);
  int16_t w = u8g2.getUTF8Width(buf);
  u8g2.setCursor((screenWidth - w) / 2, y);
  u8g2.print(buf);
}

void drawMarqueeStr(int x, int y, int boxW, int fontH, const char* text, unsigned long startTime, bool centerIfFit) {
  int textW = u8g2.getUTF8Width(text);
  if (textW <= boxW) {
    int drawX = centerIfFit ? (x + (boxW - textW) / 2) : x;
    u8g2.drawStr(drawX, y, text);
    return;
  }

  const int gap = 24;
  const unsigned long pauseMs = 800;
  const unsigned long speedMsPerPx = 45;

  unsigned long elapsed = millis() - startTime;
  int offset = 0;
  if (elapsed > pauseMs) {
    offset = ((elapsed - pauseMs) / speedMsPerPx) % (textW + gap);
  }

  int clipTop = max(0, y - fontH);
  int clipBottom = min(64, y + 2);
  u8g2.setClipWindow(x, clipTop, x + boxW, clipBottom);
  u8g2.drawStr(x - offset, y, text);
  u8g2.drawStr(x - offset + textW + gap, y, text);
  u8g2.setMaxClipWindow();
}

void utils() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  Str(7, 15, Line_A, sizeof(Line_A));
  Str(12, 35, Line_B, sizeof(Line_B));
  Str(7, 55, Line_C, sizeof(Line_C));
  u8g2.sendBuffer();
}

void conf() {
  u8g2.setBitmapMode(1);
  u8g2.clearBuffer();
  CenteredStr(128, 25, txt_n, sizeof(txt_n), u8g2_font_ncenB14_tr);
  CenteredStr(106, 40, txt_c, sizeof(txt_c), u8g2_font_ncenB08_tr);
  CenteredStr(128, 60, txt_v, sizeof(txt_v), u8g2_font_6x10_tf);
  u8g2.sendBuffer();
  delay(3000);
  u8g2.clearBuffer();
  u8g2.drawXBMP(0, 0, 128, 64, cred); 
  u8g2.sendBuffer();
  delay(250);
}

bool menuListMode = false;

namespace Setting {

int currentOption = 0;
int totalOptions = 4; 
unsigned long optionSelectTime = 0;

bool buttonUpPressed = false;
bool buttonDownPressed = false;
bool buttonSelectPressed = false;

void updateFirmware() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 15, "Dang cap nhat Firm..ware.");
  u8g2.sendBuffer();

  u8g2.setFont(u8g2_font_5x8_tr);
  if (!SD.begin(SD_CS_PIN)) {
    u8g2.drawStr(0, 30, "[ Loi the nho SD ]");
    u8g2.sendBuffer();
    delay(2000);
    return;
  }

  if (!SD.exists(FIRMWARE_FILE)) {
    u8g2.drawStr(0, 30, "[ Khong tim thay tep tin ]");
    u8g2.sendBuffer();
    delay(2000);
    return;
  }

  File firmware = SD.open(FIRMWARE_FILE);
  if (!firmware) {
    u8g2.drawStr(0, 30, "[ Loi mo file ]");
    u8g2.sendBuffer();
    delay(2000);
    return;
  }

  if (firmware) {
    u8g2.drawStr(0, 30, "[ Doi mot chut ]");
    u8g2.sendBuffer();
  }

  if (!Update.begin(firmware.size())) {
    u8g2.drawStr(0, 30, "[ Cap nhat loi ]");
    u8g2.sendBuffer();
    firmware.close();
    delay(2000);
    return;
  }

  Update.writeStream(firmware);
  if (Update.end(true)) {
    u8g2.drawStr(0, 45, "[ Cap nhat thanh cong! ]");
    u8g2.sendBuffer();
    delay(1000);
    ESP.restart();
  } else {
    u8g2.drawStr(0, 45, "[ Cap nhat loi! ]");
    u8g2.sendBuffer();
    delay(2000);
  }

  firmware.close();
}

void toggleOption(int option) {
  if (option == 0) { 
    neoPixelActive = !neoPixelActive;
    EEPROM.write(EEPROM_ADDRESS_NEOPIXEL, neoPixelActive);
    EEPROM.commit();
    Serial.print("NeoPixel hien tai ");
    Serial.println(neoPixelActive ? "BAT" : "TAT");

  } else if (option == 1) { 
    uint8_t brightnessPercent = map(oledBrightness, 0, 255, 0, 100); 
    brightnessPercent += 10; 
    if (brightnessPercent > 100) brightnessPercent = 0; 
    oledBrightness = map(brightnessPercent, 0, 100, 0, 255); 

    u8g2.setContrast(oledBrightness); 
    EEPROM.write(EEPROM_ADDRESS_BRIGHTNESS, oledBrightness);
    EEPROM.commit();

    Serial.print("Do sang hien tai: ");
    Serial.print(brightnessPercent);
    Serial.println("%");

  } else if (option == 2) {
    menuListMode = !menuListMode;
    EEPROM.write(EEPROM_ADDRESS_MENU_STYLE, menuListMode ? 1 : 0);
    EEPROM.commit();

    Serial.print("Kieu Menu hien tai: ");
    Serial.println(menuListMode ? "LIST" : "LUOI");

  } else if (option == 3) {
    updateFirmware();
  }
}

void handleButtons() {
  if (!digitalRead(BUTTON_UP_PIN)) {
    if (!buttonUpPressed) {
      buttonUpPressed = true;
      currentOption = (currentOption - 1 + totalOptions) % totalOptions;
      optionSelectTime = millis();
    }
  } else {
    buttonUpPressed = false;
  }

  if (!digitalRead(BUTTON_DOWN_PIN)) {
    if (!buttonDownPressed) {
      buttonDownPressed = true;
      currentOption = (currentOption + 1) % totalOptions;
      optionSelectTime = millis();
    }
  } else {
    buttonDownPressed = false;
  }

  if (!digitalRead(BTN_PIN_RIGHT)) {
    if (!buttonSelectPressed) {
      buttonSelectPressed = true;
      toggleOption(currentOption);
    }
  } else {
    buttonSelectPressed = false;
  }
}

void displayMenu() {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "Cai dat:");
  u8g2.drawHLine(0, 12, 128);

  u8g2.setFont(u8g2_font_5x8_tr);
  if (currentOption == 0) {
    u8g2.drawStr(0, 24, ">");
    drawMarqueeStr(10, 24, 68, 8, "NeoPixel:", optionSelectTime);
  } else {
    u8g2.drawStr(0, 24, "  NeoPixel: ");
  }

  if (currentOption == 1) {
    u8g2.drawStr(0, 36, ">");
    drawMarqueeStr(10, 36, 68, 8, "Do sang:", optionSelectTime);
  } else {
    u8g2.drawStr(0, 36, "  Do sang: ");
  }

  if (currentOption == 2) {
    u8g2.drawStr(0, 48, ">");
    drawMarqueeStr(10, 48, 68, 8, "Kieu Menu:", optionSelectTime);
  } else {
    u8g2.drawStr(0, 48, "  Kieu Menu:");
  }

  if (currentOption == 3) {
    u8g2.drawStr(0, 60, ">");
    drawMarqueeStr(10, 60, 118, 8, "Cap nhat Firmware", optionSelectTime);
  } else {
    u8g2.drawStr(0, 60, "  Cap nhat Firmware");
  }

  u8g2.setCursor(82, 24);
  u8g2.print(neoPixelActive ? "BAT" : "TAT");

  u8g2.setCursor(82, 36);
  uint8_t brightnessPercent = map(oledBrightness, 0, 255, 0, 100);
  u8g2.print(brightnessPercent);
  u8g2.print("%");

  u8g2.setCursor(82, 48);
  u8g2.print(menuListMode ? "LIST" : "LUOI");

  u8g2.sendBuffer();
}

void settingSetup() {
  Serial.begin(115200);

  EEPROM.begin(512);

  neoPixelActive = EEPROM.read(EEPROM_ADDRESS_NEOPIXEL);
  oledBrightness = EEPROM.read(EEPROM_ADDRESS_BRIGHTNESS);
  menuListMode = (EEPROM.read(EEPROM_ADDRESS_MENU_STYLE) == 1);
  
  if (oledBrightness > 255) oledBrightness = 128; 
  u8g2.setContrast(oledBrightness);

  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_PIN_RIGHT, INPUT_PULLUP);
  optionSelectTime = millis();
}

void settingLoop() {
  handleButtons();
  displayMenu();
  }
} 
