/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

#include "icon.h"
#include "setting.h"
#include "config.h"

extern uint8_t oledBrightness;

const int NUM_ITEMS = 12;
const int MAX_ITEM_LENGTH = 32;

const unsigned char* bitmap_icons[NUM_ITEMS] = {
  bitmap_icon_scanner, bitmap_icon_analyzer, bitmap_icon_jammer, bitmap_icon_kill,
  bitmap_icon_ble_jammer, bitmap_icon_spoofer, bitmap_icon_apple, bitmap_icon_ble,
  bitmap_icon_wifi, bitmap_icon_wifi_jammer, bitmap_icon_about, 
  bitmap_icon_setting
};

char menu_items[NUM_ITEMS][MAX_ITEM_LENGTH] = {  
  "QUET SONG 2.4G (Scanner)", "PHAN TICH PHO (Analyzer)", "PHA SONG WLAN (WLAN Jammer)", "PHA GIAO THUC (Proto Kill)", "PHA SONG BLE (BLE Jammer)",
  "GIA LAP BLE (BLE Spoofer)", "SPAM APPLE (Sour Apple)", "QUET BLE (BLE Scan)", "QUET WIFI (WiFi Scan)", 
  "NGAT KET NOI WIFI (Deauther)", "THONG TIN (About)", "CAI DAT (Setting)"
};

void (*menu_functions[NUM_ITEMS])() = {
  Scanner::scannerSetup, Analyzer::analyzerSetup, Jammer::jammerSetup,
  ProtoKill::blackoutSetup, BleJammer::blejammerSetup, Spoofer::spooferSetup,
  SourApple::sourappleSetup, BleScan::blescanSetup, WifiScan::wifiscanSetup, Deauther::deautherSetup,
  utils, Setting::settingSetup
};

void (*menu_loop_functions[NUM_ITEMS])() = {
  Scanner::scannerLoop, Analyzer::analyzerLoop, Jammer::jammerLoop,
  ProtoKill::blackoutLoop, BleJammer::blejammerLoop, Spoofer::spooferLoop,
  SourApple::sourappleLoop, BleScan::blescanLoop, WifiScan::wifiscanLoop, Deauther::deautherLoop,
  nullptr, Setting::settingLoop
};

int item_selected = 0;
int listStartItem = 0;
int current_screen = 0;
unsigned long last_button_time = 0;
unsigned long menuSelectTime = 0;
unsigned long lastMarqueeUpdate = 0;
bool showItemOnTopBar = false;
const unsigned long DEBOUNCE_DELAY = 150; 
const unsigned long POST_PRESS_DELAY = 200;
const unsigned long TOP_BAR_TIMEOUT = 10000;

void drawTopBar() {
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 9);
  u8g2.setDrawColor(0);

  if (!menuListMode && showItemOnTopBar) {
    drawMarqueeStr(2, 7, 124, 7, menu_items[item_selected], menuSelectTime, true);
  } else {
    char versionStr[16];
    size_t vLen = min(sizeof(txt_v), sizeof(versionStr) - 1);
    for (size_t i = 0; i < vLen; i++) {
      versionStr[i] = (char)txt_v[i];
    }
    versionStr[vLen] = '\0';

    Str(2, 7, txt_n, sizeof(txt_n));
    int version_width = u8g2.getUTF8Width(versionStr);
    Str(128 - version_width - 2, 7, txt_v, sizeof(txt_v));
  }

  u8g2.setDrawColor(1);
}

void drawListMenu() {
  const int visible_rows = 3;
  const int row_h = 18;
  const int start_y = 10;

  if (item_selected < listStartItem) {
    listStartItem = item_selected;
  } else if (item_selected >= listStartItem + visible_rows) {
    listStartItem = item_selected - visible_rows + 1;
  }
  if (listStartItem < 0) listStartItem = 0;
  if (listStartItem > NUM_ITEMS - visible_rows) listStartItem = max(0, NUM_ITEMS - visible_rows);

  u8g2.setFont(u8g2_font_5x8_tf);
  for (int r = 0; r < visible_rows; r++) {
    int idx = listStartItem + r;
    if (idx >= NUM_ITEMS) break;

    int row_y = start_y + r * row_h;
    u8g2.drawXBMP(3, row_y + 1, 16, 16, bitmap_icons[idx]);

    if (idx == item_selected) {
      u8g2.drawRFrame(0, row_y, 122, row_h, 3);
      drawMarqueeStr(22, row_y + 12, 98, 8, menu_items[idx], menuSelectTime, false);
    } else {
      u8g2.setClipWindow(22, row_y, 120, row_y + row_h);
      u8g2.drawStr(22, row_y + 12, menu_items[idx]);
      u8g2.setMaxClipWindow();
    }
  }

  const int scroll_y = 11;
  const int scroll_h = 52;
  u8g2.drawFrame(124, scroll_y, 4, scroll_h);
  int bar_height = max(6, (scroll_h * visible_rows) / NUM_ITEMS);
  int max_start = max(1, NUM_ITEMS - visible_rows);
  int bar_y = scroll_y + (listStartItem * (scroll_h - bar_height)) / max_start;
  u8g2.drawBox(124, bar_y, 4, bar_height);
}
                
void drawMenu() {
  u8g2.clearBuffer();
  if (current_screen != 0) return;

  drawTopBar();

  if (menuListMode) {
    drawListMenu();
    u8g2.sendBuffer();
    setRadiosNeutralState();
    return;
  }

  const int icons_per_row = 3;
  const int icons_per_col = 2;
  const int max_display_items = icons_per_row * icons_per_col; 

  int selected_col = item_selected % icons_per_row;
  int selected_row = (item_selected / icons_per_row) % icons_per_col;
  if (item_selected == 12) {
    selected_row = 1; 
  }
  int highlight_x = 13 + selected_col * 40;
  int highlight_y = 15 + selected_row * 27;

  int start_row = (item_selected / icons_per_row) - selected_row;
  if (start_row < 0) start_row = 0;
  int total_rows = (NUM_ITEMS + icons_per_row - 1) / icons_per_row;
  if (start_row > total_rows - icons_per_col) start_row = total_rows - icons_per_col;
  if (start_row < 0) start_row = 0; 
  int start_item = start_row * icons_per_row;
  int end_item = min(NUM_ITEMS, start_item + max_display_items);

  for (int i = start_item; i < end_item; i++) {
    int idx = i - start_item; 
    int row = idx / icons_per_row;
    int col = idx % icons_per_row;
    int x_pos = 13 + col * 40;
    int y_pos = 15 + row * 27;
    u8g2.drawXBMP(x_pos, y_pos, 16, 16, bitmap_icons[i]);
  }

  u8g2.drawRFrame(highlight_x - 3, highlight_y - 3, 22, 22, 3); 
  u8g2.setDrawColor(0);
  u8g2.drawRFrame(highlight_x - 2, highlight_y - 2, 22, 22, 3); 
  u8g2.setDrawColor(1);

  u8g2.setFont(u8g2_font_5x7_tf);
  const int scroll_y = 16;
  const int scroll_h = 40;
  u8g2.drawFrame(124, scroll_y, 4, scroll_h); 
  int bar_height = (scroll_h * icons_per_col) / total_rows;
  int step_y = scroll_h / total_rows;
  u8g2.drawBox(124, scroll_y + (step_y * start_row), 4, bar_height); 

  if (start_row > 0) {
    u8g2.drawStr(124, 14, "."); 
  }
  if (start_row < total_rows - icons_per_col) {
    u8g2.drawStr(124, 62, "."); 
  }
  u8g2.sendBuffer();

  setRadiosNeutralState();
}

bool readButton(int pin) {
  if (digitalRead(pin) == LOW && (millis() - last_button_time > DEBOUNCE_DELAY)) {
    last_button_time = millis();
    delay(POST_PRESS_DELAY); 
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  neopixelSetup();
  initAllRadios();
  EEPROM.begin(512);
  oledBrightness = EEPROM.read(EEPROM_ADDRESS_BRIGHTNESS);
  menuListMode = (EEPROM.read(EEPROM_ADDRESS_MENU_STYLE) == 1);
  u8g2.begin();
  u8g2.setContrast(oledBrightness);
  conf();
  pinMode(BUTTON_UP_PIN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT_PIN, INPUT_PULLUP);
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_PIN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_PIN_LEFT, INPUT_PULLUP);
  showItemOnTopBar = false;
  menuSelectTime = millis();
  lastMarqueeUpdate = menuSelectTime;
  drawMenu();
}

void loop() {
  if (current_screen == 0) {
    const int icons_per_row = 3;

    if (readButton(BTN_PIN_LEFT)) {
      if (menuListMode) {
        item_selected = (item_selected - 1 + NUM_ITEMS) % NUM_ITEMS;
      } else {
        item_selected = max(0, item_selected - 1);
      }
      showItemOnTopBar = true;
      menuSelectTime = millis();
      lastMarqueeUpdate = menuSelectTime;
      drawMenu();
    }
    if (readButton(BTN_PIN_RIGHT)) {
      if (menuListMode) {
        item_selected = (item_selected + 1) % NUM_ITEMS;
      } else {
        item_selected = min(NUM_ITEMS - 1, item_selected + 1);
      }
      showItemOnTopBar = true;
      menuSelectTime = millis();
      lastMarqueeUpdate = menuSelectTime;
      drawMenu();
    }
    if (readButton(BUTTON_UP_PIN)) {
      if (menuListMode) {
        item_selected = (item_selected - 1 + NUM_ITEMS) % NUM_ITEMS;
      } else {
        item_selected = max(0, item_selected - icons_per_row);
      }
      showItemOnTopBar = true;
      menuSelectTime = millis();
      lastMarqueeUpdate = menuSelectTime;
      drawMenu();
    }
    if (readButton(BUTTON_DOWN_PIN)) {
      if (menuListMode) {
        item_selected = (item_selected + 1) % NUM_ITEMS;
      } else {
        item_selected = min(NUM_ITEMS - 1, item_selected + icons_per_row);
      }
      showItemOnTopBar = true;
      menuSelectTime = millis();
      lastMarqueeUpdate = menuSelectTime;
      drawMenu();
    }
    if (readButton(BUTTON_SELECT_PIN)) {
      current_screen = 1;
      for (int cycle = 0; cycle < 2; cycle++) { 
        for (int i = 0; i < 3; i++) {
          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_6x10_tr);
          u8g2.drawStr(30, 32, "DANG TAI");
    
          String dots = "";
          for (int j = 0; j <= i; j++) {
            dots += ".";
            setNeoPixelColour("white");
          }
          setNeoPixelColour("0");
          
          u8g2.drawStr(80, 32, dots.c_str()); 
    
          u8g2.sendBuffer();
          delay(200); 
        }
      }
      menu_functions[item_selected]();
      
      while (current_screen == 1) {
        if (menu_loop_functions[item_selected]) {
          menu_loop_functions[item_selected]();
        }
        if (readButton(BUTTON_SELECT_PIN)) {
          current_screen = 0;
          break;
        }
      }
      showItemOnTopBar = true;
      menuSelectTime = millis();
      lastMarqueeUpdate = menuSelectTime;
      drawMenu();
    }

    if (menuListMode) {
      if (millis() - lastMarqueeUpdate >= 45) {
        lastMarqueeUpdate = millis();
        u8g2.setFont(u8g2_font_5x8_tf);
        if (u8g2.getUTF8Width(menu_items[item_selected]) > 98) {
          int selected_row = item_selected - listStartItem;
          int row_y = 10 + selected_row * 18;
          u8g2.setDrawColor(0);
          u8g2.drawBox(22, row_y + 2, 98, 14);
          u8g2.setDrawColor(1);
          drawMarqueeStr(22, row_y + 12, 98, 8, menu_items[item_selected], menuSelectTime, false);
          u8g2.sendBuffer();
        }
      }
    } else if (showItemOnTopBar) {
      if (millis() - menuSelectTime >= TOP_BAR_TIMEOUT) {
        showItemOnTopBar = false;
        drawTopBar();
        u8g2.sendBuffer();
      } else if (millis() - lastMarqueeUpdate >= 45) {
        lastMarqueeUpdate = millis();
        u8g2.setFont(u8g2_font_5x7_tf);
        if (u8g2.getUTF8Width(menu_items[item_selected]) > 124) {
          drawTopBar();
          u8g2.sendBuffer();
        }
      }
    }
  }
}
