/**
 * test_gas_endpoints.js — Unit tests cho GAS V9.5 PaymentBox endpoints.
 *
 * Chạy: node test_gas_endpoints.js
 *
 * Mock SpreadsheetApp + ContentService + Logger để test logic mà không cần GAS runtime.
 */

// =============== MOCKS ===============

global.Logger = { log: function() {} };

global.ContentService = {
  MimeType: { JSON: 'application/json' },
  createTextOutput: function(s) {
    return {
      _content: s,
      _mime: '',
      setMimeType: function(m) { this._mime = m; return this; },
      getContent: function() { return this._content; },
    };
  },
};

// Mock SpreadsheetApp với data injectable
let mockSheets = {};

global.SpreadsheetApp = {
  getActiveSpreadsheet: function() {
    return {
      getSheetByName: function(name) {
        if (!mockSheets[name]) return null;
        const data = mockSheets[name];
        return {
          getLastRow: function() { return data.length; },
          getRange: function(row, col, numRows, numCols) {
            return {
              getValues: function() {
                const result = [];
                for (let i = row - 1; i < row - 1 + numRows; i++) {
                  if (i >= data.length) break;
                  result.push(data[i].slice(col - 1, col - 1 + numCols));
                }
                return result;
              },
            };
          },
        };
      },
    };
  },
};

// =============== LOAD MODULE ===============

const G = require('./gas_v9_5_paymentbox_endpoints.js');

// =============== TEST RUNNER ===============

let pass = 0, fail = 0;
const failures = [];

function test(name, fn) {
  try {
    fn();
    pass++;
    console.log('  ✓ ' + name);
  } catch (e) {
    fail++;
    failures.push({ name, error: e.message });
    console.log('  ✗ ' + name + ' — ' + e.message);
  }
}

function eq(actual, expected, msg) {
  if (actual !== expected) {
    throw new Error((msg || '') + ` expected=${JSON.stringify(expected)} actual=${JSON.stringify(actual)}`);
  }
}

function truthy(v, msg) {
  if (!v) throw new Error((msg || 'expected truthy') + ` got ${v}`);
}

function falsy(v, msg) {
  if (v) throw new Error((msg || 'expected falsy') + ` got ${v}`);
}

function parseResp(resp) {
  return JSON.parse(resp.getContent());
}

// =============== TESTS: versionCmp ===============

console.log('\n=== _versionCmp_ ===');

test('versionCmp basic greater', () => {
  truthy(G._versionCmp_('1.1.0', '1.0.0') > 0);
});

test('versionCmp basic less', () => {
  truthy(G._versionCmp_('1.0.0', '1.1.0') < 0);
});

test('versionCmp equal', () => {
  eq(G._versionCmp_('1.0.0', '1.0.0'), 0);
});

test('versionCmp patch', () => {
  truthy(G._versionCmp_('1.0.5', '1.0.4') > 0);
});

test('versionCmp major dominates', () => {
  truthy(G._versionCmp_('2.0.0', '1.99.99') > 0);
});

test('versionCmp partial "1.0" vs "1.0.0"', () => {
  eq(G._versionCmp_('1.0', '1.0.0'), 0);
});

test('versionCmp invalid inputs treated as zeros', () => {
  eq(G._versionCmp_('abc', 'xyz'), 0);
});

// =============== TESTS: matchesPattern ===============

console.log('\n=== _matchesPattern_ ===');

test('matchesPattern exact match', () => {
  truthy(G._matchesPattern_('ESG-PB-A4B7C9D2', 'ESG-PB-A4B7C9D2'));
});

test('matchesPattern exact mismatch', () => {
  falsy(G._matchesPattern_('ESG-PB-A4B7C9D2', 'ESG-PB-XXXXXXXX'));
});

test('matchesPattern trailing wildcard', () => {
  truthy(G._matchesPattern_('ESG-PB-A4B7C9D2', 'ESG-PB-*'));
});

test('matchesPattern wildcard no prefix match', () => {
  falsy(G._matchesPattern_('ESG-OTHER-XXX', 'ESG-PB-*'));
});

test('matchesPattern wildcard all', () => {
  truthy(G._matchesPattern_('ANYTHING', '*'));
});

test('matchesPattern empty pattern rejected', () => {
  falsy(G._matchesPattern_('ESG-PB-X', ''));
});

test('matchesPattern middle wildcard rejected (unsafe)', () => {
  falsy(G._matchesPattern_('ESG-PB-X', 'ESG-*-X'));
});

// =============== TESTS: parseTimestamp ===============

console.log('\n=== _parseTimestamp_ ===');

test('parseTimestamp Date object', () => {
  const d = new Date('2026-05-20T08:00:00Z');
  eq(G._parseTimestamp_(d), d.getTime());
});

test('parseTimestamp ISO string', () => {
  const ms = G._parseTimestamp_('2026-05-20T08:00:00Z');
  truthy(ms > 1.7e12);
});

test('parseTimestamp epoch ms', () => {
  eq(G._parseTimestamp_(1716192000000), 1716192000000);
});

test('parseTimestamp epoch seconds auto-detect', () => {
  eq(G._parseTimestamp_(1716192000), 1716192000000);
});

test('parseTimestamp invalid → 0', () => {
  eq(G._parseTimestamp_('not a date'), 0);
});

test('parseTimestamp null/undefined → 0', () => {
  eq(G._parseTimestamp_(null), 0);
  eq(G._parseTimestamp_(undefined), 0);
  eq(G._parseTimestamp_(''), 0);
});

// =============== TESTS: findFirmwareMatch ===============

console.log('\n=== _findFirmwareMatch_ ===');

test('findFirmwareMatch — exact pattern + newer version', () => {
  const rows = [
    ['ESG-PB-A4B7C9D2', 'V8.2', '1.1.0', 'https://example.com/fw.bin', 'sha256abc', 3.5, 'Bug fix'],
  ];
  const m = G._findFirmwareMatch_(rows, 'ESG-PB-A4B7C9D2', 'V8.2', '1.0.0');
  truthy(m);
  eq(m.new_version, '1.1.0');
  eq(m.url, 'https://example.com/fw.bin');
  eq(m.min_battery_v, 3.5);
  eq(m.sha256, 'sha256abc');
});

test('findFirmwareMatch — wildcard pattern', () => {
  const rows = [
    ['ESG-PB-*', 'V8.2', '1.2.0', 'https://example.com/fw.bin', '', 3.5, 'Wildcard'],
  ];
  const m = G._findFirmwareMatch_(rows, 'ESG-PB-A4B7C9D2', 'V8.2', '1.0.0');
  truthy(m);
  eq(m.new_version, '1.2.0');
});

test('findFirmwareMatch — same version → no update', () => {
  const rows = [
    ['ESG-PB-*', 'V8.2', '1.0.0', 'https://example.com/fw.bin', '', 3.5, ''],
  ];
  const m = G._findFirmwareMatch_(rows, 'ESG-PB-X', 'V8.2', '1.0.0');
  falsy(m, 'same version should not match');
});

test('findFirmwareMatch — older version → no update (anti-downgrade)', () => {
  const rows = [
    ['ESG-PB-*', 'V8.2', '0.9.0', 'https://example.com/fw.bin', '', 3.5, ''],
  ];
  const m = G._findFirmwareMatch_(rows, 'ESG-PB-X', 'V8.2', '1.0.0');
  falsy(m);
});

test('findFirmwareMatch — hw mismatch', () => {
  const rows = [
    ['ESG-PB-*', 'V9.0', '1.1.0', 'https://example.com/fw.bin', '', 3.5, ''],
  ];
  const m = G._findFirmwareMatch_(rows, 'ESG-PB-X', 'V8.2', '1.0.0');
  falsy(m);
});

test('findFirmwareMatch — empty url skipped', () => {
  const rows = [
    ['ESG-PB-*', 'V8.2', '1.1.0', '', '', 3.5, ''],
  ];
  const m = G._findFirmwareMatch_(rows, 'ESG-PB-X', 'V8.2', '1.0.0');
  falsy(m, 'empty url should be skipped');
});

test('findFirmwareMatch — invalid version format skipped', () => {
  const rows = [
    ['ESG-PB-*', 'V8.2', 'bad-version', 'https://example.com/fw.bin', '', 3.5, ''],
  ];
  const m = G._findFirmwareMatch_(rows, 'ESG-PB-X', 'V8.2', '1.0.0');
  falsy(m);
});

test('findFirmwareMatch — first match wins', () => {
  const rows = [
    ['ESG-PB-*',         'V8.2', '1.1.0', 'https://a.com/fw.bin', '', 3.5, 'first'],
    ['ESG-PB-A4B7C9D2', 'V8.2', '1.2.0', 'https://b.com/fw.bin', '', 3.5, 'second'],
  ];
  const m = G._findFirmwareMatch_(rows, 'ESG-PB-A4B7C9D2', 'V8.2', '1.0.0');
  truthy(m);
  eq(m.new_version, '1.1.0', 'first row wins');
  eq(m.release_notes, 'first');
});

test('findFirmwareMatch — empty rows', () => {
  const m = G._findFirmwareMatch_([], 'ESG-PB-X', 'V8.2', '1.0.0');
  falsy(m);
});

test('findFirmwareMatch — min_battery_v defaults to 3.5 when missing', () => {
  const rows = [
    ['ESG-PB-*', 'V8.2', '1.1.0', 'https://example.com/fw.bin', '', '', ''],
  ];
  const m = G._findFirmwareMatch_(rows, 'ESG-PB-X', 'V8.2', '1.0.0');
  truthy(m);
  eq(m.min_battery_v, 3.5);
});

// =============== TESTS: handleGetHeartbeat_ ===============

console.log('\n=== handleGetHeartbeat_ ===');

test('getHeartbeat — invalid device_id', () => {
  mockSheets = { BankHeartbeat: [['device_id', 'last_seen'], ['ESG-PB-X', new Date()]] };
  const r = parseResp(G.handleGetHeartbeat_({ parameter: { device_id: 'bad id with spaces!' } }));
  eq(r.ok, false);
  eq(r.error, 'invalid_device_id');
});

test('getHeartbeat — sheet missing returns no heartbeat', () => {
  mockSheets = {};
  const r = parseResp(G.handleGetHeartbeat_({ parameter: { device_id: 'ESG-PB-A4B7C9D2' } }));
  eq(r.ok, true);
  eq(r.age_s, -1);
});

test('getHeartbeat — no heartbeat for device', () => {
  mockSheets = {
    BankHeartbeat: [
      ['device_id', 'last_seen'],
      ['ESG-PB-OTHER', new Date()],
    ],
  };
  const r = parseResp(G.handleGetHeartbeat_({ parameter: { device_id: 'ESG-PB-A4B7C9D2' } }));
  eq(r.ok, true);
  eq(r.age_s, -1);
});

test('getHeartbeat — finds recent heartbeat', () => {
  const recent = new Date(Date.now() - 30 * 1000);  // 30 giây trước
  mockSheets = {
    BankHeartbeat: [
      ['device_id', 'last_seen'],
      ['ESG-PB-A4B7C9D2', recent],
    ],
  };
  const r = parseResp(G.handleGetHeartbeat_({ parameter: { device_id: 'ESG-PB-A4B7C9D2' } }));
  eq(r.ok, true);
  truthy(r.age_s >= 29 && r.age_s <= 31, 'age_s around 30s');
});

test('getHeartbeat — ISO string timestamp', () => {
  const isoString = new Date(Date.now() - 60 * 1000).toISOString();
  mockSheets = {
    BankHeartbeat: [
      ['device_id', 'last_seen'],
      ['ESG-PB-A4B7C9D2', isoString],
    ],
  };
  const r = parseResp(G.handleGetHeartbeat_({ parameter: { device_id: 'ESG-PB-A4B7C9D2' } }));
  eq(r.ok, true);
  truthy(r.age_s >= 59 && r.age_s <= 61);
});

// =============== TESTS: handleOtaCheck_ ===============

console.log('\n=== handleOtaCheck_ ===');

test('otaCheck — invalid device_id', () => {
  mockSheets = {};
  const r = parseResp(G.handleOtaCheck_({ parameter: { device_id: 'bad!', version: '1.0.0', hw: 'V8.2' } }));
  eq(r.update_available, false);
  eq(r.error, 'invalid_device_id');
});

test('otaCheck — invalid version format', () => {
  mockSheets = {};
  const r = parseResp(G.handleOtaCheck_({ parameter: { device_id: 'ESG-PB-A4B7C9D2', version: 'beta', hw: 'V8.2' } }));
  eq(r.update_available, false);
  eq(r.error, 'invalid_version');
});

test('otaCheck — invalid hw format', () => {
  mockSheets = {};
  const r = parseResp(G.handleOtaCheck_({ parameter: { device_id: 'ESG-PB-A4B7C9D2', version: '1.0.0', hw: 'X' } }));
  eq(r.update_available, false);
  eq(r.error, 'invalid_hw');
});

test('otaCheck — no firmware sheet', () => {
  mockSheets = {};
  const r = parseResp(G.handleOtaCheck_({ parameter: { device_id: 'ESG-PB-A4B7C9D2', version: '1.0.0', hw: 'V8.2' } }));
  eq(r.update_available, false);
});

test('otaCheck — update available', () => {
  mockSheets = {
    Firmware: [
      ['device_pattern', 'hw', 'new_version', 'url', 'sha256', 'min_battery_v', 'release_notes'],
      ['ESG-PB-*', 'V8.2', '1.1.0', 'https://drive.google.com/fw.bin', 'abc123', 3.5, 'Fix LED'],
    ],
  };
  const r = parseResp(G.handleOtaCheck_({ parameter: { device_id: 'ESG-PB-A4B7C9D2', version: '1.0.0', hw: 'V8.2' } }));
  eq(r.update_available, true);
  eq(r.version, '1.1.0');
  eq(r.url, 'https://drive.google.com/fw.bin');
  eq(r.sha256, 'abc123');
  eq(r.min_battery_v, 3.5);
  eq(r.release_notes, 'Fix LED');
});

test('otaCheck — no update (current = latest)', () => {
  mockSheets = {
    Firmware: [
      ['device_pattern', 'hw', 'new_version', 'url', 'sha256', 'min_battery_v', 'release_notes'],
      ['ESG-PB-*', 'V8.2', '1.0.0', 'https://drive.google.com/fw.bin', '', 3.5, ''],
    ],
  };
  const r = parseResp(G.handleOtaCheck_({ parameter: { device_id: 'ESG-PB-A4B7C9D2', version: '1.0.0', hw: 'V8.2' } }));
  eq(r.update_available, false);
});

test('otaCheck — full integration with real-world data', () => {
  mockSheets = {
    Firmware: [
      ['device_pattern', 'hw', 'new_version', 'url', 'sha256', 'min_battery_v', 'release_notes'],
      ['ESG-PB-A4B7C9D2', 'V8.2', '1.2.0', 'https://drive.google.com/uc?id=ABC', 'def456', 3.6, 'Specific device fix'],
      ['ESG-PB-*',         'V8.2', '1.1.0', 'https://drive.google.com/uc?id=XYZ', 'abc123', 3.5, 'General update'],
    ],
  };
  // First-row match wins → specific device update
  const r = parseResp(G.handleOtaCheck_({ parameter: { device_id: 'ESG-PB-A4B7C9D2', version: '1.0.0', hw: 'V8.2' } }));
  eq(r.update_available, true);
  eq(r.version, '1.2.0');
  eq(r.min_battery_v, 3.6);
});

// =============== SUMMARY ===============

console.log('\n========================================');
console.log(`  ${pass} pass / ${fail} fail`);
console.log('========================================');

if (fail > 0) {
  console.log('\nFailures:');
  failures.forEach(f => console.log(`  - ${f.name}: ${f.error}`));
  process.exit(1);
}
