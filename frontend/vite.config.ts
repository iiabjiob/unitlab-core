import { fileURLToPath, URL } from 'node:url'

import { defineConfig, type Plugin } from 'vite'
import tailwindcss from '@tailwindcss/vite'
import vue from '@vitejs/plugin-vue'
import vueDevTools from 'vite-plugin-vue-devtools'

const affinoDataGridChunkRules: Array<[chunkName: string, packagePath: string]> = [
  ['vendor-affino-datagrid-app', '/node_modules/@affino/datagrid-vue-app/'],
  ['vendor-affino-datagrid-vue', '/node_modules/@affino/datagrid-vue/'],
  ['vendor-affino-datagrid-chrome', '/node_modules/@affino/datagrid-chrome/'],
  ['vendor-affino-datagrid-theme', '/node_modules/@affino/datagrid-theme/'],
  ['vendor-affino-datagrid-gantt', '/node_modules/@affino/datagrid-gantt/'],
]

function resolveAffinoChunkName(id: string): string | undefined {
  const affinoPackageMatch = id.match(/\/node_modules\/@affino\/([^/]+)\//)
  const affinoPackageName = affinoPackageMatch?.[1]

  if (!affinoPackageName) {
    return undefined
  }

  if (affinoPackageName.startsWith('datagrid-')) {
    return undefined
  }

  if (affinoPackageName.startsWith('menu-')) {
    return 'vendor-affino-menu'
  }

  return 'vendor-affino-ui'
}

function resolveAffinoDataGridCoreChunkName(id: string): string | undefined {
  const match = id.match(/\/node_modules\/@affino\/datagrid-core\/dist\/src\/([^/]+)(?:\/([^/]+))?\//)
  if (!match) {
    return undefined
  }

  const primarySegment = match[1]
  const secondarySegment = primarySegment === 'models' ? match[2] : undefined

  if (primarySegment === 'cells') {
    return 'vendor-affino-datagrid-core-cells'
  }

  if (primarySegment === 'core') {
    return 'vendor-affino-datagrid-core-core'
  }

  if (primarySegment === 'models') {
    const allowedModelSegments = new Set([
      'aggregation',
      'bootstrap',
      'compute',
      'filters',
      'host',
      'materialization',
      'mutation',
      'pivot',
      'projection',
      'snapshot',
      'state',
      'tree',
    ])

    if (secondarySegment && allowedModelSegments.has(secondarySegment)) {
      return `vendor-affino-datagrid-core-models-${secondarySegment}`
    }

    return 'vendor-affino-datagrid-core-models'
  }

  return undefined
}

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
export default defineConfig(({ command }) => ({
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
        timeout: 30000,
        proxyTimeout: 30000,
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
    command === 'serve' ? vueDevTools() : null,
    tailwindcss(),
  ].filter((plugin): plugin is Plugin => plugin !== null),
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

          const affinoDataGridCoreChunkName = resolveAffinoDataGridCoreChunkName(id)
          if (affinoDataGridCoreChunkName) {
            return affinoDataGridCoreChunkName
          }

          if (id.includes('/node_modules/vue/') || id.includes('/node_modules/vue-router/') || id.includes('/node_modules/pinia/')) {
            return 'framework-vue'
          }

          if (id.includes('/node_modules/xlsx/')) {
            return 'vendor-xlsx'
          }

          const affinoChunkName = resolveAffinoChunkName(id)
          if (affinoChunkName) {
            return affinoChunkName
          }

          if (id.includes('/node_modules/axios/')) {
            return 'vendor-axios'
          }

          if (id.includes('/node_modules/pinia-plugin-persistedstate/')) {
            return 'vendor-pinia-plugin-persistedstate'
          }

          return 'vendor-misc'
        },
      },
    },
  },
}))
