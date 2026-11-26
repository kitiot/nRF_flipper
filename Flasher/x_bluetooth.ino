#include "RF24.h"
#include <SPI.h>
#include <ezButton.h>
#include "esp_bt.h"
#include "esp_wifi.h"

// ================== CẤU HÌNH CHÂN ==================
#define HP_CE      16
#define HP_CSN     15

#define SP_CE      22
#define SP_CSN     21

#define BUTTON_PIN 33            // Nút bấm đổi Mode (Nhấn nhả: Đổi mode | Nhấn giữ: Tắt về Mode 0)

#define LED_R      25            // Chân LED Đỏ
#define LED_G      26            // Chân LED Xanh lá
#define LED_B      27            // Chân LED Xanh dương

constexpr int SPI_SPEED = 16000000;

// ========== BỘ CÂN CHỈNH ĐỘ SÁNG 3 MÀU LED (0.0 -> 1.0) ==========
// Nếu lắp trở xong mà màu Đỏ vẫn hơi lấn át màu Vàng/Hồng, bạn chỉ cần giảm CALIB_R xuống!
constexpr float CALIB_R = 0.45f; // Hãm bớt màu Đỏ xuống 45% để pha màu Vàng & Hồng chuẩn xác
constexpr float CALIB_G = 1.00f; // Giữ 100% công suất cho Xanh lá
constexpr float CALIB_B = 1.00f; // Giữ 100% công suất cho Xanh dương
// =================================================================

SPIClass *sp = nullptr;
SPIClass *hp = nullptr;
RF24 radio(HP_CE, HP_CSN, SPI_SPEED);   // HP module (HSPI)
RF24 radio1(SP_CE, SP_CSN, SPI_SPEED); // SP module (VSPI)

// MẢNG BLUETOOTH 10/10 (40 kênh phủ kín 2402-2480MHz, đan xen 3 phân vùng)
constexpr uint8_t bluetooth_channels[] = {
   2, 28, 54,  4, 30, 56,  6, 32, 58,  8,
  34, 60, 10, 36, 62, 12, 38, 64, 14, 40,
  66, 16, 42, 68, 18, 44, 70, 20, 46, 72,
  22, 48, 74, 24, 50, 76, 26, 52, 78, 80
};

// MẢNG BLE 10/10 (3 kênh quảng cáo cốt lõi)
constexpr uint8_t ble_channels[] = {2, 26, 80};

constexpr size_t NUM_BT_CHANNELS  = sizeof(bluetooth_channels) / sizeof(bluetooth_channels[0]);
constexpr size_t NUM_BLE_CHANNELS = sizeof(ble_channels) / sizeof(ble_channels[0]);

int currentMode = 1;             // Mặc định vào thẳng Mode 1
const int TOTAL_MODES = 8;       // Tổng cộng 8 mode (0 đến 7)
ezButton modeButton(BUTTON_PIN);

// Trạng thái khởi động của từng module
bool hpStarted = false;
bool spStarted = false;

// Biến quản lý thời gian cho LED & Nút bấm
unsigned long lastLedUpdate = 0;
unsigned long lastColorChangeTime = 0;
unsigned long modeChangeTime = 0;
unsigned long buttonPressTime = 0;
bool longPressHandled = false;
int autoColorIndex = 0;

// Biến đếm kênh tuần tự (Split-Band Sequential Sweep)
// Đặt lệch pha 2 ăng-ten ngay từ đầu để không bao giờ phát trùng kênh
size_t btIndexHP  = 0;
size_t btIndexSP  = NUM_BT_CHANNELS / 2; // Lệch đúng nửa mảng (20 kênh)
size_t bleIndexHP = 0;
size_t bleIndexSP = 1;

// ========== CẤU HÌNH PWM CHO ESP32 (API chuẩn v3.x) ==========
const int freq = 5000;      
const int resolution = 8;   

void setupPWM() {
  ledcAttach(LED_R, freq, resolution);
  ledcAttach(LED_G, freq, resolution);
  ledcAttach(LED_B, freq, resolution);
}

// Hàm xuất màu LED có tích hợp sẵn hệ số cân bằng màu
inline void setRGBColor(int r, int g, int b) {
  ledcWrite(LED_R, (int)(r * CALIB_R));
  ledcWrite(LED_G, (int)(g * CALIB_G));
  ledcWrite(LED_B, (int)(b * CALIB_B));
}

// ========== QUẢN LÝ MÀU SẮC & HIỆU ỨNG THỞ MƯỢT MÀ ==========
void updateLEDStatus() {
  unsigned long currentMillis = millis();

  // 1. CẢ HAI LỖI -> Nhấp nháy Đỏ rực
  if (!hpStarted && !spStarted) {
    if ((currentMillis / 250) % 2 == 0) ledcWrite(LED_R, 255);
    else ledcWrite(LED_R, 0);
    ledcWrite(LED_G, 0);
    ledcWrite(LED_B, 0);
    return;
  }

  // 2. HP LỖI -> Nhấp nháy Xanh dương
  if (!hpStarted) {
    setRGBColor(0, 0, ((currentMillis / 350) % 2 == 0) ? 255 : 0);
    return;
  }

  // 3. SP LỖI -> Nhấp nháy Xanh lá
  if (!spStarted) {
    setRGBColor(0, ((currentMillis / 350) % 2 == 0) ? 255 : 0, 0);
    return;
  }

  // 4. Sáng rõ màu mới trong 400ms đầu ngay khi nhấn nút để nhận biết tức thì
  unsigned long elapsed = currentMillis - modeChangeTime;
  int brightness;
  if (elapsed < 400) {
    brightness = 255;
  } else {
    // Sau 400ms, trượt mượt từ đỉnh 255 vào chu kỳ thở bằng hàm cos()
    float breath = (cos((elapsed - 400) / 450.0) + 1.0) / 2.0;
    brightness = (int)(breath * 255);
  }

  // 5. HIỂN THỊ MÀU CHUẨN XÁC THEO 8 MODE
  switch (currentMode) {
    case 0: // Tắt (Standby)
      setRGBColor(0, 0, 0);
      break;
    case 1: // Jam BLE -> Xanh dương thuần
      setRGBColor(0, 0, brightness);
      break;
    case 2: // Jam Bluetooth -> Xanh lá thuần
      setRGBColor(0, brightness, 0);
      break;
    case 3: // Jam All -> Đỏ thuần
      setRGBColor(brightness, 0, 0);
      break;
    case 4: // Mode Hồng / Tím (Magenta: Đỏ + Xanh dương cân bằng)
      setRGBColor(brightness, 0, brightness);
      break;
    case 5: // Mode Vàng chuẩn (Yellow: Đỏ + Xanh lá cân bằng)
      setRGBColor(brightness, brightness, 0);
      break;
    case 6: // Mode Xanh ngọc (Cyan: Xanh lá + Xanh dương)
      setRGBColor(0, brightness, brightness);
      break;
    case 7: // CHẾ ĐỘ AUTO-CYCLE: Tự động xoay vòng 6 màu mỗi 2 giây
      if (currentMillis - lastColorChangeTime >= 2000) {
        lastColorChangeTime = currentMillis;
        autoColorIndex = (autoColorIndex + 1) % 6;
      }
      switch (autoColorIndex) {
        case 0: setRGBColor(0, 0, brightness); break;              // Xanh dương
        case 1: setRGBColor(0, brightness, 0); break;              // Xanh lá
        case 2: setRGBColor(brightness, 0, 0); break;              // Đỏ
        case 3: setRGBColor(brightness, 0, brightness); break;     // Hồng
        case 4: setRGBColor(brightness, brightness, 0); break;     // Vàng
        case 5: setRGBColor(0, brightness, brightness); break;     // Xanh ngọc
      }
      break;
  }
}

// ========== KHỞI TẠO 2 ANTENNA ==========
void configureRadio() {
  hp = new SPIClass(HSPI);
  hp->begin();
  if (radio.begin(hp)) {
    Serial.println("HP Started !!!");
    hpStarted = true;
    radio.setAutoAck(false);
    radio.stopListening();
    radio.setRetries(0, 0);
    radio.setPALevel(RF24_PA_MAX, true);
    radio.setDataRate(RF24_2MBPS);
    radio.setCRCLength(RF24_CRC_DISABLED);
    radio.startConstCarrier(RF24_PA_HIGH, bluetooth_channels[0]);
  } else {
    Serial.println("HP couldn't start !!!");
    hpStarted = false;
  }

  sp = new SPIClass(VSPI);
  sp->begin();
  if (radio1.begin(sp)) {
    Serial.println("SP Started !!!");
    spStarted = true;
    radio1.setAutoAck(false);
    radio1.stopListening();
    radio1.setRetries(0, 0);
    radio1.setPALevel(RF24_PA_MAX, true);
    radio1.setDataRate(RF24_2MBPS);
    radio1.setCRCLength(RF24_CRC_DISABLED);
    radio1.startConstCarrier(RF24_PA_HIGH, ble_channels[0]);
  } else {
    Serial.println("SP couldn't start !!!");
    spStarted = false;
  }
}

// ========== CƠ CHẾ QUÉT PHÂN TÁCH DẢI & TUẦN TỰ (KHÔNG TRÙNG KÊNH) ==========
inline void jamBLE_Optimized() {
  // 2 ăng-ten luôn khóa vào 2 kênh quảng cáo BLE khác nhau
  bleIndexHP = (bleIndexHP + 1) % NUM_BLE_CHANNELS;
  bleIndexSP = (bleIndexHP + 1) % NUM_BLE_CHANNELS;
  radio.setChannel(ble_channels[bleIndexHP]);
  radio1.setChannel(ble_channels[bleIndexSP]);
}

inline void jamBluetooth_Optimized() {
  // 2 ăng-ten duyệt tuần tự cách nhau đúng nửa bảng kênh (không bao giờ trùng)
  btIndexHP = (btIndexHP + 1) % NUM_BT_CHANNELS;
  btIndexSP = (btIndexSP + 1) % NUM_BT_CHANNELS;
  radio.setChannel(bluetooth_channels[btIndexHP]);
  radio1.setChannel(bluetooth_channels[btIndexSP]);
}

inline void jamAll_Optimized() {
  // HP càn quét toàn bộ 40 kênh Bluetooth, cùng lúc SP khóa chặt 3 kênh BLE
  btIndexHP  = (btIndexHP + 1) % NUM_BT_CHANNELS;
  bleIndexSP = (bleIndexSP + 1) % NUM_BLE_CHANNELS;
  radio.setChannel(bluetooth_channels[btIndexHP]);
  radio1.setChannel(ble_channels[bleIndexSP]);
}

void executeMode() {
  if (!hpStarted || !spStarted) return; 

  switch (currentMode) {
    case 0: break; // Standby
    case 1: jamBLE_Optimized(); break;
    case 2: jamBluetooth_Optimized(); break;
    case 3: jamAll_Optimized(); break;
    case 4: jamAll_Optimized(); break;       // Mode Hồng: Đa nhiệm BT + BLE
    case 5: jamBluetooth_Optimized(); break; // Mode Vàng: Chuyên sâu Bluetooth 2 ăng-ten lệch pha
    case 6: jamBLE_Optimized(); break;       // Mode Xanh ngọc: Khóa kép BLE
    case 7: jamAll_Optimized(); break;       // Mode Auto-Cycle: Toàn diện
  }
}

// ========== XỬ LÝ NÚT BẤM (NHẤN NHẢ ĐỔI MODE / NHẤN GIỮ TẮT VỀ 0) ==========
void handleButton() {
  modeButton.loop();

  if (modeButton.isPressed()) {
    buttonPressTime = millis();
    longPressHandled = false;
  }

  // Nếu đang giữ nút quá 800ms -> Đưa thẳng về Mode 0 (Tắt đèn & tạm dừng)
  if (modeButton.getState() == LOW && !longPressHandled) {
    if (millis() - buttonPressTime >= 800) {
      longPressHandled = true;
      currentMode = 0;
      modeChangeTime = millis();
      updateLEDStatus();
      Serial.println("Long Press -> Mode 0 (Standby)");
    }
  }

  // Nếu nhấn nhả nhanh -> Chuyển tuần tự qua các Mode
  if (modeButton.isReleased() && !longPressHandled) {
    currentMode = (currentMode + 1) % TOTAL_MODES;
    modeChangeTime = millis();
    updateLEDStatus();
    Serial.print("Mode changed to: ");
    Serial.println(currentMode);
  }
}

void setup() {
  Serial.begin(115200);

  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();

  setupPWM();
  setRGBColor(255, 0, 0); // Sáng đỏ lúc khởi động

  modeButton.setDebounceTime(50);
  configureRadio();

  delay(600);
  modeChangeTime = millis();
}

void loop() {
  handleButton();

  // Quét tần số liên tục kèm nhịp nghỉ vi mô 35us để giữ mát module PA/LNA
  executeMode();
  delayMicroseconds(35);

  // Cập nhật hiệu ứng đèn cố định mỗi 15ms (~66 FPS) để đèn thở cực mượt
  if (millis() - lastLedUpdate >= 15) {
    updateLEDStatus();
    lastLedUpdate = millis();
  }
}