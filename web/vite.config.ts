import { defineConfig, type Plugin } from 'vite';
import react from '@vitejs/plugin-react';

// Strict Content-Security-Policy for the production page. Not used by the dev server, whose
// hot-reload preamble is an inline script. 'wasm-unsafe-eval' allows compiling the tracker's
// WebAssembly and nothing else; the Emscripten build uses no eval / new Function.
const CSP = [
  "default-src 'self'",
  "script-src 'self' 'wasm-unsafe-eval'",
  "style-src 'self' 'unsafe-inline'",
  "img-src 'self' data:",
  "connect-src 'self' data:",
  "base-uri 'none'",
  "form-action 'none'",
  "object-src 'none'",
].join('; ');

const csp = (): Plugin => ({
  name: 'holdfast-csp',
  apply: 'build',
  transformIndexHtml: (html) =>
    html.replace('<meta charset="UTF-8" />', `<meta charset="UTF-8" />\n    <meta http-equiv="Content-Security-Policy" content="${CSP}" />`),
});

// Served from https://<user>.github.io/holdfast/
export default defineConfig({
  base: '/holdfast/',
  plugins: [react(), csp()],
  build: { target: 'es2022' },
});
