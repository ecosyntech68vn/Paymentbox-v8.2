# GAS V9.5 Backend — PaymentBox V8.2 Endpoints

Hai endpoint bổ sung cho GAS V9.5 hỗ trợ firmware PaymentBox V8.2:
1. `?action=getHeartbeat` — `gas_poller` xác nhận phone đang push GAS
2. `?action=ota_check` — `ota_updater` kiểm tra firmware mới

---

## Cấu trúc

```
backend/gas_v9_5/
├── README.md                            ← Bạn đang đọc
├── TEST_REPORT.md                       ← 42/42 unit tests pass
├── gas_v9_5_paymentbox_endpoints.js    ← Code copy vào GAS Editor
└── test_gas_endpoints.js               ← Local tests (node.js)
```

---

## Tích hợp vào GAS V9.5

### Bước 1 — Thêm code

1. Mở Google Apps Script Editor của project GAS V9.5 hiện tại
2. Tạo file mới: **File → New → Script file** → tên `PaymentBoxEndpoints`
3. Copy toàn bộ nội dung `gas_v9_5_paymentbox_endpoints.js` (trừ block `module.exports` ở cuối — GAS không cần)
4. Paste vào file vừa tạo

### Bước 2 — Sửa router `doGet()`

Thêm 2 case vào hàm `doGet()` hiện tại (gần đầu hàm):

```javascript
function doGet(e) {
  // ... các action cũ ...

  // V8.2 PaymentBox endpoints
  if (e.parameter.action === 'getHeartbeat') return handleGetHeartbeat_(e);
  if (e.parameter.action === 'ota_check')    return handleOtaCheck_(e);

  // ... fallback / default ...
}
```

### Bước 3 — Tạo 2 sheet

Trong spreadsheet GAS V9.5:

**Sheet `BankHeartbeat`** (nếu chưa có — phone push vào đây mỗi lần ping):
| A: device_id | B: last_seen |
|---|---|
| ESG-PB-A4B7C9D2 | 2026-05-20T08:00:00Z |

**Sheet `Firmware`** (firmware registry):
| A: device_pattern | B: hw | C: new_version | D: url | E: sha256 | F: min_battery_v | G: release_notes |
|---|---|---|---|---|---|---|
| ESG-PB-* | V8.2 | 1.1.0 | https://drive.google.com/uc?export=download&id=XXX | abc123... | 3.5 | Fix LED flicker |

Hỗ trợ wildcard pattern:
- `ESG-PB-*` → tất cả PaymentBox
- `ESG-PB-A4B7C9D2` → chỉ device cụ thể (specific overrides — row trên cùng wins)

### Bước 4 — Deploy

1. **Deploy → New deployment** (hoặc Manage deployments → Edit existing)
2. Type: **Web app**
3. Execute as: **Me**
4. Who has access: **Anyone**
5. Click **Deploy** → copy URL `https://script.google.com/macros/s/SHEET_ID/exec`
6. Set URL này vào `paymentbox_config.h` constant `GAS_BASE_URL` trước khi build firmware

### Bước 5 — Test bằng curl

```bash
# Heartbeat check
curl "https://script.google.com/macros/s/SHEET_ID/exec?action=getHeartbeat&device_id=ESG-PB-A4B7C9D2"
# → {"ok":true,"last_heartbeat_ms":1716192345678,"age_s":42}

# OTA check (no update)
curl "https://script.google.com/macros/s/SHEET_ID/exec?action=ota_check&device_id=ESG-PB-A4B7C9D2&version=1.0.0&hw=V8.2"
# → {"update_available":false}

# OTA check (có update — sau khi thêm row trong sheet Firmware)
# → {"update_available":true,"version":"1.1.0","url":"https://...","min_battery_v":3.5,...}
```

---

## Local testing (không cần deploy GAS)

```bash
cd backend/gas_v9_5
node test_gas_endpoints.js

# Expected:
# ========================================
#   42 pass / 0 fail
# ========================================
```

Test mock `SpreadsheetApp` + `ContentService` + `Logger` để chạy logic ngoài GAS runtime. Khi sửa endpoint, chạy lại test này trước khi push lên GAS.

---

## Bảo mật

- ✅ Endpoint public (GAS Web App "Anyone") nhưng KHÔNG trả thông tin nhạy cảm
- ✅ Validate input chống injection: regex strict cho device_id, version, hw
- ✅ Anti-downgrade: refuse OTA nếu version ≤ current
- ✅ Empty/invalid rows trong sheet Firmware bị skip
- ⚠️ Rate limit dựa vào quota tự nhiên của GAS (~20 req/giây) — đủ cho deploy <1000 device

**KHÔNG** dùng endpoint này cho secrets/auth — chỉ là pull-only metadata.

---

## Quota & Performance

- GAS execution time limit: 6 phút/request (thừa thãi)
- GAS daily quota Web App: 20K calls/day free
- Mỗi PaymentBox: 60s heartbeat + 24h OTA check = ~1450 calls/device/day
- → Free tier hỗ trợ tối đa **~13 device**
- Khi scale > 13 device: chuyển sang Cloud Functions hoặc Firebase RTDB (cheaper, faster)

---

## TODO sau khi deploy

- [ ] Verify 2 endpoint trả JSON đúng format qua curl
- [ ] Add 1 row test vào sheet `Firmware` với device ESG-PB-test
- [ ] Set `GAS_BASE_URL` trong firmware `paymentbox_config.h`
- [ ] Test end-to-end: ESP32 booted → check log thấy poll thành công
- [ ] Add monitoring: nếu GAS down >10min → Telegram alert (extend `_handleError()`)
