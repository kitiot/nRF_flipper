# Hướng dẫn nạp Firmware — nRF-KitIot (v0.1.1)

Thư mục này chứa bộ file firmware biên dịch sẵn cho **ESP32 (4MB Flash, chế độ DIO, 80MHz, phân vùng Minimal SPIFFS 1.9MB APP)** cùng công cụ nạp **Espressif Flash Download Tool v3.9.7** (Windows).

> **Lưu ý cấu hình nút bấm:** Bản `.bin` biên dịch sẵn trong `firmware/` được cấu hình theo **Mạch v2** (`OK / SELECT = GPIO 32`, `DOWN = GPIO 33`). Nếu dùng PCB **v3** (`OK = GPIO 33`, `DOWN = GPIO 32`), bạn cần chỉnh lại trong [`../nRFBox-main/nRFBox/config.h`](../nRFBox-main/nRFBox/config.h) và biên dịch lại bằng Arduino IDE.

---

## 1. Danh sách file trong `firmware/`

| File | Địa chỉ nạp (Offset) | Mô tả |
| :--- | :---: | :--- |
| `nRF-KitIot_v0.1.1_merged.bin` | **`0x0`** | **Khuyên dùng** — File gộp đầy đủ (bootloader + partition table + firmware), chỉ cần nạp 1 file duy nhất |
| `nRFBox.ino.bootloader.bin` | `0x1000` | Bootloader ESP32 (DIO, 80MHz, 4MB) |
| `nRFBox.ino.partitions.bin` | `0x8000` | Bảng phân vùng `min_spiffs` (`app0` = 1.9MB, bắt buộc vì firmware ~1.62MB) |
| `nRFBox.ino.bin` | `0x10000` | Firmware chính `nRF-KitIot v0.1.1` (cũng dùng để chép vào thẻ MicroSD dưới tên `firmware.bin`) |

---

## 2. Cách 1: Nạp trên Windows bằng Flash Download Tool v3.9.7

1. Mở [`flash_download_tool_3.9.7/flash_download_tool_3.9.7.exe`](flash_download_tool_3.9.7/flash_download_tool_3.9.7.exe).
2. Ở cửa sổ chọn chip ban đầu:
   - **ChipType:** `ESP32`
   - **WorkMode:** `Develop`
   - **LoadMode:** `UART`
   - Nhấn **OK**.
3. File cấu hình đã được điền sẵn đường dẫn tương đối (`..\firmware\nRF-KitIot_v0.1.1_merged.bin` ở địa chỉ **`0x0`**). Nếu công cụ không tự nhận đường dẫn tương đối, hãy bấm nút `...` ở dòng đầu tiên và chọn:
   - **Cách gọn (1 file — khuyên dùng):** tích chọn 1 dòng duy nhất:
     - `firmware/nRF-KitIot_v0.1.1_merged.bin` → ô địa chỉ điền **`0x0`**
   - **Hoặc nạp 3 file riêng:** tích chọn 3 dòng:
     - `firmware/nRFBox.ino.bootloader.bin` → **`0x1000`**
     - `firmware/nRFBox.ino.partitions.bin` → **`0x8000`**
     - `firmware/nRFBox.ino.bin` → **`0x10000`**
4. Thiết lập thông số phía dưới:
   - **SPI SPEED:** `80MHz` (hoặc `40MHz`)
   - **SPI MODE:** `DIO`
   - Tích chọn **`DoNotChgBin`**
   - **COM:** chọn đúng cổng COM của ESP32 (kiểm tra trong Device Manager; cài driver CP210x hoặc CH340 nếu chưa hiện cổng)
   - **BAUD:** `115200` (hoặc `460800` để nạp nhanh hơn)
5. *(Tùy chọn, nên làm nếu ESP32 trước đó cài firmware khác)* Nhấn **ERASE** để xóa sạch bộ nhớ cũ.
6. Nhấn **START** để bắt đầu nạp. Khi hiển thị **`FINISH`** (màu xanh), rút cáp hoặc nhấn nút **EN / RST** trên ESP32 để khởi động.

---

## 3. Cách 2: Nạp trên macOS / Linux / Windows bằng `esptool`

Cài đặt `esptool` (nếu chưa có):

```bash
pip install esptool
```

Tìm cổng serial trên macOS/Linux:

```bash
ls /dev/cu.* /dev/ttyUSB* 2>/dev/null
```

### Nạp nhanh bằng file gộp (khuyên dùng)

```bash
esptool.py --chip esp32 --port /dev/cu.usbserial-0001 --baud 460800 \
  write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB \
  0x0 firmware/nRF-KitIot_v0.1.1_merged.bin
```

*(Thay `/dev/cu.usbserial-0001` bằng cổng thực tế của bạn, ví dụ `COM3` trên Windows hoặc `/dev/ttyUSB0` trên Linux.)*

### Hoặc nạp 3 file riêng

```bash
esptool.py --chip esp32 --port /dev/cu.usbserial-0001 --baud 460800 \
  write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB \
  0x1000  firmware/nRFBox.ino.bootloader.bin \
  0x8000  firmware/nRFBox.ino.partitions.bin \
  0x10000 firmware/nRFBox.ino.bin
```

---

## 4. Cách 3: Nạp trực tiếp qua trình duyệt (không cần cài phần mềm)

1. Dùng trình duyệt **Chrome** hoặc **Edge**, truy cập [ESP Tool Web (espressif.github.io/esptool-js)](https://espressif.github.io/esptool-js/).
2. Nhấn **Connect**, chọn cổng serial của ESP32 (Baudrate `115200` hoặc `460800`).
3. Chọn **Flash Address:** `0x0`, chọn file `firmware/nRF-KitIot_v0.1.1_merged.bin`.
4. Nhấn **Program**, chờ hoàn tất rồi nhấn **RST** trên mạch ESP32.

---

## 5. Cách 4: Cập nhật trực tiếp bằng thẻ nhớ MicroSD (khi mạch đã có firmware)

1. Copy file `firmware/nRFBox.ino.bin` vào **thư mục gốc** thẻ MicroSD (định dạng FAT32) và đổi tên thành **`firmware.bin`**.
2. Cắm thẻ vào khe MicroSD trên mạch.
3. Trên màn hình OLED, vào **`CAI DAT (Setting)` → `Cap nhat Firmware`**, nhấn **`RIGHT`** để cập nhật.
4. Khi màn hình báo `[ Cap nhat thanh cong! ]`, mạch sẽ tự khởi động lại.

> **Lưu ý:** Chân `SD_CS_PIN` dùng chung **GPIO 5** với `NRF_CE_PIN_A` (nRF24 A). Đảm bảo chỉ cập nhật từ menu cài đặt khi các module nRF24 đang ở trạng thái nghỉ.

---

## 6. Xử lý lỗi thường gặp khi nạp

- **Lỗi `Failed to connect` / `Timed out waiting for packet header`:**
  - Nhấn giữ nút **BOOT** trên mạch ESP32 khi bấm **START** (hoặc khi chạy `esptool`), thả ra khi thấy bắt đầu ghi flash (`Writing at 0x0000...`).
  - Do **GPIO 2** đang nối chân `CSN` của module **nRF24 C** (mà GPIO 2 là chân strapping boot của ESP32), nếu vẫn không vào được Bootloader, hãy **tạm rút dây GPIO 2** (hoặc tháo module nRF24 C) trong lúc nạp.
- **Nạp xong bị bootloop / khởi động lại liên tục:**
  - Nguyên nhân thường do trước đó mạch dùng bảng phân vùng `default` (chỉ có 1.3MB cho app) và chỉ nạp mỗi file `nRFBox.ino.bin` ở `0x10000` mà quên nạp `nRFBox.ino.partitions.bin` ở `0x8000`.
  - **Cách sửa:** Bấm **ERASE** rồi nạp lại bằng file gộp `nRF-KitIot_v0.1.1_merged.bin` tại địa chỉ **`0x0`**.
- **Không thấy cổng COM:**
  - Kiểm tra lại cáp USB (phải là cáp truyền dữ liệu, không phải cáp chỉ sạc) và cài driver **CP210x** hoặc **CH340** tùy chip nạp trên board ESP32.
