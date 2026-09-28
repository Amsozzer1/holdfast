// Types for the Emscripten module built from wasm/bindings.cpp.
declare module '*/wasm/holdfast.js' {
  export interface SideFrame {
    config: string;
    idSwitches: number;
    recoveries: number;
    recoveriesCorrect: number;
    updateMs: number;
    /** stride 9: id x y w h state matched sigmaX sigmaY  (state: 0 tentative, 1 confirmed, 2 lost) */
    tracks: Float32Array;
  }
  export interface Frame {
    frame: number;
    t: number;
    delivered: boolean;
    stale: boolean;
    cmcFailed: boolean;
    /** world -> image, row-major 2x3 */
    view: Float32Array;
    /** previous delivered frame -> this frame, row-major 2x3 */
    motion: Float32Array;
    /** stride 7: x y w h id car occluded */
    truth: Float32Array;
    /** stride 5: x y w h score */
    dets: Float32Array;
    sides: SideFrame[];
  }
  export interface World {
    roads: Float32Array;
    buildings: Float32Array;
    occluders: Float32Array;
  }
  export class Playground {
    constructor(seed: number, left: string, right: string);
    step(): Frame;
    world(): World;
    setConfig(side: number, config: string): boolean;
    blackout(seconds: number): void;
    freeze(seconds: number): void;
    setDropout(p: number): void;
    setJitterMs(ms: number): void;
    setObjects(n: number): void;
    setFalsePositives(rate: number): void;
    setCameraMode(mode: number): void;
    setAutoBlackout(everySeconds: number): void;
    setAutoFreeze(everySeconds: number): void;
    delete(): void;
  }
  export interface HoldfastModule {
    Playground: typeof Playground;
  }
  const createHoldfast: () => Promise<HoldfastModule>;
  export default createHoldfast;
}
