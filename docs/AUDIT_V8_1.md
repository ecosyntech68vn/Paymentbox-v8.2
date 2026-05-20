# AUDIT V8.1 — Lỗi tiềm ẩn và điểm cần sửa

**Ngày audit**: 20/05/2026
**Đối tượng**: PCB V8.1-rev1.0 đã build
**Mục đích**: Tìm hết bug, lỗ hổng kỹ thuật trước khi order JLCPCB

---

## A. LỖI LOGIC NETLIST (PHẢI SỬA TRƯỚC KHI FAB)

### A1. 🔴 CRITICAL — LED polarity ngược

**Phát hiện**: Trong `nets_paymentbox_v8_1.py`, LED 0805 pad numbering:
- KiCad LED_0805_2012Metric: **Pin 1 = Cathode** (mark side), **Pin 2 = Anode**
- Code hiện tại nối: `LED1.2 → GND` (sai — pin 2 là anode), `LED1.1 ← R1` (sai — pin 1 là cathode)
- **Kết quả nếu fab**: LED sẽ **không sáng** (ngược chiều), GPIO drive sẽ phải sink current từ 3V3 — không hoạt động.

**Fix**: Đảo lại pad number:
```python
"GND": [..., ("LED1", "1"), ("LED2", "1"), ("LED3", "1"), ...]   # pin 1 = cathode
"LED_R_ANODE": [("R1", "2"), ("LED1", "2")]                       # pin 2 = anode
```

---

### A2. 🔴 CRITICAL — Strapping pin GPIO12 không có pull-down

**Vấn đề**: GPIO12 ESP32 là strapping pin. Lúc reset:
- LOW = flash voltage 3.3V (đúng)
- HIGH = flash voltage 1.8V → **brick** module

Hiện tại GPIO12 nối: GPIO12 → R1 220Ω → LED1 anode → LED1 cathode → GND.
- LED forward voltage Vf ≈ 2V
- Khi GPIO12 floating (boot moment), điện áp tại GPIO12 = 2V → có thể bị đọc HIGH → boot fail.

**Fix**: Thêm `R21 = 10kΩ` pull-down trực tiếp từ GPIO12 xuống GND. Khi boot, R21 kéo GPIO12 về 0V (R220 + LED không cản được vì impedance cao). Khi ESP32 chạy, GPIO12 drive HIGH active đè qua R21.

---

### A3. 🔴 CRITICAL — Strapping pin GPIO15 không có pull-down

**Vấn đề**: GPIO15 dùng cho `RELAY_DRV` (phương án F).
- ESP32 yêu cầu GPIO15 LOW hoặc HIGH ổn định lúc boot
- Nếu floating → boot debug message in ra UART → spam log
- **Nghiêm trọng hơn**: nếu nhiễu kéo GPIO15 lên HIGH ngắn lúc khởi động → opto bật → Q1 bật → **RELAY KÍCH FALSE** → nguy hiểm vì có thể cắt nguồn phone bất ngờ.

**Fix**: Thêm `R22 = 10kΩ` pull-down từ GPIO15 xuống GND. Đảm bảo RELAY tắt mặc định khi power-on.

---

### A4. 🟠 MAJOR — Flyback diode D4 (1N4148) quá yếu

**Vấn đề**: 
- Relay coil SRD-05VDC inductance ≈ 50mH, dòng coil = 71mA
- Khi tắt, inductive kick có thể đạt **> 1A** trong vài µs
- 1N4148 max IFSM = 200mA peak → có thể chết sau vài chục lần switching

**Fix**: Thay bằng **Schottky SS14 (1A, 40V)** hoặc **1N4007 (1A)** — chấp nhận drop nhỏ hơn, đáng tin cậy hơn.

---

### A5. 🟠 MAJOR — Tụ bulk cho microSD card chưa đủ

**Vấn đề**:
- SD card consume 100mA peak khi write/erase block
- 3V3 rail có C5 (10µF) sát ESP32, KHÔNG có tụ bulk gần U5 (microSD socket)
- → SD write có thể gây sụt áp 3V3 → ESP32 brown-out → reset

**Fix**: Thêm `C_SD_BULK = 10µF tantalum 0805` sát chân VDD của microSD (U5 pin 4).

---

### A6. 🟡 MINOR — ATECC608B I2C address conflict

**Lưu ý**: ATECC608B mặc định I2C address = `0x60`. Một số PCF8574 (LCD backpack) có thể được nhà sản xuất config `0x60` (hiếm, thường `0x27` hoặc `0x3F`).

**Mitigation**: Khi mua module LCD, **verify I2C address với multimeter/scanner trước khi cắm**. Nếu trùng → đổi LCD module.

---

## B. THIẾU PROTECTION (PHẢI THÊM CHO V8.2)

### B1. 🔴 CRITICAL — Không có chống ngược cực

**Yêu cầu THUAN**: "đỡ việc cắm nhầm cực" cho pin 18650.

**Hiện trạng V8.1**: Chỉ có USB-C input (USB-C tự reverse-protected qua spec). KHÔNG có battery input. Khi thêm 18650 cắm sai cực → **toàn bộ board chết tức thì**.

**Fix V8.2**:
1. Dùng **pin 18650 protected** (có sẵn DW01A + 8205A trên cell)
2. Holder kim loại có hình dạng asymmetric — chỉ cắm được 1 chiều
3. **P-MOSFET AO3401** ở đường vào battery → khi cắm ngược, MOSFET không turn on → chặn dòng ngược

---

### B2. 🔴 CRITICAL — Không có protection chống sét/surge

**Hiện trạng**: D5 = PESD5V0S1UB chỉ chịu ESD ~30V, KHÔNG đủ cho surge từ lưới điện qua adapter USB-C.

Hiện nay điện lưới VN có:
- Sét cảm ứng → adapter USB-C → laptop/phone → board: peak ~500-2000V transient
- Switching surge khi máy nén/máy hàn bật/tắt: 200-800V

**Fix V8.2**:
1. Thay D5 bằng **SMBJ5.0CA** (bidirectional TVS, 600W peak power, 6.8V breakdown) — chịu được surge thực
2. Thêm **PPTC fuse F1 500mA reset-able** trên VBUS input
3. Thêm **ferrite bead nâng cấp** 1kΩ@100MHz (thay vì 600R hiện tại)
4. Adapter USB-C: khuyến nghị adapter chính hãng có **EMI filter + ESD protection nội bộ** (đắt nhưng đáng)

---

### B3. 🟠 MAJOR — Chống ẩm chưa được tính tới

**Vấn đề**: Việt Nam độ ẩm > 80% mùa mưa. Bo mạch không coating:
- Solder mask không chống ẩm hoàn toàn
- Connector USB-C có khe hở → ẩm vào trong → ăn mòn pad
- ESP32 module có khe antenna → ẩm có thể tạo tụ ký sinh → mất RF tuning

**Fix V8.2** (không đổi PCB, đổi SOP production):
1. **Conformal coating MG Chemicals 419D** (acrylic) hoặc **Dow Corning 1-2577** (silicone) — spray toàn bộ PCB sau assembly, để khô 24h
2. **Hot-melt glue** vào USB-C connector mép đáy (chặn đường ẩm vào)
3. **Hộp ABS IP65** với gasket cao su silicone (đặt sản xuất hoặc 3D print + EPDM gasket)
4. **Silica gel** 2 gói trong hộp (hấp thụ ẩm 12 tháng)
5. **Breather vent** Gore-tex membrane (cân bằng áp suất nhưng chặn nước)

---

## C. LỖI PCB LAYOUT

### C1. 🟠 MAJOR — ESP32 antenna có thể bị ảnh hưởng

**Phát hiện qua DFM**: `[edge_clearance] U1: extends within 0.5mm of board edge`

**Vấn đề**:
- ESP32 module rotation 0° tại (35, 24)
- Phần antenna của module (top edge) extends lên phía y=11 — sát zone keepout y=0-12
- Có thể trace gần antenna gây nhiễu RF

**Fix V8.2**:
- Đổi vị trí ESP32 xuống y=27 (dịch xuống 3mm)
- Hoặc xoay 180° để antenna ở phía trong board, edge keepout chuyển sang phía dưới — không recommend vì design xấu

---

### C2. 🟡 MINOR — J3 (expansion header), K1 (relay), J4 (terminal) gần edge

DFM cảnh báo edge_clearance < 0.5mm. JLCPCB chấp nhận 0.3mm. **Acceptable** nhưng nên dời các component vào sâu hơn 0.5mm.

---

## D. SUMMARY — 11 LỖI CẦN FIX TRONG V8.2

| # | Mức độ | Lỗi | Fix V8.2 |
|---|---|---|---|
| A1 | 🔴 CRITICAL | LED polarity ngược | Đảo pad number trong nets.py |
| A2 | 🔴 CRITICAL | GPIO12 strapping floating | Thêm R21 10kΩ pull-down |
| A3 | 🔴 CRITICAL | GPIO15 strapping floating | Thêm R22 10kΩ pull-down (chống relay false-trigger) |
| A4 | 🟠 MAJOR | Flyback diode 1N4148 yếu | Thay bằng SS14 Schottky |
| A5 | 🟠 MAJOR | Tụ bulk SD card thiếu | Thêm C13 10µF gần U5 |
| A6 | 🟡 MINOR | I2C address conflict tiềm năng | Verify LCD trước khi cắm |
| B1 | 🔴 CRITICAL | Không có chống ngược cực | Thêm Q3 AO3401 P-FET, JST connector |
| B2 | 🔴 CRITICAL | Không có chống sét/surge | Thay D5 → SMBJ5.0CA, thêm F1 PPTC |
| B3 | 🟠 MAJOR | Chống ẩm | SOP coating + hộp IP65 |
| C1 | 🟠 MAJOR | ESP32 sát antenna keepout | Dời xuống y=27 |
| C2 | 🟡 MINOR | Edge clearance | Dời components vào trong |

**Cộng các yêu cầu mới của THUAN**:

| # | Tính năng | Components V8.2 |
|---|---|---|
| E1 | Pin 18650 + sạc | TP4056 (U6), MT3608 boost (U7), JST connector (J5), 18650 protected cell external |
| E2 | Indicator sạc | LED4 (CHRG red), LED5 (DONE green) |
| E3 | ADC monitor pin | R19/R20 voltage divider lên GPIO36 (ADC) |

V8.2 sẽ là design **production-ready**, không phải prototype.
