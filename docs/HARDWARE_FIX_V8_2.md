# HARDWARE FIX INSTRUCTIONS — PaymentBox V8.2

Các fix này yêu cầu sửa trong KiCad PCB layout, không thể fix bằng code.

---

## 1. C4 — Annular ring USB-C shield pad J1.S1

**Issue**: `J1.S1 annular -0.150mm < min 0.075mm` — Pad nhỏ hơn drill.

**Fix trong KiCad**:
1. Mở `paymentbox_v8_2.kicad_pcb`
2. Double-click pad S1 của J1 (USB-C connector, shield pad)
3. Tab "Pad Properties": tăng đường kính pad lên +0.3mm (từ hiện tại lên ít nhất 0.2mm > drill)
4. Hoặc: giảm drill size xuống 0.1mm nếu pad không thể tăng
5. Re-run DFM để verify

---

## 2. M4 — Power trace width +5V_USB_RAW < 0.4mm

**Issue**: Trace dòng +5V_USB_RAW chỉ 0.25mm < min 0.4mm cho power net.

**Fix trong KiCad**:
1. Chọn net `+5V_USB_RAW`
2. Update routing rule: set min width = 0.5mm (cho 1A+)
3. Re-route: highlight net → thay trace 0.25mm → 0.5mm
4. Nếu trace quá dày cho không gian hẹp, dùng copper pour / polygon pour thay vì trace
5. Nên dùng polygon pour cho toàn bộ power path: USB-C → F1 → Q3 → TP4056

---

## 3. M7 — Decoupling caps C1 (11.4mm) và C2 (15.1mm) xa ESP32

**Issue**: C1 và C2 cách ESP32 power pads > 5mm (khuyến nghị max 5mm).

**Fix trong KiCad**:
1. Di chuyển C1, C2 đến sát ESP32 pins 2 (3V3) và 1 (GND)
2. Khoảng cách tối đa: 5mm từ pad tụ đến pad ESP32
3. Nếu không đủ chỗ, thêm C3 (đã có 100nF) và xóa C1/C2 cũ
4. Caps gợi ý vị trí mới: x=32, y=30 (sát ESP32 U1)

---

## 4. M8 — Edge clearance violations (6 components)

**Issue**: U5, K1, J4, U1, R11, J3 < 0.5mm từ board edge.

**Fix trong KiCad**:
| Ref | Dịch chuyển |
|-----|-------------|
| U5 (microSD) | Dịch lên +2mm (vào trong board) |
| K1 (relay) | Dịch lên +1mm |
| J4 (terminal) | Dịch lên +1mm |
| U1 (ESP32) | Đã fix từ V8.1 → y=28, chấp nhận nếu keepout zone cố định |
| R11 | Dịch vào +1mm (gần J1 hơn) |
| J3 (expansion) | Dịch xuống +1mm |

---

## 5. Boost C13 feedforward (liên quan M3)

**Issue**: nets_v8_2.py đã fix tên thành C13 + value 10nF.

**Fix trong KiCad PCB**:
1. Rename `C13_boost` → `C13` trong PCB (hoặc đánh dấu BOM mới)
2. Đảm bảo footprint là C_0805
3. BOM note: 10nF (0.01µF) ceramic, không dùng 100nF

---

## 6. Netlist consistency check

Sau khi sửa PCB, chạy:
```bash
python3 hardware/nets_v8_2.py
```
Verify output khớp với ERC (Electrical Rules Check) trong KiCad.
