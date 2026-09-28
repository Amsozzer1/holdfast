import { useCallback, useEffect, useRef, useState } from 'react';
import createHoldfast, { type Frame, type Playground, type World } from './wasm/holdfast.js';
import { drawPanel, IMAGE_H, IMAGE_W, updateTrails, type Trails, type ViewOptions } from './draw';

const CONFIGS: { value: string; label: string }[] = [
  { value: 'baseline', label: 'Baseline (ByteTrack-style, frame-index)' },
  { value: 'cmc', label: 'Baseline + camera-motion comp.' },
  { value: 'full', label: 'Holdfast (full)' },
  { value: 'full+cmc=0', label: 'Holdfast − camera-motion comp.' },
  { value: 'full+recovery=0', label: 'Holdfast − recovery stage' },
  { value: 'full+stale=0', label: 'Holdfast − stale-frame guard' },
  { value: 'full+vdt=0', label: 'Holdfast − timestamp dt' },
  { value: 'full+oc=0', label: 'Holdfast − OC re-update' },
];

const CAMERA = ['Static', 'Slow pan', 'Drone'] as const;
const FPS = 30;

interface SideStats { idSwitches: number; recoveries: number; recoveriesCorrect: number; tracks: number }
interface Settings {
  dropout: number; jitterMs: number; objects: number; falsePositives: number; camera: number; chaos: boolean;
}
const DEFAULTS: Settings = { dropout: 0.1, jitterMs: 0, objects: 45, falsePositives: 0.6, camera: 2, chaos: false };

// Initial state from the URL, e.g. ?left=cmc&right=full&camera=2&chaos=1&seed=7&t=20
function fromUrl() {
  const q = new URLSearchParams(window.location.search);
  const num = (k: string, d: number, lo: number, hi: number) => {
    const v = Number(q.get(k));
    return q.has(k) && Number.isFinite(v) ? Math.min(hi, Math.max(lo, v)) : d;
  };
  const cfg = (k: string, d: string) => {
    const v = q.get(k);
    return v && CONFIGS.some((c) => c.value === v) ? v : d;
  };
  return {
    configs: [cfg('left', 'baseline'), cfg('right', 'full')] as [string, string],
    settings: {
      dropout: num('dropout', DEFAULTS.dropout, 0, 0.5),
      jitterMs: num('jitter', DEFAULTS.jitterMs, 0, 20),
      objects: Math.round(num('objects', DEFAULTS.objects, 10, 90)),
      falsePositives: num('fp', DEFAULTS.falsePositives, 0, 4),
      camera: Math.round(num('camera', DEFAULTS.camera, 0, 2)),
      chaos: q.get('chaos') === '1',
    },
    seed: Math.round(num('seed', 42, 0, 1e9)),
    skipSeconds: num('t', 0, 0, 600),
    paused: q.get('paused') === '1',
  };
}
const INITIAL = fromUrl();

function applySettings(pg: Playground, s: Settings): void {
  pg.setDropout(s.dropout);
  pg.setJitterMs(s.jitterMs);
  pg.setObjects(s.objects);
  pg.setFalsePositives(s.falsePositives);
  pg.setCameraMode(s.camera);
  pg.setAutoBlackout(s.chaos ? 4 : 0);
  pg.setAutoFreeze(s.chaos ? 6 : 0);
}

function confirmedCount(tracks: Float32Array): number {
  let n = 0;
  for (let i = 5; i < tracks.length; i += 9) if (tracks[i] !== 0) n++;
  return n;
}

export default function App() {
  const [error, setError] = useState<string | null>(null);
  const [ready, setReady] = useState(false);
  const [configs, setConfigs] = useState<[string, string]>(INITIAL.configs);
  const [settings, setSettings] = useState<Settings>(INITIAL.settings);
  const [view, setView] = useState<ViewOptions>({ showTruth: false, showDetections: false, showTrails: true });
  const [paused, setPaused] = useState(INITIAL.paused);
  const [speed, setSpeed] = useState(1);
  const [seed, setSeed] = useState(INITIAL.seed);
  const [stepMs, setStepMs] = useState(0);
  const stepTimeRef = useRef({ total: 0, n: 0 });
  const [stats, setStats] = useState<SideStats[]>([]);
  const [clock, setClock] = useState({ t: 0, delivered: true, stale: false });

  const moduleRef = useRef<Awaited<ReturnType<typeof createHoldfast>> | null>(null);
  const pgRef = useRef<Playground | null>(null);
  const worldRef = useRef<World | null>(null);
  const frameRef = useRef<Frame | null>(null);
  const trailsRef = useRef<Trails[]>([new Map(), new Map()]);
  const canvasRefs = [useRef<HTMLCanvasElement>(null), useRef<HTMLCanvasElement>(null)];
  const viewRef = useRef(view);
  viewRef.current = view;
  const settingsRef = useRef(settings);
  settingsRef.current = settings;
  const configsRef = useRef(configs);
  configsRef.current = configs;
  const pausedRef = useRef(paused);
  pausedRef.current = paused;
  const speedRef = useRef(speed);
  speedRef.current = speed;

  // Load the WebAssembly module once.
  useEffect(() => {
    createHoldfast().then((m) => { moduleRef.current = m; setReady(true); })
      .catch((e: unknown) => setError(`Could not load the WebAssembly module: ${String(e)}`));
  }, []);

  // (Re)create the playground when the module is ready or the seed changes.
  useEffect(() => {
    const m = moduleRef.current;
    if (!ready || !m) return;
    const pg = new m.Playground(seed, configsRef.current[0], configsRef.current[1]);
    applySettings(pg, settingsRef.current);
    pgRef.current = pg;
    worldRef.current = pg.world();
    trailsRef.current = [new Map(), new Map()];
    frameRef.current = pg.step();
    // Fast-forward (?t=seconds) only for the first scene.
    const skip = seed === INITIAL.seed ? Math.round(INITIAL.skipSeconds * FPS) : 0;
    for (let i = 0; i < skip; i++) {
      const f = pg.step();
      f.sides.forEach((s, k) => updateTrails(trailsRef.current[k], f, s));
      frameRef.current = f;
    }
    return () => { pg.delete(); pgRef.current = null; };
  }, [ready, seed]);

  useEffect(() => { if (pgRef.current) applySettings(pgRef.current, settings); }, [settings]);

  const setConfig = (side: 0 | 1, value: string) => {
    pgRef.current?.setConfig(side, value);
    trailsRef.current[side] = new Map();
    setConfigs((c) => (side === 0 ? [value, c[1]] : [c[0], value]));
  };

  const advance = useCallback((steps: number) => {
    const pg = pgRef.current;
    if (!pg) return;
    for (let i = 0; i < steps; i++) {
      const t0 = performance.now();
      const f = pg.step();
      stepTimeRef.current.total += performance.now() - t0;
      stepTimeRef.current.n += 1;
      f.sides.forEach((s, k) => updateTrails(trailsRef.current[k], f, s));
      frameRef.current = f;
    }
  }, []);

  // Fixed-rate simulation (30 frames per simulated second), drawn on every animation frame.
  useEffect(() => {
    let raf = 0, last = performance.now(), acc = 0, lastStats = -Infinity;
    const loop = (now: number) => {
      const dt = Math.min(0.1, (now - last) / 1000);
      last = now;
      if (!pausedRef.current) {
        acc += dt * speedRef.current * FPS;
        const n = Math.floor(acc);
        acc -= n;
        if (n > 0) advance(Math.min(n, 6));
      }
      const f = frameRef.current, world = worldRef.current;
      if (f && world) {
        f.sides.forEach((s, k) => {
          const c = canvasRefs[k].current;
          if (c) drawPanel(c, f, s, world, trailsRef.current[k], viewRef.current);
        });
        if (now - lastStats > 200) {
          lastStats = now;
          setStats(f.sides.map((s) => ({ idSwitches: s.idSwitches, recoveries: s.recoveries,
            recoveriesCorrect: s.recoveriesCorrect, tracks: confirmedCount(s.tracks) })));
          setClock({ t: f.t, delivered: f.delivered, stale: f.stale });
          const st = stepTimeRef.current;
          if (st.n >= 30) { setStepMs(st.total / st.n); stepTimeRef.current = { total: 0, n: 0 }; }
        }
      }
      raf = requestAnimationFrame(loop);
    };
    raf = requestAnimationFrame(loop);
    return () => cancelAnimationFrame(raf);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [advance]);

  // Size canvases to their box at device resolution.
  useEffect(() => {
    const ro = new ResizeObserver(() => {
      for (const r of canvasRefs) {
        const c = r.current;
        if (!c) continue;
        const w = Math.round(c.clientWidth * Math.min(2, window.devicePixelRatio || 1));
        c.width = w;
        c.height = Math.round((w * IMAGE_H) / IMAGE_W);
      }
    });
    canvasRefs.forEach((r) => r.current && ro.observe(r.current));
    return () => ro.disconnect();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [ready]);

  // Keyboard: B blackout, F freeze, space pause, S step.
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.target instanceof HTMLInputElement || e.target instanceof HTMLSelectElement) return;
      if (e.key === 'b' || e.key === 'B') pgRef.current?.blackout(1);
      else if (e.key === 'f' || e.key === 'F') pgRef.current?.freeze(0.5);
      else if (e.key === ' ') { e.preventDefault(); setPaused((p) => !p); }
      else if (e.key === 's' || e.key === 'S') advance(1);
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [advance]);

  const set = <K extends keyof Settings>(k: K, v: Settings[K]) => setSettings((s) => ({ ...s, [k]: v }));

  return (
    <div className="page">
      <header>
        <h1>Holdfast playground</h1>
        <p className="lede">
          The Holdfast C++ tracker, compiled to WebAssembly and running in your browser. Two trackers get the
          identical broken feed. Cut the video, freeze it, shake the camera, and watch who keeps their IDs.
        </p>
        <a className="repo" href="https://github.com/Amsozzer1/holdfast" rel="noreferrer">Source, benchmarks and design notes on GitHub →</a>
      </header>

      {error && <p className="error">{error}</p>}
      {!ready && !error && <p className="loading">Loading the tracker…</p>}

      <section className="panels" aria-label="Side-by-side trackers">
        {[0, 1].map((k) => {
          const s = stats[k];
          const other = stats[1 - k];
          const better = s && other && s.idSwitches < other.idSwitches;
          return (
            <figure key={k} className="panel">
              <div className="panel-head">
                <select value={configs[k]} onChange={(e) => setConfig(k as 0 | 1, e.target.value)} aria-label={`Tracker ${k + 1}`}>
                  {CONFIGS.map((c) => <option key={c.value} value={c.value}>{c.label}</option>)}
                </select>
              </div>
              <canvas ref={canvasRefs[k]} width={IMAGE_W} height={IMAGE_H} />
              <figcaption className="stats">
                <span className={better ? 'good' : undefined}><b>{s?.idSwitches ?? 0}</b> ID switches</span>
                <span><b>{s?.recoveriesCorrect ?? 0}</b>/{s?.recoveries ?? 0} re-acquired correctly</span>
                <span><b>{s?.tracks ?? 0}</b> tracks</span>
              </figcaption>
            </figure>
          );
        })}
      </section>

      <section className="controls" aria-label="Controls">
        <div className="group">
          <h2>Break the feed</h2>
          <div className="row">
            <button className="primary" onClick={() => pgRef.current?.blackout(1)} title="B">Cut video 1 s</button>
            <button className="primary" onClick={() => pgRef.current?.freeze(0.5)} title="F">Freeze 0.5 s</button>
            <label className="check"><input type="checkbox" checked={settings.chaos} onChange={(e) => set('chaos', e.target.checked)} /> Chaos mode</label>
          </div>
          <Slider label="Detection dropout" value={settings.dropout} min={0} max={0.5} step={0.05} fmt={(v) => `${Math.round(v * 100)}%`} onChange={(v) => set('dropout', v)} />
          <Slider label="Timestamp jitter" value={settings.jitterMs} min={0} max={20} step={1} fmt={(v) => `${v} ms`} onChange={(v) => set('jitterMs', v)} />
          <Slider label="False positives" value={settings.falsePositives} min={0} max={4} step={0.2} fmt={(v) => `${v.toFixed(1)}/frame`} onChange={(v) => set('falsePositives', v)} />
        </div>
        <div className="group">
          <h2>Scene</h2>
          <div className="row segmented" role="radiogroup" aria-label="Camera motion">
            {CAMERA.map((c, i) => (
              <button key={c} role="radio" aria-checked={settings.camera === i} className={settings.camera === i ? 'on' : undefined} onClick={() => set('camera', i)}>{c}</button>
            ))}
          </div>
          <Slider label="Objects in view" value={settings.objects} min={10} max={90} step={5} fmt={(v) => String(v)} onChange={(v) => set('objects', v)} />
          <div className="row">
            <label className="check"><input type="checkbox" checked={view.showTrails} onChange={(e) => setView({ ...view, showTrails: e.target.checked })} /> Trails</label>
            <label className="check"><input type="checkbox" checked={view.showDetections} onChange={(e) => setView({ ...view, showDetections: e.target.checked })} /> Detections</label>
            <label className="check"><input type="checkbox" checked={view.showTruth} onChange={(e) => setView({ ...view, showTruth: e.target.checked })} /> Ground truth</label>
          </div>
        </div>
        <div className="group">
          <h2>Playback</h2>
          <div className="row">
            <button onClick={() => setPaused((p) => !p)} title="Space">{paused ? 'Play' : 'Pause'}</button>
            <button onClick={() => advance(1)} disabled={!paused} title="S">Step</button>
            <button onClick={() => setSeed((s) => s + 1)}>New scene</button>
          </div>
          <div className="row segmented" role="radiogroup" aria-label="Speed">
            {[0.25, 0.5, 1, 2].map((v) => (
              <button key={v} role="radio" aria-checked={speed === v} className={speed === v ? 'on' : undefined} onClick={() => setSpeed(v)}>{v}×</button>
            ))}
          </div>
          <p className="clock">
            t = {clock.t.toFixed(1)} s · seed {seed} · <span title="One simulated frame: the scene, the detector and both trackers, in WebAssembly">{stepMs.toFixed(2)} ms/frame</span>
            {!clock.delivered && <span className="tag red">no video</span>}
            {clock.stale && <span className="tag amber">frozen</span>}
          </p>
          <p className="keys">Keys: <kbd>B</kbd> cut · <kbd>F</kbd> freeze · <kbd>Space</kbd> pause · <kbd>S</kbd> step</p>
        </div>
      </section>

      <section className="notes">
        <div>
          <h2>Reading the picture</h2>
          <ul>
            <li><b>Solid box</b>: track matched to a detection this frame.</li>
            <li><b>Dashed box + ellipse</b>: coasting on the Kalman prediction; the ellipse is the 2σ position uncertainty and grows while the object is unseen.</li>
            <li>During a cut, the baseline's boxes stay put (it counts frames, so it thinks no time passed). Holdfast uses timestamps, so its predictions keep moving.</li>
            <li>Trees hide what is under them: detections there are mostly missed, which is where re-acquisition matters.</li>
          </ul>
        </div>
        <div>
          <h2>What is real and what is simulated</h2>
          <ul>
            <li><b>Real</b>: the tracker (Kalman filter, Hungarian matching, ByteTrack stages, recovery, OC-SORT re-update, stale-frame guard, camera-motion compensation) is the same C++ as the repository, compiled with Emscripten.</li>
            <li><b>Simulated</b>: the scene and the detector (ground truth plus noise, as in the benchmark). The camera-motion estimate is the true motion plus noise; in the real pipeline it comes from optical flow and RANSAC.</li>
            <li>ID switches here are a live estimate. The benchmark numbers on real drone video (VisDrone) are in the <a href="https://github.com/Amsozzer1/holdfast#results" rel="noreferrer">README</a>.</li>
          </ul>
        </div>
      </section>
    </div>
  );
}

function Slider(props: { label: string; value: number; min: number; max: number; step: number; fmt: (v: number) => string; onChange: (v: number) => void }) {
  const id = `s-${props.label.replace(/\s+/g, '-').toLowerCase()}`;
  return (
    <div className="slider">
      <label htmlFor={id}>{props.label}</label>
      <input id={id} type="range" min={props.min} max={props.max} step={props.step} value={props.value}
        onChange={(e) => props.onChange(Number(e.target.value))} />
      <output htmlFor={id}>{props.fmt(props.value)}</output>
    </div>
  );
}
