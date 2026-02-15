import { fileURLToPath, URL } from 'node:url'

import { defineConfig, type Plugin } from 'vite'
import tailwindcss from '@tailwindcss/vite'
import vue from '@vitejs/plugin-vue'
import vueDevTools from 'vite-plugin-vue-devtools'

function patchAffinoMenuPointerRelatedTarget(): Plugin {
  const marker = 'const i = c.relatedTarget instanceof HTMLElement ? c.relatedTarget : null;'
  return {
    name: 'patch-affino-menu-pointer-related-target',
    enforce: 'pre',
    transform(code, id) {
      if (!id.includes('/@affino/menu-vue/dist/index.js')) return null
      if (!code.includes(marker)) return null
      if (code.includes('elementFromPoint')) return null

      const patched = code.replace(
        marker,
        `let i = c.relatedTarget instanceof HTMLElement ? c.relatedTarget : null;
    if (!i && typeof document < "u" && typeof c.clientX == "number" && typeof c.clientY == "number") {
      const l = document.elementFromPoint;
      if (typeof l == "function")
        try {
          const v = l.call(document, c.clientX, c.clientY);
          i = v instanceof HTMLElement ? v : null;
        } catch {
          i = null;
        }
    }`,
      )

      if (patched === code) return null
      return { code: patched, map: null }
    },
  }
}

// https://vite.dev/config/
export default defineConfig({
  server: {
    host: true,
    port: 5173,
    strictPort: true,
    proxy: {
      '/api': {
        target: 'http://localhost:8000',
        changeOrigin: true,
        ws: true,
      },
      '/ws': {
        target: 'ws://localhost:8000',
        ws: true,
      },
    },
  },
  optimizeDeps: {
    exclude: ['@affino/menu-vue'],
  },
  plugins: [
    patchAffinoMenuPointerRelatedTarget(),
    vue(),
    vueDevTools(),
    tailwindcss(),
  ],
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url))
    },
  },
})
