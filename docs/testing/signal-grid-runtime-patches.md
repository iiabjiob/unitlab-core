# Signal grid runtime patches

The Signals page sends runtime updates through the RAF-batched patch queue to
Affino 0.6's public `api.rows.patch` and `api.view.refreshCellsByRowKeys`.
`patchRows` is an internal method name, not a member of the public rows namespace.
The adapter derives these method types from `DataGridInstance` to catch API drift.

Direct control publishes the persisted `markTested` response timestamp through
`testedAtRealtimeStore`. Automated test events use the same store and page watcher.
Do not synthesize browser timestamps or replace verification results with `tested`.
Do not restore saved views or reload the dataset for runtime cell updates.

Validation:
- `signalGridInstalledApi.test.js` uses the core actually installed beneath the Vue
  adapter; it checks changed row data, unchanged neighboring rows/columns, and no
  `setRows` calls. It does not verify browser rendering or DOM scroll position.
- `signalSheetStore.test.ts` checks delivery of persisted direct-control timestamps.
- In the browser, scroll horizontally, resize a column, then run manual and automated
  controls. Check both result cells without F5 and preserve selection/scroll/widths.
