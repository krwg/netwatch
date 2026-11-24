import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

const espTarget = process.env.VITE_ESP_PROXY || 'http://netwatch.local'

export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    proxy: {
      '/api': {
        target: espTarget,
        changeOrigin: true,
      },
    },
  },
})
