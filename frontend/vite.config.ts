import { fileURLToPath, URL } from 'node:url'

import { defineConfig, type Plugin } from 'vite'
import vue from '@vitejs/plugin-vue'
import vueDevTools from 'vite-plugin-vue-devtools'

const affinoDataGridChunkRules: Array<[chunkName: string, packagePath: string]> = [
  ['vendor-affino-datagrid-gantt-stage', '/node_modules/@affino/datagrid-vue-app/dist/chunks/DataGridGanttStageEntry-'],
  ['vendor-affino-datagrid-row-model', '/node_modules/@affino/datagrid-vue-app/dist/chunks/useDataGridAppRowModel-'],
  ['vendor-affino-datagrid-app', '/node_modules/@affino/datagrid-vue-app/'],
  ['vendor-affino-datagrid-vue', '/node_modules/@affino/datagrid-vue/'],
  ['vendor-affino-datagrid-core', '/node_modules/@affino/datagrid-core/'],
  ['vendor-affino-datagrid-orchestration', '/node_modules/@affino/datagrid-orchestration/'],
  ['vendor-affino-datagrid-formula', '/node_modules/@affino/datagrid-formula-engine/'],
  ['vendor-affino-datagrid-pivot', '/node_modules/@affino/datagrid-pivot/'],
  ['vendor-affino-datagrid-worker', '/node_modules/@affino/datagrid-worker/'],
  ['vendor-affino-datagrid-server', '/node_modules/@affino/datagrid-server-adapters/'],
  ['vendor-affino-datagrid-server', '/node_modules/@affino/datagrid-server-client/'],
  ['vendor-affino-datagrid-format', '/node_modules/@affino/datagrid-format/'],
  ['vendor-affino-datagrid-chrome', '/node_modules/@affino/datagrid-chrome/'],
  ['vendor-affino-datagrid-theme', '/node_modules/@affino/datagrid-theme/'],
  ['vendor-affino-datagrid-gantt', '/node_modules/@affino/datagrid-gantt/'],
]

const affinoUiChunkRules: Array<[chunkName: string, packagePath: string]> = [
  ['vendor-affino-dialog', '/node_modules/@affino/dialog-'],
  ['vendor-affino-disclosure', '/node_modules/@affino/disclosure-'],
  ['vendor-affino-listbox', '/node_modules/@affino/listbox-'],
  ['vendor-affino-popover', '/node_modules/@affino/popover-'],
  ['vendor-affino-tabs', '/node_modules/@affino/tabs-'],
  ['vendor-affino-tooltip', '/node_modules/@affino/tooltip-'],
  ['vendor-affino-treeview', '/node_modules/@affino/treeview-'],
  ['vendor-affino-overlay', '/node_modules/@affino/overlay-'],
  ['vendor-affino-surface', '/node_modules/@affino/surface-'],
  ['vendor-affino-selection', '/node_modules/@affino/selection-'],
  ['vendor-affino-focus', '/node_modules/@affino/focus-utils/'],
]

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
        headers: {
          'accept-encoding': 'identity',
        },
        timeout: 600000,
        proxyTimeout: 600000,
      },
      '/ws': {
        target: 'ws://localhost:8000',
        ws: true,
      },
    },
  },
  optimizeDeps: {
    exclude: ['@affino/menu-vue', '@affino/treeview-vue'],
  },
  plugins: [
    patchAffinoMenuPointerRelatedTarget(),
    vue(),
    vueDevTools(),
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
          if (!id.includes('/node_modules/')) {
            return undefined
          }

          for (const [chunkName, packagePath] of affinoDataGridChunkRules) {
            if (id.includes(packagePath)) {
              return chunkName
            }
          }

          if (id.includes('/node_modules/vue/') || id.includes('/node_modules/vue-router/') || id.includes('/node_modules/pinia/')) {
            return 'framework-vue'
          }

          if (id.includes('/node_modules/xlsx/')) {
            return 'vendor-xlsx'
          }

          if (id.includes('/node_modules/@affino/menu-')) {
            return 'vendor-affino-menu'
          }

          for (const [chunkName, packagePath] of affinoUiChunkRules) {
            if (id.includes(packagePath)) {
              return chunkName
            }
          }

          if (id.includes('/node_modules/@affino/')) {
            return 'vendor-affino-ui'
          }

          return 'vendor-misc'
        },
      },
    },
  },
})
