# GAS V9.5 Backend Endpoints — PaymentBox V8.2

Specification cho các endpoint mà GAS phải implement để hỗ trợ firmware V8.2.

---

## 1. Heartbeat status check

Dùng cho `gas_poller` xác nhận phone đang push GAS thành công.

```
GET <GAS_URL>?action=getHeartbeat&device_id=<id>
```

**Query params**:
- `device_id` — định danh thiết bị, format `ESG-PB-XXXXXXXX` (8 hex)

**Response (success)**:
```json
{
  "ok": true,
  "last_heartbeat_ms": 1716192345678,
  "age_s": 42
}
```

**Response (no heartbeat)**:
```json
{
  "ok": true,
  "last_heartbeat_ms": 0,
  "age_s": -1
}
```

**Response (error)**: HTTP 5xx hoặc body có `"ok":false`. Firmware sẽ publish `EV_GAS_UNREACHABLE`.

**GAS implementation** (thêm vào monolith GAS V9.5):
```javascript
function doGet(e) {
  if (e.parameter.action === 'getHeartbeat') {
    return handleGetHeartbeat(e.parameter.device_id);
  }
  // ... existing actions
}

function handleGetHeartbeat(deviceId) {
  const sheet = SpreadsheetApp
    .openById(SHEET_ID)
    .getSheetByName('BankHeartbeat');
  const last = sheet.getRange('B' + sheet.getLastRow()).getValue();
  const lastMs = last ? new Date(last).getTime() : 0;
  const ageS = lastMs ? Math.floor((Date.now() - lastMs) / 1000) : -1;
  return ContentService
    .createTextOutput(JSON.stringify({ ok: true, last_heartbeat_ms: lastMs, age_s: ageS }))
    .setMimeType(ContentService.MimeType.JSON);
}
```

---

## 2. OTA update check

Dùng cho `ota_updater` task định kỳ 24h hoặc trigger thủ công.

```
GET <GAS_URL>?action=ota_check&device_id=<id>&version=<x.y.z>&hw=<hw_version>
```

**Query params**:
- `device_id` — định danh thiết bị
- `version` — version firmware đang chạy (e.g. `1.0.0`)
- `hw` — phiên bản phần cứng (e.g. `V8.2`)

**Response (có update)**:
```json
{
  "update_available": true,
  "version": "1.1.0",
  "url": "https://drive.google.com/uc?export=download&id=ABCDEF/firmware.bin",
  "sha256": "abc123def456...",
  "min_battery_v": 3.5,
  "release_notes": "Fix LED flicker, add NTP sync"
}
```

**Response (không có update)**:
```json
{"update_available": false}
```

**Important constraints**:
- URL phải là direct download link (Google Drive yêu cầu `&export=download`)
- Phải support HTTPS (esp_tls dùng CA bundle Google đã trusted)
- File firmware bin **không nén** (esp_https_ota stream raw bytes)
- Size firmware < 1MB (vừa với partition `ota_0`/`ota_1` 1MB)
- `min_battery_v` ngăn flash khi pin yếu (firmware skip OTA nếu V_BAT < threshold)

**GAS implementation** (firmware version table trong sheet `Firmware`):

| device_pattern | min_version | new_version | url | sha256 | min_battery_v | release_notes |
|---|---|---|---|---|---|---|
| ESG-PB-* | 1.0.0 | 1.1.0 | https://... | abc123... | 3.5 | Fix LED |

```javascript
function handleOtaCheck(deviceId, currentVer, hw) {
  const sheet = SpreadsheetApp.openById(SHEET_ID).getSheetByName('Firmware');
  const rows = sheet.getDataRange().getValues();
  // Match device_pattern (wildcard) AND hw version
  for (const r of rows.slice(1)) {
    const [pattern, minVer, newVer, url, sha256, minBat, notes, hwMatch] = r;
    if (matchesPattern(deviceId, pattern) && hwMatch === hw
        && versionGt(newVer, currentVer)) {
      return jsonResponse({
        update_available: true,
        version: newVer,
        url: url,
        sha256: sha256,
        min_battery_v: minBat,
        release_notes: notes,
      });
    }
  }
  return jsonResponse({ update_available: false });
}
```

---

## 3. Telegram alert (cho EV_SYSTEM_ALERT)

Firmware chưa gọi endpoint này nhưng có thể thêm:

```
POST <GAS_URL>?action=alert
Body: {"device_id":"ESG-PB-...","level":"alert","message":"Phone dead 5min"}
```

GAS forward sang Telegram bot existing.

---

## 4. Migration plan từ GAS V9.5 hiện tại

GAS V9.5 đã có:
- `BankHeartbeat` sheet — phone push heartbeat ✓
- Telegram dedup alerts ✓
- DLQ ✓
- LockService race protection ✓

**Cần thêm**:
1. Sheet mới `Firmware` (version registry như bảng trên)
2. Function `handleGetHeartbeat()` — đọc BankHeartbeat sheet
3. Function `handleOtaCheck()` — đọc Firmware sheet
4. Update `doGet()` switch case cho 2 action mới

Estimate effort: **~2 giờ** thêm 80 dòng JS vào GAS monolith.

---

## 5. Test endpoints bằng curl

```bash
# Heartbeat check
curl "https://script.google.com/macros/s/SHEET_ID/exec?action=getHeartbeat&device_id=ESG-PB-A4B7C9D2"
# → {"ok":true,"last_heartbeat_ms":1716192345678,"age_s":42}

# OTA check
curl "https://script.google.com/macros/s/SHEET_ID/exec?action=ota_check&device_id=ESG-PB-A4B7C9D2&version=1.0.0&hw=V8.2"
# → {"update_available":false}

# Hoặc với update mới
# → {"update_available":true,"version":"1.1.0","url":"https://...","min_battery_v":3.5}
```

Khi 2 endpoint trên hoạt động đúng, firmware V8.2 sẵn sàng chạy.
