import { fileURLToPath, URL } from 'node:url'

import { defineConfig } from 'vite'
import tailwindcss from '@tailwindcss/vite'
import vue from '@vitejs/plugin-vue'
import vueDevTools from 'vite-plugin-vue-devtools'

// https://vite.dev/config/
export default defineConfig({
  plugins: [
    vue(),
    vueDevTools(),
    tailwindcss(),
  ],
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url))
    },
  },
  build: {
    rollupOptions: {
      output: {
        manualChunks(id) {
          if (id.includes('ag-grid-community') || id.includes('ag-grid-vue3')) {
            return 'ag-grid';
          }
          if (id.includes('node_modules')) {
            // например, выделить все зависимости в отдельный чанк "vendor"
            return 'vendor';
          }
        }
      }
    }
  }
})
