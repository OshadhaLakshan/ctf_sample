import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// Forward local API and WebSocket traffic to the C++ process.
export default defineConfig({
  plugins: [react()],
  server: { proxy: { '/api': { target: `http://127.0.0.1:${process.env.ROWDOGG_PORT || '8080'}`, ws: true } } },
});
