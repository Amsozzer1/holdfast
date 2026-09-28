import type { Frame, SideFrame, World } from './wasm/holdfast.js';

export const IMAGE_W = 1280;
export const IMAGE_H = 720;

export interface ViewOptions {
  showTruth: boolean;
  showDetections: boolean;
  showTrails: boolean;
}

/** Per-panel trail history in image coordinates, warped by the camera motion each frame. */
export type Trails = Map<number, number[]>;

const TRAIL_POINTS = 18;

export function idColor(id: number): string {
  return `hsl(${(id * 47) % 360} 85% 62%)`;
}

export function updateTrails(trails: Trails, frame: Frame, side: SideFrame): void {
  if (!frame.delivered || frame.stale) return;
  const m = frame.motion;
  const identity = m[0] === 1 && m[1] === 0 && m[2] === 0 && m[3] === 0 && m[4] === 1 && m[5] === 0;
  if (!identity) {
    for (const pts of trails.values()) {
      for (let i = 0; i < pts.length; i += 2) {
        const x = pts[i], y = pts[i + 1];
        pts[i] = m[0] * x + m[1] * y + m[2];
        pts[i + 1] = m[3] * x + m[4] * y + m[5];
      }
    }
  }
  const live = new Set<number>();
  const t = side.tracks;
  for (let i = 0; i < t.length; i += 9) {
    const id = t[i], state = t[i + 5], matched = t[i + 6];
    if (state === 0) continue;
    live.add(id);
    if (matched !== 1) continue;
    const pts = trails.get(id) ?? [];
    pts.push(t[i + 1] + t[i + 3] / 2, t[i + 2] + t[i + 4] / 2);
    if (pts.length > TRAIL_POINTS * 2) pts.splice(0, 2);
    trails.set(id, pts);
  }
  for (const id of trails.keys()) if (!live.has(id)) trails.delete(id);
}

function worldRects(ctx: CanvasRenderingContext2D, r: Float32Array, pad = 0, radius = 0): void {
  for (let i = 0; i < r.length; i += 4) {
    ctx.beginPath();
    ctx.roundRect(r[i] - pad, r[i + 1] - pad, r[i + 2] + 2 * pad, r[i + 3] + 2 * pad, radius);
    ctx.fill();
  }
}

function drawGround(ctx: CanvasRenderingContext2D, frame: Frame, world: World): void {
  const v = frame.view;
  ctx.save();
  ctx.transform(v[0], v[3], v[1], v[4], v[2], v[5]);
  ctx.fillStyle = '#39402f';
  ctx.fillRect(-2000, -2000, 8200, 7000);
  // Pavements, then asphalt, then lane markings.
  ctx.fillStyle = '#5b5d58';
  worldRects(ctx, world.roads, 10);
  ctx.fillStyle = '#2c2e31';
  worldRects(ctx, world.roads);
  ctx.strokeStyle = 'rgba(235, 228, 200, 0.55)';
  ctx.lineWidth = 2;
  ctx.setLineDash([18, 22]);
  const r = world.roads;
  for (let i = 0; i < r.length; i += 4) {
    ctx.beginPath();
    if (r[i + 2] > r[i + 3]) {
      ctx.moveTo(r[i], r[i + 1] + r[i + 3] / 2);
      ctx.lineTo(r[i] + r[i + 2], r[i + 1] + r[i + 3] / 2);
    } else {
      ctx.moveTo(r[i] + r[i + 2] / 2, r[i + 1]);
      ctx.lineTo(r[i] + r[i + 2] / 2, r[i + 1] + r[i + 3]);
    }
    ctx.stroke();
  }
  ctx.setLineDash([]);
  // Rooftops.
  const b = world.buildings;
  for (let i = 0; i < b.length; i += 4) {
    ctx.fillStyle = '#6b6f73';
    ctx.fillRect(b[i], b[i + 1], b[i + 2], b[i + 3]);
    ctx.fillStyle = '#7d8186';
    ctx.fillRect(b[i] + 10, b[i + 1] + 10, b[i + 2] - 20, b[i + 3] - 20);
  }
  ctx.restore();
}

function drawOccluders(ctx: CanvasRenderingContext2D, frame: Frame, world: World): void {
  const v = frame.view;
  ctx.save();
  ctx.transform(v[0], v[3], v[1], v[4], v[2], v[5]);
  const o = world.occluders;
  for (let i = 0; i < o.length; i += 4) {
    // A canopy: a few overlapping discs filling the rectangle.
    const x = o[i], y = o[i + 1], w = o[i + 2], h = o[i + 3];
    const r = Math.min(w, h) * 0.42;
    ctx.fillStyle = '#284a31';
    for (const [fx, fy] of [[0.3, 0.3], [0.7, 0.32], [0.35, 0.7], [0.68, 0.7], [0.5, 0.5]]) {
      ctx.beginPath();
      ctx.arc(x + fx * w, y + fy * h, r, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.fillStyle = 'rgba(80, 130, 80, 0.55)';
    ctx.beginPath();
    ctx.arc(x + 0.45 * w, y + 0.42 * h, r * 0.7, 0, Math.PI * 2);
    ctx.fill();
  }
  ctx.restore();
}

function drawObjects(ctx: CanvasRenderingContext2D, frame: Frame): void {
  const t = frame.truth;
  for (let i = 0; i < t.length; i += 7) {
    const x = t[i], y = t[i + 1], w = t[i + 2], h = t[i + 3], car = t[i + 5] === 1;
    if (car) {
      ctx.fillStyle = '#d7dbe0';
      ctx.beginPath();
      ctx.roundRect(x, y, w, h, 5);
      ctx.fill();
      ctx.fillStyle = '#9aa3ad';
      ctx.beginPath();
      ctx.roundRect(x + w * 0.22, y + h * 0.22, w * 0.56, h * 0.56, 3);
      ctx.fill();
    } else {
      ctx.fillStyle = '#f2c98a';
      ctx.beginPath();
      ctx.arc(x + w / 2, y + h / 2, Math.max(3, w / 2.4), 0, Math.PI * 2);
      ctx.fill();
    }
  }
}

function drawTruthOutlines(ctx: CanvasRenderingContext2D, frame: Frame): void {
  const t = frame.truth;
  ctx.strokeStyle = 'rgba(255,255,255,0.55)';
  ctx.lineWidth = 1;
  ctx.setLineDash([3, 3]);
  for (let i = 0; i < t.length; i += 7) ctx.strokeRect(t[i], t[i + 1], t[i + 2], t[i + 3]);
  ctx.setLineDash([]);
}

function drawDetections(ctx: CanvasRenderingContext2D, frame: Frame): void {
  const d = frame.dets;
  for (let i = 0; i < d.length; i += 5) {
    const cx = d[i] + d[i + 2] / 2, cy = d[i + 1] + d[i + 3] / 2, s = d[i + 4];
    ctx.strokeStyle = s >= 0.5 ? 'rgba(255,255,255,0.9)' : 'rgba(255,255,255,0.4)';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(cx - 7, cy); ctx.lineTo(cx + 7, cy);
    ctx.moveTo(cx, cy - 7); ctx.lineTo(cx, cy + 7);
    ctx.stroke();
  }
}

function drawTracks(ctx: CanvasRenderingContext2D, side: SideFrame, trails: Trails, opts: ViewOptions): void {
  if (opts.showTrails) {
    ctx.lineWidth = 2.5;
    for (const [id, pts] of trails) {
      if (pts.length < 4) continue;
      ctx.strokeStyle = idColor(id);
      ctx.globalAlpha = 0.7;
      ctx.beginPath();
      ctx.moveTo(pts[0], pts[1]);
      for (let i = 2; i < pts.length; i += 2) ctx.lineTo(pts[i], pts[i + 1]);
      ctx.stroke();
    }
    ctx.globalAlpha = 1;
  }
  const t = side.tracks;
  ctx.font = '700 17px ui-monospace, SFMono-Regular, Menlo, monospace';
  ctx.textBaseline = 'bottom';
  for (let i = 0; i < t.length; i += 9) {
    const id = t[i], x = t[i + 1], y = t[i + 2], w = t[i + 3], h = t[i + 4];
    const state = t[i + 5], matched = t[i + 6] === 1, sx = t[i + 7], sy = t[i + 8];
    if (state === 0) continue;
    const c = idColor(id);
    ctx.strokeStyle = c;
    if (matched && state === 1) {
      ctx.lineWidth = 3;
      ctx.strokeRect(x, y, w, h);
    } else {
      // Coasting: dashed box plus the 2-sigma position uncertainty, which grows while unseen.
      ctx.lineWidth = 2;
      ctx.setLineDash([7, 5]);
      ctx.strokeRect(x, y, w, h);
      ctx.beginPath();
      ctx.ellipse(x + w / 2, y + h / 2, Math.max(2, sx), Math.max(2, sy), 0, 0, Math.PI * 2);
      ctx.stroke();
      ctx.setLineDash([]);
    }
    const label = String(id);
    const tw = ctx.measureText(label).width + 8;
    ctx.fillStyle = c;
    ctx.fillRect(x, y - 20, tw, 20);
    ctx.fillStyle = '#111';
    ctx.fillText(label, x + 4, y - 2);
  }
}

function banner(ctx: CanvasRenderingContext2D, text: string, color: string): void {
  ctx.font = '700 30px system-ui, sans-serif';
  const w = ctx.measureText(text).width + 36;
  const x = (IMAGE_W - w) / 2, y = IMAGE_H / 2 - 32;
  ctx.fillStyle = color;
  ctx.fillRect(x, y, w, 64);
  ctx.fillStyle = '#fff';
  ctx.textBaseline = 'middle';
  ctx.fillText(text, x + 18, y + 33);
}

export function drawPanel(
  canvas: HTMLCanvasElement, frame: Frame, side: SideFrame, world: World, trails: Trails, opts: ViewOptions,
): void {
  const ctx = canvas.getContext('2d');
  if (!ctx) return;
  const s = canvas.width / IMAGE_W;
  ctx.setTransform(s, 0, 0, s, 0, 0);
  ctx.save();
  ctx.beginPath();
  ctx.rect(0, 0, IMAGE_W, IMAGE_H);
  ctx.clip();

  if (!frame.delivered) {
    ctx.fillStyle = '#0c0d0f';
    ctx.fillRect(0, 0, IMAGE_W, IMAGE_H);
    drawTracks(ctx, side, trails, opts);
    banner(ctx, 'NO VIDEO — frames lost', '#b3261e');
  } else {
    drawGround(ctx, frame, world);
    drawObjects(ctx, frame);
    drawOccluders(ctx, frame, world);
    if (opts.showTruth) drawTruthOutlines(ctx, frame);
    if (opts.showDetections) drawDetections(ctx, frame);
    drawTracks(ctx, side, trails, opts);
    if (frame.stale) banner(ctx, 'FROZEN FEED — stale frame', '#b86e00');
  }
  ctx.restore();
}
