import { realpathSync } from 'node:fs'
import { resolve } from 'node:path'
import { pathToFileURL } from 'node:url'
import { expect, it, vi } from 'vitest'
import { createSignalGridPatchQueue } from './useSignalGridPatchQueue'

// Resolve the actual core installed under the application's Vue adapter.
const appPath = realpathSync('node_modules/@affino/datagrid-vue-app')
const vuePath = realpathSync(resolve(appPath, '../datagrid-vue'))
const coreUrl = pathToFileURL(resolve(vuePath, '../datagrid-core/dist/src/index.js')).href

it('updates real Affino row data through the installed public API without resetting columns', async () => {
  const { createClientRowModel, createDataGridColumnModel, createDataGridApi } = await import(/* @vite-ignore */ coreUrl)
  const rowModel = createClientRowModel({
    rows: [{ rowId: 'signal-1', tested_at: null, test_status: '' }, { rowId: 'signal-2', tested_at: null }],
    resolveRowId: (row) => row.rowId,
  })
  const columnModel = createDataGridColumnModel({ columns: [{ key: 'tested_at', width: 240 }, { key: 'test_status', width: 180 }] })
  const api = createDataGridApi({ rowModel, columnModel, lifecycle: { state: 'started' }, init: async () => {}, start: async () => {}, stop: async () => {}, dispose: async () => {} })
  const columnsBefore = columnModel.getSnapshot()
  const setRows = vi.spyOn(rowModel, 'setRows')
  const queue = createSignalGridPatchQueue({ value: { getApi: () => api } })
  queue.enqueueRowPatches([{ rowId: 'signal-1', changes: { tested_at: '2026-09-15T22:00:01Z', test_status: 'verified' }, columns: ['tested_at', 'test_status'] }])
  queue.flush({ immediate: true })
  expect(rowModel.getRow(0).data).toMatchObject({ tested_at: '2026-09-15T22:00:01Z', test_status: 'verified' })
  expect(rowModel.getRow(1).data.tested_at).toBeNull()
  expect(columnModel.getSnapshot()).toEqual(columnsBefore)
  expect(setRows).not.toHaveBeenCalled()
  expect(queue.diagnostics().droppedRowPatches).toBe(0)
})
