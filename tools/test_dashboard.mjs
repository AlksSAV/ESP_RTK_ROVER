/**
 * Run web/rtk-monitor.html's <script> in node against a DOM stub and check what
 * the dashboard actually renders for known receiver lines.
 *
 * The dashboard is one HTML file with no build step and no tests, so every
 * regression in it - a field parsed from the wrong column, a card filled only on
 * one code path, markup left visible - ships silently to the installer image.
 * This harness feeds real sentences through the same line splitter the transport
 * uses and asserts on the resulting DOM text.
 *
 * Requires node (any modern version). Run from the project root:
 *
 *     node tools/test_dashboard.mjs
 *
 * Exits non-zero on the first failed expectation.
 */
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';

const ROOT = path.resolve(import.meta.dirname, '..');
const HTML = path.join(ROOT, 'web', 'rtk-monitor.html');

const html = fs.readFileSync(HTML, 'utf8');
const script = html.slice(html.lastIndexOf('<script>') + 8, html.lastIndexOf('</script>'));
if (!script) {
  console.error('no <script> block in', HTML);
  process.exit(1);
}

// ---------------------------------------------------------------------------
// Minimal DOM: the dashboard only touches these members.
// ---------------------------------------------------------------------------
const mkEl = () => ({
  textContent: '', innerHTML: '', className: '', href: '', hidden: false,
  disabled: false, scrollHeight: 0, scrollTop: 0, clientHeight: 0,
  style: { setProperty() {} }, addEventListener() {},
});

const els = new Map();
const el = id => {
  if (!els.has(id)) els.set(id, mkEl());
  return els.get(id);
};

const ctx = {
  console, performance,
  setInterval: () => 0, setTimeout: () => 0, clearTimeout: () => {},
  TextDecoder,
  location: { hostname: 'localhost' },   // not the rover AP: no WebSocket path
  navigator: {},
  document: { getElementById: el },
};
ctx.globalThis = ctx;
vm.createContext(ctx);
vm.runInContext(script, ctx);

const run = expr => vm.runInContext(expr, ctx);

// feedBytes() is the WebSocket transport's entry point and the BLE handler uses
// the same splitter, so a line that survives this call reaches the parser in
// both transports.
const feed = line => run(`feedBytes(${JSON.stringify(line + '\r\n')}, ${line.length + 2})`);

// ---------------------------------------------------------------------------
// Sample sentences
// ---------------------------------------------------------------------------
const nmea = body => {
  body = body.replace(/^\$/, '');
  let cs = 0;
  for (const ch of body) cs ^= ch.charCodeAt(0);
  return `$${body}*${cs.toString(16).toUpperCase().padStart(2, '0')}`;
};

// BESTPOSA example from the reference manual, V3.1 §3.2.5.1.
const BESTPOS_PPP = '#BESTPOSA,COM3,0,60.0,FINESTEERING,2329,443319.000,00000000,0000,1114'
  + ';SOL_COMPUTED,PPP_CONVERGING,31.35087016848,121.29169259959,31.9559,10.3015,WGS84,'
  + '0.0295,0.0295,0.1212,"0000",22.000,127.000,48,19,47,47,207,0,16,0*4a980949';

// A fixed solution, the shape the bench tool saw on FW 7.4.6.
const BESTPOS_INT = '#BESTPOSA,COM1,0,60.0,FINESTEERING,2442,403900.000,00000000,0000,1114'
  + ';SOL_COMPUTED,NARROW_INT,45.26160271333,38.13065058833,18.4591,13.7240,WGS84,'
  + '0.0081,0.0064,0.0152,"0004",1.000,120.000,32,24,30,30,0,0,0,0*55667788';

// What the board emits with no sky: no solution, every numeric field zero.
const BESTPOS_NONE = '#BESTPOSA,COM1,0,60.0,FINESTEERING,2442,403819.100,00000000,0000,1114'
  + ';COLD_START,NONE,0.00000000000,0.00000000000,0.0000,0.0000,WGS84,'
  + '0.0000,0.0000,0.0000,"0000",0.000,0.000,0,0,0,0,0,0,0,0*11223344';

const GGA_FIX = nmea('GPGGA,155506.00,4515.6961628,N,03807.8390353,E,4,24,0.8,4.7351,M,13.724,M,1.0,0004');
const GGA_SINGLE = nmea('GPGGA,155506.00,4515.6961628,N,03807.8390353,E,1,16,0.9,4.7351,M,13.724,M,,');
const GST = nmea('GPGST,155506.00,1.05,0.63,0.48,31.3403,1.1315,1.9790,4.2285');

// ---------------------------------------------------------------------------
// Assertions
// ---------------------------------------------------------------------------
let failures = 0;
let checks = 0;

const text = id => {
  const e = els.get(id);
  if (!e) return undefined;
  return (e.innerHTML || e.textContent || '').replace(/<u>/g, ' ').replace(/<\/u>/g, '').trim();
};

function check(what, got, want) {
  checks++;
  const ok = typeof want === 'function' ? want(got) : got === want;
  if (!ok) {
    failures++;
    console.log(`  FAIL  ${what}: got ${JSON.stringify(got)}`);
  } else {
    console.log(`  ok    ${what} = ${JSON.stringify(got)}`);
  }
}

const reset = () => run('S.bestpos = null; S.dirty = true; render()');

// --- 1. BESTPOSA reaches the parser at all --------------------------------
console.log('\n1. #BESTPOSA survives the transport filter');
reset();
feed(BESTPOS_PPP);
check('sentences', run('S.sentences'), 1);
check('pos type', run('S.bestpos && S.bestpos.soln'), 'PPP_CONVERGING');

// --- 2. heights: MSL, geoid separation and their sum ----------------------
console.log('\n2. heights from BESTPOSA');
run('render()');
check('altitude MSL', text('alt'), '31.9559 m');
check('geoid separation', text('geoid'), '10.3015 m');
check('ellipsoidal ht', text('hae'), '42.2574 m');
// set() writes textContent, so a unit marker written through it stays visible
// as the literal text "<u>m</u>". Every value with a unit must go through
// html(), which is what this checks.
check('unit rendered, not printed',
  els.get('bpHae').textContent, v => !v.includes('<u>'));
check('engine card ellipsoidal ht', text('bpHae'), '42.2574 m');

// --- 3. the fields the parser used to drop --------------------------------
// BESTPOSA carries diff_age; GGA carries the same number in its own field 13.
// With both present the engine's value wins, and the label says seconds.
console.log('\n3. correction age and satellites used in the solution');
feed(GGA_FIX);
run('render()');
check('corr age (BESTPOSA wins)', text('age'), '22.0 s');
check('corr age (banner)', text('fixAge'), '22.0 s');
check('sats used', text('sats'), '19');
check('sats in banner', text('fixSats'), '19');
// The same fields from GGA alone.
reset();
feed(GGA_SINGLE);
run('render()');
check('corr age (GGA only)', text('age'), '–');   // this GGA has no age field
check('sats used (GGA only)', text('sats'), '16');

// --- 4. no solution: zeros are not a position ----------------------------
console.log('\n4. BESTPOSA with Pos type NONE shows dashes, not zeros');
feed(BESTPOS_NONE);
run('render()');
check('pos type', text('fixQuality'), 'NONE');
check('altitude', text('alt'), '–');
check('geoid separation', text('geoid'), '–');
check('ellipsoidal ht', text('hae'), '–');
check('corr age', text('age'), '–');
check('sats', text('sats'), '0');

// --- 5. GGA alone still drives everything -------------------------------
console.log('\n5. GGA fallback when BESTPOSA is absent');
reset();
feed(GGA_SINGLE);
feed(GST);
run('render()');
check('fix quality', text('fixQuality'), 'SINGLE');
check('engine card', els.get('bestposCard').hidden, true);
check('altitude MSL', text('alt'), '4.7351 m');
check('geoid separation', text('geoid'), '13.724 m');
check('ellipsoidal ht', text('hae'), '18.4591 m');
check('sigma latitude', text('sdLat'), '1.131 m');

// --- 6. a stale engine verdict must not survive -------------------------
console.log('\n6. BESTPOSA ages out after 5 s');
feed(BESTPOS_INT);
run('render()');
check('before', text('fixQuality'), 'RTK FIXED');
run('S.bestposMs = performance.now() - 6000; render()');
check('after 6 s', text('fixQuality'), 'SINGLE');
check('engine card hidden', els.get('bestposCard').hidden, true);

// --- 7. malformed input is ignored, not rendered ------------------------
console.log('\n7. short BESTPOSA and a bad checksum');
check('parseBestpos(short)', run("parseBestpos('#BESTPOSA,COM1,0,60.0;SOL_COMPUTED,NONE,1,2,3')"), null);
run('S.bad = 0');
feed('$GPGGA,155506.00,4515.6961628,N,03807.8390353,E,1,16,0.8,4.7351,M,13.724,M,,*00');
check('bad checksum counter', run('S.bad'), 1);

// --- 8. every element the render path writes to exists in the markup ----
console.log('\n8. no render() target is missing from the HTML');
const missing = [...els.keys()].filter(id => !html.includes(`id="${id}"`));
check('unknown element ids', missing.length ? missing.join(',') : 'none', 'none');

console.log(`\n${checks - failures}/${checks} checks passed`);
process.exit(failures ? 1 : 0);
