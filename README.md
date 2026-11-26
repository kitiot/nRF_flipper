# nRF-KitIot (v0.1.1)

Bộ công cụ kiểm thử sóng không dây 2.4GHz / Wi-Fi / BLE sử dụng **ESP32**, **3 module nRF24L01+**, **màn hình OLED 0.96" SSD1306 (I2C)**, **5 nút nhấn điều hướng** và **LED RGB WS2812B** — Việt hóa và tùy biến bởi **kitiot.com** dựa trên mã nguồn [cifertech/nrfbox](https://github.com/cifertech/nrfbox) (v2.7.2, giấy phép MIT).

> **Lưu ý pháp lý:** Dự án chỉ phục vụ mục đích học tập, nghiên cứu và kiểm thử bảo mật trên thiết bị/mạng thuộc quyền sở hữu của bạn.

---

## 1. Sơ đồ cắm chân (Pinout) toàn bộ dự án

Toàn bộ cấu hình chân nằm trong file [`nRFBox-main/nRFBox/config.h`](nRFBox-main/nRFBox/config.h).

### 1.1. Nút bấm điều hướng (5 nút)

Tất cả các nút đều dùng điện trở kéo lên nội (`INPUT_PULLUP`): **1 chân nút nối vào GPIO ESP32, chân còn lại nối GND**.

| Nút bấm | Macro trong `config.h` | GPIO (Mạch v2 - Hiện tại) | GPIO (Mạch v3 gốc) | Ghi chú |
| :--- | :--- | :---: | :---: | :--- |
| **UP (Lên)** | `BUTTON_UP_PIN` | **GPIO 26** | GPIO 26 | Di chuyển lên / bật-tắt hoặc đổi chế độ ở một số mục |
| **DOWN (Xuống)** | `BUTTON_DOWN_PIN` | **GPIO 33** | GPIO 32 | **Đảo với SELECT giữa v2 và v3** |
| **LEFT (Trái)** | `BTN_PIN_LEFT` | **GPIO 25** | GPIO 25 | Di chuyển trái / quay lại / đổi kiểu hoặc giảm giá trị |
| **RIGHT (Phải)** | `BTN_PIN_RIGHT` | **GPIO 27** | GPIO 27 | Di chuyển phải / xem chi tiết / xác nhận / tăng giá trị |
| **OK / SELECT** | `BUTTON_SELECT_PIN` | **GPIO 32** | GPIO 33 | Mở chức năng từ menu chính & thoát về menu chính |

> **Lưu ý:** File `config.h` và bản firmware biên dịch sẵn trong [`Flasher/firmware/`](Flasher/firmware/) đã cấu hình theo **Mạch v2** (`OK = GPIO 32`, `DOWN = GPIO 33`, thiết kế v2 xem trong [`nRFBox-main/PCB/Previous Designs/v2 Design.rar`](nRFBox-main/PCB/Previous%20Designs/v2%20Design.rar)). Nếu bạn dùng PCB **v3** ([`nRFBox-main/Schematic/nRFBOX-Schematic.jpg`](nRFBox-main/Schematic/nRFBOX-Schematic.jpg)), hãy đổi lại `BUTTON_SELECT_PIN = 33` và `BUTTON_DOWN_PIN = 32` trong `config.h` rồi biên dịch lại.

---

### 1.2. Màn hình OLED 0.96" SSD1306 (128×64, I2C)

| Chân OLED | Chân ESP32 | Ghi chú |
| :--- | :---: | :--- |
| **VCC** | **3.3V** | Không cấp 5V nếu màn chỉ hỗ trợ 3.3V |
| **GND** | **GND** | Mass chung |
| **SDA** | **GPIO 21** | I2C Data mặc định của ESP32 |
| **SCL** | **GPIO 22** | I2C Clock mặc định của ESP32 |

- Khởi tạo trong `config.h`: `U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);`
- Nếu màn hình lắp ngược 180°, đổi `U8G2_R0` thành `U8G2_R2`.

---

### 1.3. 3 Module nRF24L01+ (Bus SPI dùng chung - VSPI)

Cả 3 module nRF24L01+ dùng chung đường **VCC (3.3V)**, **GND**, **SCK**, **MISO**, **MOSI**, chỉ khác nhau cặp chân **CE** và **CSN** (chân **IRQ** bỏ trống):

#### Đường SPI dùng chung (nối chung cho cả 3 module nRF24L01+)

| Chân nRF24L01+ | Chân ESP32 |
| :--- | :---: |
| **SCK** | **GPIO 18** |
| **MISO** | **GPIO 19** |
| **MOSI** | **GPIO 23** |
| **VCC** | **3.3V** |
| **GND** | **GND** |
| **IRQ** | *(Không nối)* |

#### Chân điều khiển riêng cho từng module nRF24L01+

| Module | Vai trò | CE | CSN | Macro trong `config.h` |
| :--- | :--- | :---: | :---: | :--- |
| **nRF24 A** (Radio 1) | Scanner, Analyzer, Jammer, ProtoKill, BLE Jammer | **GPIO 5** | **GPIO 17** | `NRF_CE_PIN_A` / `NRF_CSN_PIN_A` |
| **nRF24 B** (Radio 2) | Jammer, ProtoKill, BLE Jammer | **GPIO 16** | **GPIO 4** | `NRF_CE_PIN_B` / `NRF_CSN_PIN_B` |
| **nRF24 C** (Radio 3) | Jammer, ProtoKill, BLE Jammer | **GPIO 15** | **GPIO 2** | `NRF_CE_PIN_C` / `NRF_CSN_PIN_C` |

---

### 1.4. LED RGB NeoPixel WS2812B (Tùy chọn)

| Chân WS2812B | Chân ESP32 / Nguồn | Ghi chú |
| :--- | :---: | :--- |
| **DIN** | **GPIO 14** | Khai báo tại `config.h` dòng 47 (`pixels(1, 14, ...)`) |
| **VCC** | **5V** (hoặc 3.3V) | Trên sơ đồ nối 5V, nên qua trở hạn dòng ~330Ω–1kΩ ở chân DIN |
| **GND** | **GND** | Mass chung |

- Có thể bật/tắt LED trong mục **CAI DAT (Setting) → NeoPixel** (lưu EEPROM).

---

### 1.5. Khe thẻ nhớ MicroSD (Tùy chọn - dùng cho Cập nhật Firmware)

Dùng khi cập nhật firmware qua thẻ nhớ (file `/firmware.bin` đặt ở thư mục gốc thẻ SD định dạng FAT32):

| Chân MicroSD | Chân ESP32 | Ghi chú |
| :--- | :---: | :--- |
| **CS** | **GPIO 5** | `SD_CS_PIN` *(trùng chân với `NRF_CE_PIN_A`)* |
| **SCK** | **GPIO 18** | Dùng chung SPI |
| **MISO** | **GPIO 19** | Dùng chung SPI |
| **MOSI** | **GPIO 23** | Dùng chung SPI |
| **VCC** | **3.3V** | |
| **GND** | **GND** | |

> **Cảnh báo:** `SD_CS_PIN` đang khai báo cùng **GPIO 5** với `NRF_CE_PIN_A`. Khi cập nhật qua thẻ SD, hãy đảm bảo module nRF24 A đang ở trạng thái nghỉ (neutral — mặc định khi ở Menu/Cài đặt), hoặc đổi `SD_CS_PIN` sang chân trống khác nếu tự đấu mạch rời.

---

### 1.6. Bảng tổng hợp nhanh các chân ESP32 đã dùng

| GPIO | Chức năng | GPIO | Chức năng |
| :---: | :--- | :---: | :--- |
| **GPIO 2** | nRF24 C - CSN | **GPIO 21** | OLED - SDA (I2C) |
| **GPIO 4** | nRF24 B - CSN | **GPIO 22** | OLED - SCL (I2C) |
| **GPIO 5** | nRF24 A - CE *(và SD CS)* | **GPIO 23** | SPI - MOSI (chung) |
| **GPIO 14** | LED WS2812B - DIN | **GPIO 25** | Nút LEFT (Trái) |
| **GPIO 15** | nRF24 C - CE | **GPIO 26** | Nút UP (Lên) |
| **GPIO 16** | nRF24 B - CE | **GPIO 27** | Nút RIGHT (Phải) |
| **GPIO 17** | nRF24 A - CSN | **GPIO 32** | Nút OK / SELECT (v2) |
| **GPIO 18** | SPI - SCK (chung) | **GPIO 33** | Nút DOWN (Xuống) (v2) |
| **GPIO 19** | SPI - MISO (chung) | | |

---

## 2. Lưu ý quan trọng về phần cứng & nguồn điện

1. **Nguồn 3.3V cho nRF24L01+:**
   - **Tuyệt đối không cấp 5V** vào chân VCC của nRF24L01+ (sẽ cháy module).
   - Khi chạy đồng thời 3 module nRF24L01+ (đặc biệt bản có PA+LNA), dòng đỉnh rất cao dễ làm ESP32 bị sụt áp/reset. **Bắt buộc hàn tụ lọc 10µF – 100µF** (song song với tụ gốm 100nF càng tốt) sát chân VCC–GND của từng module và dùng mạch ổn áp 3.3V riêng (như LF33 / AMS1117 / mạch buck 3.3V dòng >= 800mA).
2. **Chân Strapping khi nạp code (GPIO 2 & GPIO 15):**
   - `GPIO 2` nối vào `CSN` của nRF24 C. Nếu lúc nạp code ESP32 báo lỗi `Failed to connect` / không vào được chế độ Bootloader, hãy nhấn giữ nút **BOOT** khi nạp hoặc tạm rút dây `GPIO 2`.

---

## 3. Danh sách chức năng & Cách điều khiển

### Giao diện Menu chính (2 chế độ: `LUOI` & `LIST`)

Bạn có thể chuyển đổi kiểu hiển thị trong **CAI DAT (Setting) → Kieu Menu** (tự động lưu EEPROM):
- **Kiểu `LUOI` (Grid 3×2 — mặc định):** Hiển thị 12 biểu tượng theo lưới 3 cột × 2 hàng. Thanh trạng thái trên cùng hiển thị tên đầy đủ của mục đang chọn (tự chạy chữ marquee nếu dài; sau 10 giây không bấm nút sẽ quay lại hiển thị `nRF-KitIot` và `v0.1.1`).
- **Kiểu `LIST` (Danh sách):** Hiển thị 3 mục/trang kèm biểu tượng 16×16 và tên đầy đủ (tự chạy chữ ở mục đang chọn), điều hướng xoay vòng bằng `UP/DOWN` hoặc `LEFT/RIGHT`.
- **OK (SELECT):** Mở chức năng đang chọn. Khi đang ở trong bất kỳ chức năng nào, nhấn **OK (SELECT)** để thoát về menu chính.

### Bảng 12 chức năng

| # | Tên trên màn hình | Chức năng | Thao tác nút bên trong |
| :-: | :--- | :--- | :--- |
| 1 | **QUET SONG 2.4G (Scanner)** | Quét mức tín hiệu 64 điểm trên băng 2.4GHz, vẽ biểu đồ lịch sử (lưu EEPROM mỗi 5s) | `OK`: Thoát |
| 2 | **PHAN TICH PHO (Analyzer)** | Phân tích phổ 128 kênh 2.4GHz dạng biểu đồ cột thời gian thực | `OK`: Thoát |
| 3 | **PHA SONG WLAN (WLAN Jammer)** | Phát nhiễu kênh Wi-Fi (1–14) tùy chỉnh `Channel`, `PA Level` (MIN/LOW/HIGH/MAX), `Data Rate` (250K/1M/2M) | `UP/DOWN`: Chọn dòng; `LEFT/RIGHT`: Đổi thông số / bật-tắt Jamming; `OK`: Thoát |
| 4 | **PHA GIAO THUC (Proto Kill)** | Phá sóng 8 giao thức 2.4GHz bằng 3 module nRF24 lệch pha *(chi tiết bên dưới)* | `LEFT/RIGHT`: Đổi giao thức; `UP`: Bật/tắt (`-BAT-`/`-TAT-`); `OK`: Thoát |
| 5 | **PHA SONG BLE (BLE Jammer)** | Phá sóng BLE / Bluetooth / BT+BLE với 3 kiểu lịch phát (`PRO`, `STACK`, `BASIC`) và hiển thị trạng thái 3 Radio | Nhấn nhanh `UP`: Đổi Mode (`Deactive` → `BLE` → `Bluetooth` → `BT+BLE`); Giữ `UP` (>=0.8s): Tắt nhanh; `LEFT/RIGHT`: Đổi kiểu (`PRO`/`STACK`/`BASIC`); `OK`: Thoát |
| 6 | **GIA LAP BLE (BLE Spoofer)** | Giả lập gói quảng bá BLE cho 21 thiết bị (17 Apple, 3 Samsung Galaxy Watch, 1 Google Smart Ctrl) với 5 `Adv Type` | `UP/DOWN`: Chọn dòng (`Device`/`Adv Type`/`Advertising`); `LEFT/RIGHT`: Đổi giá trị / bật-tắt; `OK`: Thoát |
| 7 | **SPAM APPLE (Sour Apple)** | Gửi liên tục gói quảng bá BLE Apple (MAC ngẫu nhiên) | `OK`: Thoát |
| 8 | **QUET BLE (BLE Scan)** | Quét thiết bị BLE xung quanh (5s), hỗ trợ chạy chữ tên dài | `UP/DOWN`: Cuộn danh sách; `RIGHT`: Xem chi tiết (Name, Addr, RSSI); `LEFT`: Quay lại; `OK`: Thoát |
| 9 | **QUET WIFI (WiFi Scan)** | Quét mạng Wi-Fi 2.4GHz xung quanh, hỗ trợ chạy chữ SSID dài | `UP/DOWN`: Cuộn danh sách; `RIGHT`: Xem chi tiết (SSID, BSSID, RSSI, Channel); `LEFT`: Quay lại; `OK`: Thoát |
| 10 | **NGAT KET NOI WIFI (Deauther)** | Quét mạng Wi-Fi (sắp xếp theo RSSI) và gửi gói Deauth 802.11 tới AP đã chọn (tự kiểm tra đổi kênh mỗi 15s) | Danh sách: `UP/DOWN` chọn mạng, `RIGHT` mở chi tiết. Chi tiết: `RIGHT` bắt đầu/dừng tấn công, `LEFT` dừng & quay lại danh sách; `OK`: Thoát |
| 11 | **THONG TIN (About)** | Thông tin tác giả (`kitiot@gmail.com`, `GitHub/kitiot`, `tiktok/kitiot`) | `OK`: Thoát |
| 12 | **CAI DAT (Setting)** | 4 tùy chọn: `NeoPixel` (BAT/TAT), `Do sang` (0–100%), `Kieu Menu` (LUOI/LIST), `Cap nhat Firmware` (SD) | `UP/DOWN`: Chọn mục; `RIGHT`: Thay đổi / Xác nhận; `OK`: Thoát |

### Chi tiết chế độ nâng cao

#### Các kiểu phát trong `PHA SONG BLE (BLE Jammer)`
- **`PRO` (Split-Band Staggered):** 3 radio phát trên 3 kênh khác nhau lệch pha, xen kẽ cách nhau 55µs để không dẫm kênh nhau. Ở chế độ `BT+BLE`, Radio 1 + 2 quét 40 kênh Bluetooth lệch nửa băng tần, Radio 3 chuyên quét 3 kênh quảng cáo BLE (`2, 26, 80`).
- **`STACK` (Sequential Stack):** 3 radio nối đuôi nhau bắn cùng một kênh cách nhau 55µs (Radio 1 → 2 → 3) để tăng mật độ năng lượng trên kênh mục tiêu.
- **`BASIC` (Original):** Cả 3 radio cùng nhảy ngẫu nhiên trên một kênh (21 kênh Bluetooth gốc / 3 kênh BLE).

#### 8 giao thức trong `PHA GIAO THUC (Proto Kill)`
`WiFi` (13 kênh 2412–2472) · `Video TX` (3 kênh 2470–2480) · `RC` (4 kênh 2401–2407) · `BLE` (3 kênh 2402/26/80) · `Bluetooth` (40 kênh 2402–2480) · `USB Wireless` (3 kênh 2440–2460) · `Zigbee` (16 kênh 2405–2480) · `NRF24` (3 kênh 2476–2479).

---

## 4. Hướng dẫn nạp Firmware & Biên dịch

### Cách 1: Nạp nhanh bằng Flasher (không cần cài Arduino IDE)

Toàn bộ công cụ nạp và bộ file `.bin` biên dịch sẵn cho **nRF-KitIot v0.1.1** nằm trong thư mục [`Flasher/`](Flasher/). Xem hướng dẫn chi tiết từng bước cho **Windows, macOS/Linux (`esptool`), trình duyệt Web và thẻ MicroSD** tại:

👉 **[`Flasher/README.md`](Flasher/README.md)**

Tóm tắt nhanh địa chỉ nạp (Flash Mode: `DIO`, Flash Freq: `80MHz`, Flash Size: `4MB`):

| Phương án | File trong `Flasher/firmware/` | Địa chỉ (Offset) |
| :--- | :--- | :---: |
| **Gộp 1 file (khuyên dùng)** | `nRF-KitIot_v0.1.1_merged.bin` | `0x0` |
| **Hoặc 3 file riêng** | `nRFBox.ino.bootloader.bin` | `0x1000` |
| | `nRFBox.ino.partitions.bin` | `0x8000` |
| | `nRFBox.ino.bin` | `0x10000` |

---

### Cách 2: Biên dịch & nạp từ mã nguồn (Arduino IDE)

#### Thư viện & Board cần cài đặt
- **U8g2** (by oliver)
- **RF24** (by TMRh20)
- **Adafruit NeoPixel** (by Adafruit)
- Board package: **esp32** by Espressif Systems (chọn board `Node32s` hoặc `ESP32 Dev Module`).

#### Cấu hình bắt buộc trước khi Upload
1. Mở file [`nRFBox-main/nRFBox/nRFBox.ino`](nRFBox-main/nRFBox/nRFBox.ino) bằng Arduino IDE.
2. Kiểm tra lại cấu hình chân nút bấm trong [`nRFBox-main/nRFBox/config.h`](nRFBox-main/nRFBox/config.h) cho khớp với mạch thực tế của bạn (v2 hay v3).
3. Trong menu **Tools → Partition Scheme**, bắt buộc chọn **`Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)`** (vì firmware ~1.62 MB lớn hơn giới hạn 1.3 MB của phân vùng mặc định).
4. Chọn đúng cổng **Port (COM / `/dev/cu.*`)** và nhấn **Upload**.

---

## 5. Cấu trúc thư mục

```text
FLIPPER ESP32/
├── README.md                            # Tài liệu hướng dẫn & sơ đồ chân (file này)
├── Flasher/                             # Bộ công cụ nạp & firmware biên dịch sẵn (v0.1.1)
│   ├── README.md                        # Hướng dẫn nạp chi tiết (Windows, macOS/Linux, Web, SD)
│   ├── firmware/                        # Các file .bin của nRF-KitIot v0.1.1
│   │   ├── nRF-KitIot_v0.1.1_merged.bin # File gộp nạp 1 bước tại 0x0
│   │   ├── nRFBox.ino.bootloader.bin    # Bootloader (0x1000)
│   │   ├── nRFBox.ino.partitions.bin    # Partition table min_spiffs (0x8000)
│   │   └── nRFBox.ino.bin               # Firmware chính (0x10000 / firmware.bin cho thẻ SD)
│   ├── flash_download_tool_3.9.7/       # Espressif Flash Download Tool v3.9.7 (Windows)
│   └── x_bluetooth.ino                  # Sketch tham khảo phá sóng Bluetooth 2 module nRF24
└── nRFBox-main/
    ├── nRFBox/                          # Mã nguồn chính (nRF-KitIot v0.1.1 / gốc v2.7.2)
    │   ├── nRFBox.ino                   # Menu chính (LUOI/LIST), thanh tiêu đề & vòng lặp chính
    │   ├── config.h                     # Khai báo toàn bộ chân GPIO, OLED, thư viện
    │   ├── bluetooth.cpp                # BLE Jammer (PRO/STACK/BASIC), BLE Scan, Spoofer, SourApple
    │   ├── ism.cpp                      # Scanner, Analyzer, Jammer, ProtoKill (8 giao thức)
    │   ├── wifi.cpp                     # WiFi Scan, Deauther
    │   ├── setting.cpp / .h             # Cài đặt, EEPROM, marquee text, quản lý 3 module nRF24
    │   ├── neopixel.cpp                 # Điều khiển LED WS2812B
    │   ├── icon.h                       # Icon bitmap OLED, logo & thông tin phiên bản
    │   └── build/                       # Kết quả biên dịch (esp32.esp32.node32s)
    ├── PCB/                             # File PCB v3, BOM, CPL, Gerber & bản thiết kế v2 cũ
    ├── Schematic/                       # Sơ đồ nguyên lý bản v3 (nRFBOX-Schematic.jpg)
    ├── Flash File/                      # File nạp gốc từ cifertech/nrfbox
    └── Pre-compiled Bin/                # Các bản .bin gốc theo phiên bản (v2.0.0 – v2.7.2)
```
