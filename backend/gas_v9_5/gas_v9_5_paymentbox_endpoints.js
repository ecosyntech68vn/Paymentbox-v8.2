/**
 * gas_v9_5_paymentbox_endpoints.js
 *
 * EcoSynTech GAS Backend V9.5 — Endpoints bổ sung cho PaymentBox V8.2.
 *
 * HƯỚNG DẪN TÍCH HỢP:
 * 1. Copy nội dung file này, paste vào GAS Editor (script.google.com)
 *    dưới dạng file mới "PaymentBoxEndpoints.gs"
 * 2. Sửa hàm doGet() hiện tại của bạn để route 2 action mới:
 *    if (e.parameter.action === 'getHeartbeat') return handleGetHeartbeat_(e);
 *    if (e.parameter.action === 'ota_check')    return handleOtaCheck_(e);
 * 3. Tạo 2 sheet mới trong spreadsheet (nếu chưa có):
 *    - "BankHeartbeat" — cột A: device_id, cột B: last_seen_iso
 *    - "Firmware"     — registry firmware updates (xem schema bên dưới)
 * 4. Deploy → New deployment → Web app → Execute as: Me, Access: Anyone
 *
 * SCHEMA SHEET "Firmware" (registry firmware OTA):
 * | A: device_pattern | B: hw    | C: new_version | D: url      | E: sha256 | F: min_battery_v | G: release_notes |
 * |-------------------|----------|----------------|-------------|-----------|------------------|------------------|
 * | ESG-PB-*          | V8.2     | 1.1.0          | https://... | abc123... | 3.5              | Fix LED flicker  |
 *
 * device_pattern hỗ trợ wildcard '*':
 *   "ESG-PB-*"  → match tất cả PaymentBox
 *   "ESG-PB-A4B7C9D2" → chỉ device cụ thể
 *
 * BẢO MẬT:
 * - Endpoint là public (GAS web app deploy "Anyone")
 * - Không trả về thông tin nhạy cảm
 * - Validate input để chống injection
 * - Rate limit dựa vào Apps Script quota tự nhiên (~20 req/giây)
 */

// ============================================================
// CONSTANTS — adjust để match spreadsheet của bạn
// ============================================================
const PB_HEARTBEAT_SHEET = 'BankHeartbeat';
const PB_FIRMWARE_SHEET  = 'Firmware';
const PB_DEVICE_ID_REGEX = /^[A-Z0-9\-]{8,32}$/;   // chấp nhận ESG-PB-XXXXXXXX, MAC-based, v.v.
const PB_VERSION_REGEX   = /^\d+\.\d+\.\d+$/;       // semver-lite "X.Y.Z"
const PB_HW_REGEX        = /^V\d+(\.\d+)?$/;        // "V8.2", "V9", v.v.

// ============================================================
// PUBLIC HANDLERS — gọi từ doGet() router
// ============================================================

/**
 * GET ?action=getHeartbeat&device_id=<id>
 * → {ok: true, last_heartbeat_ms: <ms>, age_s: <int>}
 *   age_s = -1 nếu chưa có heartbeat nào
 */
function handleGetHeartbeat_(e) {
  const deviceId = (e.parameter.device_id || '').trim();

  if (!PB_DEVICE_ID_REGEX.test(deviceId)) {
    return _jsonResponse({ ok: false, error: 'invalid_device_id' });
  }

  try {
    const sheet = SpreadsheetApp.getActiveSpreadsheet().getSheetByName(PB_HEARTBEAT_SHEET);
    if (!sheet) {
      return _jsonResponse({ ok: true, last_heartbeat_ms: 0, age_s: -1 });
    }

    const lastMs = _findLastHeartbeatMs_(sheet, deviceId);
    if (lastMs === 0) {
      return _jsonResponse({ ok: true, last_heartbeat_ms: 0, age_s: -1 });
    }

    const ageS = Math.floor((Date.now() - lastMs) / 1000);
    return _jsonResponse({ ok: true, last_heartbeat_ms: lastMs, age_s: ageS });
  } catch (err) {
    Logger.log('handleGetHeartbeat_ error: ' + err);
    return _jsonResponse({ ok: false, error: 'internal' });
  }
}

/**
 * GET ?action=ota_check&device_id=<id>&version=<x.y.z>&hw=<v8.2>
 * → {update_available: bool, version, url, sha256?, min_battery_v?, release_notes?}
 */
function handleOtaCheck_(e) {
  const deviceId   = (e.parameter.device_id || '').trim();
  const currentVer = (e.parameter.version   || '').trim();
  const hw         = (e.parameter.hw        || '').trim();

  if (!PB_DEVICE_ID_REGEX.test(deviceId)) {
    return _jsonResponse({ update_available: false, error: 'invalid_device_id' });
  }
  if (!PB_VERSION_REGEX.test(currentVer)) {
    return _jsonResponse({ update_available: false, error: 'invalid_version' });
  }
  if (!PB_HW_REGEX.test(hw)) {
    return _jsonResponse({ update_available: false, error: 'invalid_hw' });
  }

  try {
    const sheet = SpreadsheetApp.getActiveSpreadsheet().getSheetByName(PB_FIRMWARE_SHEET);
    if (!sheet || sheet.getLastRow() < 2) {
      return _jsonResponse({ update_available: false });
    }

    const rows = sheet.getRange(2, 1, sheet.getLastRow() - 1, 7).getValues();
    const match = _findFirmwareMatch_(rows, deviceId, hw, currentVer);

    if (!match) {
      return _jsonResponse({ update_available: false });
    }

    return _jsonResponse({
      update_available: true,
      version: match.new_version,
      url: match.url,
      sha256: match.sha256 || '',
      min_battery_v: typeof match.min_battery_v === 'number' ? match.min_battery_v : 3.5,
      release_notes: match.release_notes || '',
    });
  } catch (err) {
    Logger.log('handleOtaCheck_ error: ' + err);
    return _jsonResponse({ update_available: false, error: 'internal' });
  }
}

// ============================================================
// PURE LOGIC — testable không cần SpreadsheetApp
// ============================================================

/**
 * So sánh 2 version "X.Y.Z" theo semver-lite.
 * @return {number} > 0 nếu a > b, < 0 nếu a < b, 0 nếu bằng.
 */
function _versionCmp_(a, b) {
  const pa = String(a).split('.').map(function (n) { return parseInt(n, 10) || 0; });
  const pb = String(b).split('.').map(function (n) { return parseInt(n, 10) || 0; });
  for (let i = 0; i < 3; i++) {
    const va = pa[i] || 0;
    const vb = pb[i] || 0;
    if (va !== vb) return va - vb;
  }
  return 0;
}

/**
 * Match device_id theo wildcard '*' (chỉ trailing wildcard, ví dụ "ESG-PB-*").
 */
function _matchesPattern_(deviceId, pattern) {
  if (!pattern) return false;
  if (pattern === '*') return true;
  if (pattern.indexOf('*') === -1) {
    return deviceId === pattern;
  }
  // Trailing wildcard "ABC-*" → starts with "ABC-"
  if (pattern.endsWith('*')) {
    const prefix = pattern.slice(0, -1);
    return deviceId.startsWith(prefix);
  }
  // Chỉ hỗ trợ trailing wildcard cho safety/speed; reject others
  return false;
}

/**
 * Tìm row firmware match device + hw + có version mới hơn current.
 * @param {Array<Array>} rows — values từ getRange().getValues() (đã skip header)
 * @return {object|null}
 */
function _findFirmwareMatch_(rows, deviceId, hw, currentVer) {
  for (let i = 0; i < rows.length; i++) {
    const r = rows[i];
    const pattern    = String(r[0] || '').trim();
    const rowHw      = String(r[1] || '').trim();
    const newVersion = String(r[2] || '').trim();
    const url        = String(r[3] || '').trim();
    const sha256     = String(r[4] || '').trim();
    const minBat     = parseFloat(r[5]);
    const notes      = String(r[6] || '').trim();

    if (!pattern || !rowHw || !newVersion || !url) continue;
    if (!PB_VERSION_REGEX.test(newVersion))         continue;
    if (rowHw !== hw)                                continue;
    if (!_matchesPattern_(deviceId, pattern))        continue;
    if (_versionCmp_(newVersion, currentVer) <= 0)   continue;

    return {
      new_version: newVersion,
      url: url,
      sha256: sha256,
      min_battery_v: isNaN(minBat) ? 3.5 : minBat,
      release_notes: notes,
    };
  }
  return null;
}

/**
 * Tìm timestamp heartbeat gần nhất cho device từ sheet.
 * Sheet schema: col A = device_id, col B = ISO datetime hoặc Date object.
 * Trả về 0 nếu không thấy.
 *
 * Tách thành function riêng để testable + xử lý nhiều format timestamp.
 */
function _findLastHeartbeatMs_(sheet, deviceId) {
  const lastRow = sheet.getLastRow();
  if (lastRow < 2) return 0;

  const data = sheet.getRange(2, 1, lastRow - 1, 2).getValues();
  let latest = 0;
  for (let i = data.length - 1; i >= 0; i--) {
    if (String(data[i][0]).trim() === deviceId) {
      const ts = _parseTimestamp_(data[i][1]);
      if (ts > latest) latest = ts;
      // Sheet thường append theo thứ tự, row cuối là mới nhất → break early
      if (latest > 0) break;
    }
  }
  return latest;
}

/**
 * Parse timestamp về ms. Hỗ trợ:
 * - Date object (GAS auto-convert)
 * - ISO string "2026-05-20T08:00:00Z"
 * - Number ms (epoch)
 */
function _parseTimestamp_(v) {
  if (!v) return 0;
  if (v instanceof Date) return v.getTime();
  if (typeof v === 'number') {
    // Nếu < 10^12 thì là giây, ngược lại là ms
    return v < 1e12 ? v * 1000 : v;
  }
  if (typeof v === 'string') {
    const ms = Date.parse(v);
    return isNaN(ms) ? 0 : ms;
  }
  return 0;
}

// ============================================================
// HELPERS
// ============================================================

function _jsonResponse(obj) {
  return ContentService
    .createTextOutput(JSON.stringify(obj))
    .setMimeType(ContentService.MimeType.JSON);
}

// ============================================================
// EXPORT cho node.js unit test (no-op trên GAS runtime)
// ============================================================
if (typeof module !== 'undefined' && module.exports) {
  module.exports = {
    _versionCmp_,
    _matchesPattern_,
    _findFirmwareMatch_,
    _findLastHeartbeatMs_,
    _parseTimestamp_,
    handleGetHeartbeat_,
    handleOtaCheck_,
    PB_HEARTBEAT_SHEET,
    PB_FIRMWARE_SHEET,
  };
}
