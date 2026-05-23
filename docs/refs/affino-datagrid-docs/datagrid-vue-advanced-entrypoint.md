# DataGrid Vue Advanced Entrypoint

Updated: `2026-05-20`

This document defines the explicit advanced namespace for power-user hooks in `@affino/datagrid-vue`.

## Entrypoint

- `@affino/datagrid-vue/advanced`
- `@affino/datagrid-vue/advanced/layout`
- `@affino/datagrid-vue/advanced/pointer`
- `@affino/datagrid-vue/advanced/selection`
- `@affino/datagrid-vue/advanced/editing`
- `@affino/datagrid-vue/advanced/clipboard`
- `@affino/datagrid-vue/advanced/filtering`
- `@affino/datagrid-vue/advanced/history`

`@affino/datagrid-vue/advanced` is the canonical aggregate barrel. Prefer domain entrypoints for stricter import boundaries.

## Advanced Surface

- `createDataGridViewportController`
- `useDataGridCellPointerDownRouter`
- `useDataGridCellPointerHoverRouter`
- `useDataGridDragSelectionLifecycle`
- `useDataGridDragPointerSelection`
- `useDataGridFillSelectionLifecycle`
- `useDataGridFillHandleStart`
- `useDataGridRangeMoveLifecycle`
- `useDataGridRangeMoveStart`
- `useDataGridSelectionMoveHandle`
- `useDataGridTabTargetResolver`
- `useDataGridCellNavigation`
- `useDataGridClipboardValuePolicy`
- `useDataGridCellDatasetResolver`
- `useDataGridCellRangeHelpers`
- `useDataGridNavigationPrimitives`
- `useDataGridMutationSnapshot`
- `useDataGridCellVisualStatePredicates`
- `useDataGridRangeMutationEngine`
- `useDataGridA11yCellIds`
- `useDataGridColumnUiPolicy`
- `useDataGridEditableValuePolicy`
- `useDataGridMoveMutationPolicy`
- `useDataGridInlineEditorSchema`
- `useDataGridInlineEditOrchestration`
- `useDataGridInlineEditorTargetNavigation`
- `useDataGridInlineEditorKeyRouter`
- `useDataGridHeaderContextActions`
- `useDataGridHeaderLayerOrchestration`
- `useDataGridCopyRangeHelpers`
- `useDataGridHeaderSortOrchestration`
- `useDataGridHeaderResizeOrchestration`
- `useDataGridHeaderInteractionRouter`
- `useDataGridColumnFilterOrchestration`
- `useDataGridEnumTrigger`
- `useDataGridGroupValueLabelResolver`
- `useDataGridGroupMetaOrchestration`
- `useDataGridGroupBadge`
- `useDataGridGroupingSortOrchestration`
- `useDataGridViewportMeasureScheduler`
- `useDataGridVisibleRowsSyncScheduler`
- `useDataGridColumnLayoutOrchestration`
- `useDataGridSelectionOverlayOrchestration`
- `useDataGridRowsProjection`
- `useDataGridRowSelectionOrchestration`
- `useDataGridRowSelectionInputHandlers`
- `useDataGridVirtualRangeMetrics`
- `useDataGridContextMenuAnchor`
- `useDataGridContextMenuActionRouter`
- `useDataGridViewportContextMenuRouter`
- `useDataGridViewportBlurHandler`
- `useDataGridViewportScrollLifecycle`
- `useDataGridLinkedPaneScrollSync`
- `useDataGridResizeClickGuard`
- `useDataGridInitialViewportRecovery`
- `useDataGridManagedWheelScroll`
- `useDataGridManagedTouchScroll`
- `useDataGridScrollIdleGate`
- `useDataGridScrollPerfTelemetry`
- `useDataGridClearSelectionLifecycle`
- `useDataGridGlobalPointerLifecycle`
- `useDataGridPointerAutoScroll`
- `useDataGridPointerPreviewRouter`
- `useDataGridPointerCellCoordResolver`
- `useDataGridAxisAutoScrollDelta`
- `useDataGridCellVisibilityScroller`
- `useDataGridGlobalMouseDownContextMenuCloser`
- `useDataGridKeyboardCommandRouter`
- `useDataGridQuickFilterActions`
- `useDataGridCellCoordNormalizer`
- `useDataGridSelectionComparators`
- `useDataGridRowSelectionModel`
- `useDataGridPointerModifierPolicy`
- `useDataGridHistoryActionRunner`
- `useDataGridInlineEditorFocus`
- `useDataGridClipboardBridge`
- `useDataGridClipboardMutations`
- `useDataGridIntentHistory`

## Boundary Rule

- Root/stable entrypoint (`@affino/datagrid-vue`, `@affino/datagrid-vue/stable`) must not export low-level pointer, editing, header, viewport, scroll, keyboard, clipboard mutation, or lifecycle orchestration hooks.
- `useDataGridSelectionOverlayOrchestration`, `createGrid`, and `useAffinoGrid` are intentionally also present on root/stable as stable integration primitives. They remain available from advanced/domain entrypoints for existing power-user imports.
- Advanced entrypoint is for integrators who intentionally opt into lower-level orchestration APIs.

## Perf Note

- `useDataGridHeaderResizeOrchestration` supports `resizeApplyMode`:
  - `raf` (default): coalesces pointer resize updates to one width apply per animation frame.
  - `sync`: applies width immediately on every pointer update (useful for deterministic tests).
- `useDataGridGlobalPointerLifecycle` supports `pointerPreviewApplyMode`:
  - `sync` (default): immediate preview updates (deterministic tests / strict behavior checks).
  - `raf`: coalesces range/fill/drag preview updates to one apply per animation frame for smoother pointer sessions.

## Contract Guards

- Advanced/composable contract coverage lives in:
  - `packages/datagrid-vue/src/composables/__tests__/*.contract.spec.ts`
- Run package contract suite:
  - `pnpm --filter @affino/datagrid-vue run test:contracts`
