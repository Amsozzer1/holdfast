// Headless check of the WebAssembly module: run a minute of simulated degraded video through
// both trackers and assert the outputs are sane. Run with: node web/scripts/smoke.mjs
import createHoldfast from '../src/wasm/holdfast.js';

const M = await createHoldfast();
const pg = new M.Playground(42, 'baseline', 'full');
pg.setAutoBlackout(3);
pg.setAutoFreeze(4);
pg.setDropout(0.1);

let f, delivered = 0, stale = 0, maxMs = 0;
const t0 = performance.now();
for (let i = 0; i < 1800; i++) {
  f = pg.step();
  if (f.delivered) delivered++;
  if (f.stale) stale++;
  for (const s of f.sides) maxMs = Math.max(maxMs, s.updateMs);
}
const wall = performance.now() - t0;
const [base, full] = f.sides;
console.log(`frames 1800 delivered ${delivered} stale ${stale} | wall ${wall.toFixed(0)} ms (${(1800 / wall * 1000).toFixed(0)} steps/s)`);
console.log(`baseline IDSW ${base.idSwitches}  full IDSW ${full.idSwitches}  | truth ${f.truth.length / 7} dets ${f.dets.length / 5} tracks ${full.tracks.length / 9}`);
const w = pg.world();
const ok = delivered > 1000 && delivered < 1800 && stale > 0 && full.idSwitches < base.idSwitches
  && f.truth.length % 7 === 0 && full.tracks.length % 9 === 0 && w.roads.length > 0
  && pg.setConfig(0, 'cmc') && !pg.setConfig(0, 'nonsense');
pg.delete();
if (!ok) { console.error('SMOKE TEST FAILED'); process.exit(1); }
console.log('smoke ok');
