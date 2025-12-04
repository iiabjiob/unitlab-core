import {
	createWritableSignal,
	type CreateWritableSignal,
	type WritableSignal,
} from "../runtime/signals"

import { createFrameScheduler, type FrameSchedulerHooks } from "../runtime/frameScheduler"
import { createRafScheduler } from "../runtime/rafScheduler"
import { flushMeasurements } from "../runtime/measurementQueue"
import type { TableViewportResizeObserver } from "./viewportHostEnvironment"

import type { UiTableColumn, VisibleRow } from "../types"
import { BASE_ROW_HEIGHT, clamp } from "../utils/constants"
import {
	COLUMN_VIRTUALIZATION_BUFFER,
	resolveColumnWidth as resolveColumnWidthDefault,
} from "../dom/gridUtils"
import { createHorizontalOverscanController } from "../virtualization/dynamicOverscan"
import {
	updateColumnSnapshot,
	createEmptyColumnSnapshot,
} from "../virtualization/columnSnapshot"
import { clampScrollOffset, computeHorizontalScrollLimit } from "../virtualization/scrollLimits"
import { createAxisVirtualizer } from "../virtualization/axisVirtualizer"
import {
	createHorizontalAxisStrategy,
	type HorizontalVirtualizerPayload,
} from "../virtualization/horizontalVirtualizer"
import {
	createTableViewportSignals,
	type TableViewportSignals,
} from "./tableViewportSignals"
import { createTableViewportDiagnostics } from "./tableViewportDiagnostics"
import {
	createTableViewportScrollIo,
	type TableViewportScrollStateAdapter,
} from "./tableViewportScrollIo"
import { createTableViewportVirtualization } from "./tableViewportVirtualization"
import {
	buildHorizontalMeta,
	type TableViewportHorizontalMeta,
} from "./tableViewportHorizontalMeta"
import {
	applyHorizontalViewport,
	prepareHorizontalViewport,
	type HorizontalUpdateCallbacks,
	type HorizontalUpdatePrepared,
} from "./tableViewportHorizontalUpdate"
import type {
	ViewportMetricsSnapshot,
	LayoutMeasurementSnapshot,
	TableViewportImperativeCallbacks,
	TableViewportServerIntegration,
	TableViewportControllerOptions,
	ViewportSyncTargets,
} from "./tableViewportTypes"
import { applyViewportSyncTransforms, resetViewportSyncTransforms } from "./scrollSync"
import type { ViewportSyncState } from "./tableViewportTypes"
import type { ColumnPinMode } from "../virtualization/types"

export type {
	ViewportMetricsSnapshot,
	LayoutMeasurementSnapshot,
	ImperativeColumnUpdatePayload,
	ImperativeRowUpdatePayload,
	ImperativeScrollSyncPayload,
	TableViewportImperativeCallbacks,
	TableViewportServerIntegration,
	TableViewportControllerOptions,
	TableViewportRuntimeOverrides,
		ViewportSyncTargets,
} from "./tableViewportTypes"
import {
	createDefaultHostEnvironment,
	createMonotonicClock,
	type ViewportClock,
} from "./tableViewportConfig"
import {
	createLayoutMeasurementCache,
	type LayoutMeasurementCache,
} from "./tableViewportLayoutCache"
import {
	FRAME_BUDGET_CONSTANTS,
	HORIZONTAL_VIRTUALIZATION_CONSTANTS,
	VERTICAL_VIRTUALIZATION_CONSTANTS,
} from "./tableViewportConstants"
import { sampleBoundingRect, sampleContainerMetrics, sampleHeaderHeight } from "./tableViewportEnvironment"
export type { TableViewportState, RowPoolItem } from "./tableViewportSignals"


export interface TableViewportController extends TableViewportSignals {
	attach(container: HTMLDivElement | null, header: HTMLElement | null): void
	detach(): void
	setProcessedRows(rows: VisibleRow[]): void
	setColumns(columns: UiTableColumn[]): void
	setZoom(zoom: number): void
	setVirtualizationEnabled(enabled: boolean): void
	setHorizontalVirtualizationEnabled(enabled: boolean): void
	setRowHeightMode(mode: "fixed" | "auto"): void
	setBaseRowHeight(height: number): void
	setViewportMetrics(metrics: ViewportMetricsSnapshot | null): void
	setIsLoading(loading: boolean): void
	setImperativeCallbacks(callbacks: TableViewportImperativeCallbacks | null | undefined): void
	setOnAfterScroll(callback: (() => void) | null | undefined): void
	setOnNearBottom(callback: (() => void) | null | undefined): void
	setServerIntegration(integration: TableViewportServerIntegration | null | undefined): void
	setDebugMode(enabled: boolean): void
	handleScroll(event: Event): void
	updateViewportHeight(): void
	measureRowHeight(): void
	cancelScrollRaf(): void
	scrollToRow(index: number): void
	scrollToColumn(key: string): void
	isRowVisible(index: number): boolean
		clampScrollTopValue(value: number): number
		setViewportSyncTargets(targets: ViewportSyncTargets | null): void
	refresh(force?: boolean): void
	dispose(): void
}


export function createTableViewportController(
	options: TableViewportControllerOptions,
): TableViewportController {
	const resolveColumnWidth = options.resolveColumnWidth ?? resolveColumnWidthDefault
	const getColumnKey = options.getColumnKey ?? ((column: UiTableColumn) => column.key)

	const fallbackResolvePinMode = (column: UiTableColumn): ColumnPinMode => {
		if (column.sticky === "left" || column.isSystem) {
			return "left"
		}
		if (column.sticky === "right") {
			return "right"
		}

		const raw = column as unknown as Record<string, unknown>
		const pinned = raw?.pinned
		const pin = raw?.pin
		const lock = raw?.lock
		const locked = raw?.locked

		if (pinned === true || pinned === "left" || pin === "left" || lock === "left" || locked === true) {
			return "left"
		}
		if (pinned === "right" || pin === "right" || lock === "right") {
			return "right"
		}
		if (column.stickyLeft) {
			return "left"
		}
		if (column.stickyRight) {
			return "right"
		}
		return "none"
	}

	const resolvePinMode = (column: UiTableColumn): ColumnPinMode => {
		const userResolved = options.resolvePinMode ? options.resolvePinMode(column) : "none"
		if (userResolved === "left" || userResolved === "right") {
			return userResolved
		}
		return fallbackResolvePinMode(column)
	}

	const hostEnvironment = options.hostEnvironment ?? createDefaultHostEnvironment()
		const clock: ViewportClock = options.clock ?? createMonotonicClock()
		const frameBudget = options.frameBudget ?? FRAME_BUDGET_CONSTANTS
		const verticalVirtualization = options.verticalVirtualization ?? VERTICAL_VIRTUALIZATION_CONSTANTS
		const horizontalVirtualization = options.horizontalVirtualization ?? HORIZONTAL_VIRTUALIZATION_CONSTANTS
		const verticalScrollEpsilon = verticalVirtualization.scrollEpsilon
		const horizontalScrollEpsilon = horizontalVirtualization.scrollEpsilon
		const horizontalMinOverscan = Math.max(0, horizontalVirtualization.minOverscan)
	const runtimeOverrides = options.runtime ?? {}
	const scheduler =
		runtimeOverrides.rafScheduler ??
		(typeof runtimeOverrides.createRafScheduler === "function"
			? runtimeOverrides.createRafScheduler()
			: createRafScheduler())
	const ownsScheduler = !runtimeOverrides.rafScheduler
	const createFrameSchedulerFn = runtimeOverrides.createFrameScheduler ?? createFrameScheduler
	const flushMeasurementQueue = runtimeOverrides.measurementQueue?.flush ?? flushMeasurements

		const createSignal = <T,>(initial: T): WritableSignal<T> => {
			const factory = options.createSignal as CreateWritableSignal<T> | undefined
			if (factory) {
				return factory(initial)
			}
			return createWritableSignal(initial)
		}

		const signals = createTableViewportSignals(createSignal)
		const { input, core, derived, dispose: disposeSignals } = signals
		const {
			scrollTop,
			scrollLeft,
			viewportHeight,
			viewportWidth,
			virtualizationEnabled,
		} = input
		const {
			totalRowCount,
			effectiveRowHeight,
			totalContentHeight,
			startIndex,
			endIndex,
		} = core
		const {
			columns: {
				visibleColumns,
				visibleColumnEntries,
				visibleScrollableColumns,
				visibleScrollableEntries,
				pinnedLeftColumns,
				pinnedLeftEntries,
				pinnedRightColumns,
				pinnedRightEntries,
				leftPadding,
				rightPadding,
				columnWidthMap,
				visibleStartCol,
				visibleEndCol,
				scrollableRange,
				columnVirtualState,
			},
			metrics: {
				debugMode,
				fps,
				frameTime,
				droppedFrames,
				layoutReads,
				layoutWrites,
				syncScrollRate,
				heavyUpdateRate,
				virtualizerUpdates,
				virtualizerSkips,
			},
		} = derived

		const frameScheduler = createFrameSchedulerFn({
			onBeforeFrame: () => {
				if (!heavyFramePending && !heavyFrameInProgress) {
					return
				}
				frameForce = pendingForce
				pendingForce = false
			},
			onRead: () => {
				if (!heavyFramePending && !heavyFrameInProgress) {
					return
				}
				measureLayout()
			},
			onCommit: () => {
				if (!heavyFramePending && !heavyFrameInProgress) {
					frameForce = false
					return
				}
				runUpdate(frameForce)
				frameForce = false
			},
		} as FrameSchedulerHooks)

		const diagnostics = createTableViewportDiagnostics({
			scheduler,
			clock,
			signals: {
				debugMode,
				fps,
				frameTime,
				droppedFrames,
				layoutReads,
				layoutWrites,
				syncScrollRate,
				heavyUpdateRate,
				virtualizerUpdates,
				virtualizerSkips,
			},
		})

		const {
			recordLayoutRead,
			recordLayoutWrite,
			recordSyncScroll,
			recordHeavyPass,
			recordVirtualizerUpdate,
			recordVirtualizerSkip,
			setDebugMode: applyDebugMode,
			dispose: disposeDiagnostics,
		} = diagnostics

		const virtualization = createTableViewportVirtualization({
			signals,
			diagnostics,
			clock,
			frameBudget,
			verticalConfig: verticalVirtualization,
		})

		const horizontalVirtualizer = createAxisVirtualizer(
			"horizontal",
			createHorizontalAxisStrategy<UiTableColumn>(),
			{
				visibleStart: 0,
				visibleEnd: 0,
				leftPadding: 0,
				rightPadding: 0,
				totalScrollableWidth: 0,
				visibleScrollableWidth: 0,
				averageWidth: 0,
				scrollSpeed: 0,
				effectiveViewport: 0,
			} satisfies HorizontalVirtualizerPayload,
		)

		const horizontalOverscanController = createHorizontalOverscanController({
			minOverscan: horizontalMinOverscan,
			velocityRatio: horizontalVirtualization.velocityOverscanRatio,
			viewportRatio: horizontalVirtualization.viewportOverscanRatio,
			decay: horizontalVirtualization.overscanDecay,
			maxViewportMultiplier: horizontalVirtualization.maxViewportMultiplier,
			teleportMultiplier: frameBudget.teleportMultiplier,
			frameDurationMs: frameBudget.frameDurationMs,
			minSampleMs: frameBudget.minVelocitySampleMs,
		})

		const columnSnapshot = createEmptyColumnSnapshot<UiTableColumn>()
		// Cache layout metrics from observers so the heavy path never touches the DOM.
		const layoutCache: LayoutMeasurementCache = createLayoutMeasurementCache()
		let container: HTMLDivElement | null = null
		let header: HTMLElement | null = null
		let processedRows: VisibleRow[] = []
		let columns: UiTableColumn[] = []
		let virtualizationFlag = true
		let horizontalVirtualizationFlag = true
		let rowHeightMode: "fixed" | "auto" = "fixed"
		let baseRowHeight = BASE_ROW_HEIGHT
		let viewportMetrics: ViewportMetricsSnapshot | null = null
		let loading = false
		let serverIntegration: TableViewportServerIntegration = options.serverIntegration ?? {
			rowModel: null,
			enabled: false,
		}
		let imperativeCallbacks: TableViewportImperativeCallbacks = options.imperativeCallbacks ?? {}
		let onAfterScroll = options.onAfterScroll ?? null
		let onNearBottom = options.onNearBottom ?? null

		let pendingScrollTop: number | null = null
		let pendingScrollLeft: number | null = null
		let afterScrollTaskId: number | null = null
		let pendingForce = false
		let frameForce = false
		let heavyFramePending = false
		let heavyFrameInProgress = false
		let heavyUpdateTaskId: number | null = null
		let pendingHorizontalSettle = false
		let horizontalOverscan = horizontalMinOverscan
		let resizeObserver: TableViewportResizeObserver | null = null
		let attached = false
		let lastScrollTopSample = 0
		let lastScrollLeftSample = 0
		let lastAppliedScrollTop = 0
		let driftCorrectionPending = false
		let lastHorizontalSampleTime = 0
		let smoothedHorizontalVelocity = 0
		let horizontalMetaVersion = 0
		let lastHorizontalMetaSignature = ""
		let lastAppliedHorizontalMetaVersion = -1
		let cachedContainerWidth = -1
		let cachedContainerHeight = -1
		let cachedHeaderHeight = -1
		let cachedNativeScrollHeight = -1
		let cachedNativeScrollWidth = -1
		let layoutMeasurement: LayoutMeasurementSnapshot | null = null
		let lastAppliedScrollLeft = 0
		let lastHeavyScrollTop = 0
		let lastHeavyScrollLeft = 0
		let lastAverageColumnWidth = 0
		let lastScrollDirection = 0
		let scrollSyncTargets: ViewportSyncTargets | null = null
		let latestViewportSyncTargets: ViewportSyncTargets | null = null
		const scrollSyncState: ViewportSyncState = {
			scrollLeft: 0,
			scrollTop: 0,
			pinnedOffsetLeft: 0,
			pinnedOffsetRight: 0,
		}

		function measureLayout() {
			if (!attached) {
				layoutMeasurement = null
				return
			}
			layoutMeasurement = layoutCache.snapshot()
		}

		function captureLayoutMetrics(label: "attach" | "resize" | "manual") {
			// All DOM reads stay confined to observer-driven phases.
			if (!container) {
				return
			}
			flushMeasurementQueue()
			const containerMetrics = sampleContainerMetrics(hostEnvironment, recordLayoutRead, container)
			const rect = sampleBoundingRect(hostEnvironment, recordLayoutRead, container)
			layoutCache.updateContainer(containerMetrics, rect)
			cachedContainerHeight = containerMetrics.clientHeight > 0 ? containerMetrics.clientHeight : cachedContainerHeight
			cachedContainerWidth = containerMetrics.clientWidth > 0 ? containerMetrics.clientWidth : cachedContainerWidth
			cachedNativeScrollHeight = containerMetrics.scrollHeight
			cachedNativeScrollWidth = containerMetrics.scrollWidth
			if (header) {
				const headerHeightValue = sampleHeaderHeight(hostEnvironment, recordLayoutRead, header)
				layoutCache.updateHeader({ height: headerHeightValue })
				if (headerHeightValue > 0) {
					cachedHeaderHeight = headerHeightValue
				}
			} else {
				layoutCache.updateHeader({ height: 0 })
				cachedHeaderHeight = 0
			}
			if (label !== "manual") {
				measureLayout()
			}
		}

		function cancelPendingHeavyUpdate() {
			if (heavyUpdateTaskId === null) {
				return
			}
			scheduler.cancel(heavyUpdateTaskId)
			heavyUpdateTaskId = null
		}

		function requestHeavyFrame(force: boolean) {
			if (heavyFrameInProgress) {
				heavyFramePending = true
				frameScheduler.invalidate()
				return
			}
			if (!heavyFramePending) {
				heavyFramePending = true
				frameScheduler.invalidate()
				return
			}
			if (force) {
				frameScheduler.invalidate()
			}
		}

		function scheduleUpdate(force = false) {
			pendingForce = pendingForce || force

			if (force) {
				cancelPendingHeavyUpdate()
				requestHeavyFrame(true)
				return
			}

			if (heavyFrameInProgress) {
				requestHeavyFrame(false)
				return
			}

			if (heavyFramePending) {
				return
			}

			if (heavyUpdateTaskId !== null) {
				return
			}

			const taskId = scheduler.schedule(() => {
				heavyUpdateTaskId = null
				requestHeavyFrame(false)
			}, { priority: "normal" })

			if (taskId >= 0) {
				heavyUpdateTaskId = taskId
				return
			}

			requestHeavyFrame(false)
		}

		function flushSchedulers() {
			flushMeasurementQueue()
			frameScheduler.flush()
			scheduler.flush()
		}

		function updateCachedScrollOffsets(scrollTopValue: number, scrollLeftValue: number) {
			const metrics = {
				clientWidth: cachedContainerWidth >= 0 ? cachedContainerWidth : 0,
				clientHeight: cachedContainerHeight >= 0 ? cachedContainerHeight : 0,
				scrollWidth: cachedNativeScrollWidth >= 0 ? cachedNativeScrollWidth : 0,
				scrollHeight: cachedNativeScrollHeight >= 0 ? cachedNativeScrollHeight : 0,
				scrollTop: scrollTopValue,
				scrollLeft: scrollLeftValue,
			}
			layoutCache.updateContainer(metrics, null)
			lastScrollTopSample = scrollTopValue
			lastScrollLeftSample = scrollLeftValue
		}

		function emitImperativeScrollSync(scrollTopValue: number, scrollLeftValue: number, timestamp?: number) {
			if (typeof imperativeCallbacks.onScrollSync !== "function") {
				return
			}
			const resolvedTs = Number.isFinite(timestamp) ? (timestamp as number) : clock.now()
			imperativeCallbacks.onScrollSync({
				scrollTop: scrollTopValue,
				scrollLeft: scrollLeftValue,
				timestamp: resolvedTs,
			})
		}

		let scrollSyncTaskId: number | null = null

		const scrollState: TableViewportScrollStateAdapter = {
			getContainer: () => container,
			setContainer: value => {
				container = value
			},
			getHeader: () => header,
			setHeader: value => {
				header = value
			},
			getSyncTargets: () => scrollSyncTargets,
			setSyncTargets: value => {
				scrollSyncTargets = value
			},
			getSyncState: () => scrollSyncState,
			getLastAppliedScroll: () => ({ top: lastAppliedScrollTop, left: lastAppliedScrollLeft }),
			setLastAppliedScroll: (top, left) => {
				lastAppliedScrollTop = top
				lastAppliedScrollLeft = left
			},
			getLastHeavyScroll: () => ({ top: lastHeavyScrollTop, left: lastHeavyScrollLeft }),
			setLastHeavyScroll: (top, left) => {
				lastHeavyScrollTop = top
				lastHeavyScrollLeft = left
			},
			isAttached: () => attached,
			setAttached: value => {
				attached = value
			},
			getResizeObserver: () => resizeObserver,
			setResizeObserver: value => {
				resizeObserver = value
			},
			getPendingScrollTop: () => pendingScrollTop,
			setPendingScrollTop: value => {
				pendingScrollTop = value
			},
			getPendingScrollLeft: () => pendingScrollLeft,
			setPendingScrollLeft: value => {
				pendingScrollLeft = value
			},
			getAfterScrollTaskId: () => afterScrollTaskId,
			setAfterScrollTaskId: value => {
				afterScrollTaskId = value
			},
			getScrollSyncTaskId: () => scrollSyncTaskId,
			setScrollSyncTaskId: value => {
				scrollSyncTaskId = value
			},
			getLastScrollSamples: () => ({ top: lastScrollTopSample, left: lastScrollLeftSample }),
			setLastScrollSamples: (top, left) => {
				lastScrollTopSample = top
				lastScrollLeftSample = left
			},
			isPendingHorizontalSettle: () => pendingHorizontalSettle,
			setPendingHorizontalSettle: value => {
				pendingHorizontalSettle = value
			},
			isDriftCorrectionPending: () => driftCorrectionPending,
			setDriftCorrectionPending: value => {
				driftCorrectionPending = value
			},
			resetCachedMeasurements: () => {
				cachedContainerHeight = -1
				cachedContainerWidth = -1
				cachedHeaderHeight = -1
				cachedNativeScrollHeight = -1
				cachedNativeScrollWidth = -1
			},
			clearLayoutMeasurement: () => {
				layoutMeasurement = null
			},
			resetScrollSamples: () => {
				lastScrollTopSample = 0
				lastScrollLeftSample = 0
				lastAppliedScrollTop = 0
				lastAppliedScrollLeft = 0
				driftCorrectionPending = false
				scrollSyncState.scrollTop = 0
				scrollSyncState.scrollLeft = 0
				scrollSyncState.pinnedOffsetLeft = 0
				scrollSyncState.pinnedOffsetRight = 0
			},
		}

		function resolveViewportSyncNextState(overrides?: Partial<ViewportSyncState>): ViewportSyncState {
			return {
				scrollLeft: overrides?.scrollLeft ?? lastScrollLeftSample,
				scrollTop: overrides?.scrollTop ?? lastScrollTopSample,
				pinnedOffsetLeft: overrides?.pinnedOffsetLeft ?? scrollSyncState.pinnedOffsetLeft,
				pinnedOffsetRight: overrides?.pinnedOffsetRight ?? scrollSyncState.pinnedOffsetRight,
			}
		}

		function setViewportSyncTargetsValue(targets: ViewportSyncTargets | null) {
			latestViewportSyncTargets = targets
			if (scrollSyncTargets === targets) {
				if (targets) {
					applyViewportSyncTransforms(targets, scrollSyncState, resolveViewportSyncNextState())
				}
				scrollState.setSyncTargets(targets)
				return
			}
			scrollSyncTargets = targets
			scrollState.setSyncTargets(targets)
			if (!targets) {
				resetViewportSyncTransforms(null, scrollSyncState)
				return
			}
			ensureOverlayRootInsideContainer(targets)
			applyViewportSyncTransforms(targets, scrollSyncState, resolveViewportSyncNextState())
		}

		function ensureOverlayRootInsideContainer(targets: ViewportSyncTargets | null) {
			const host = targets?.scrollHost ?? container
			if (!host) {
				return
			}
			const findOverlay = (root: Element | null | undefined) => {
				if (!root || typeof root.querySelector !== "function") {
					return null
				}
				return root.querySelector<HTMLElement>(".ui-table__overlay-layer")
			}
			let overlayNode = targets?.overlayRoot ?? findOverlay(host)
			if (!overlayNode && targets?.layoutRoot) {
				overlayNode = findOverlay(targets.layoutRoot)
			}
			if (!overlayNode) {
				const fallbackRoot = host.parentElement ?? container?.parentElement ?? null
				overlayNode = findOverlay(fallbackRoot)
			}
			if (!overlayNode) {
				return
			}
			if (overlayNode.parentElement !== host && typeof host.appendChild === "function") {
				// Mount overlay inside the active scroll host so unified transforms stay in sync.
				host.appendChild(overlayNode)
			}
			if (targets && targets.overlayRoot !== overlayNode) {
				targets.overlayRoot = overlayNode
			}
		}

		const heavyIdleMs = Math.max(48, frameBudget.frameDurationMs * 4)
		function resolveGatedHeavyThresholds(): { vertical: number; horizontal: number } {
			const rowHeightCandidate = effectiveRowHeight.value && effectiveRowHeight.value > 0
				? effectiveRowHeight.value
				: baseRowHeight
			const vertical = Math.max(verticalScrollEpsilon, rowHeightCandidate * 0.5)
			const fallbackWidth = columnSnapshot.metrics.widths[0] ?? 32
			const widthReference = lastAverageColumnWidth > 0 ? lastAverageColumnWidth : fallbackWidth
			const horizontalBase = Math.max(horizontalScrollEpsilon, widthReference * 0.5)
			const horizontal = Math.min(Math.max(horizontalScrollEpsilon, horizontalBase), 96)
			return { vertical, horizontal }
		}
		const scrollIo = createTableViewportScrollIo({
			hostEnvironment,
			scheduler,
			recordLayoutRead,
			recordSyncScroll,
			queueHeavyUpdate: scheduleUpdate,
			flushSchedulers,
			getOnAfterScroll: () => onAfterScroll,
			state: scrollState,
			normalizeAndClampScroll: Boolean(options.normalizeAndClampScroll),
			clampScrollTop: clampScrollTopValue,
			clampScrollLeft: clampScrollLeftValue,
			frameDurationMs: frameBudget.frameDurationMs,
			resolveHeavyUpdateThresholds: resolveGatedHeavyThresholds,
			getTimestamp: () => clock.now(),
			maxHeavyIdleMs: heavyIdleMs,
			onResizeMetrics: () => captureLayoutMetrics("resize"),
			onScrollMetrics: ({ scrollTop, scrollLeft }) => {
				updateCachedScrollOffsets(scrollTop, scrollLeft)
			},
			onScrollSyncFrame: ({ scrollTop, scrollLeft }) => {
				emitImperativeScrollSync(scrollTop, scrollLeft)
			},
		})

			function attach(containerRef: HTMLDivElement | null, headerRef: HTMLElement | null) {
				scrollIo.attach(containerRef, headerRef)
				if (containerRef) {
					captureLayoutMetrics("attach")
					if (latestViewportSyncTargets) {
						setViewportSyncTargetsValue(latestViewportSyncTargets)
					} else {
						ensureOverlayRootInsideContainer(scrollSyncTargets)
					}
				}
			}

			function detach() {
				resetViewportSyncTransforms(scrollSyncTargets, scrollSyncState)
				scrollIo.detach()
				cancelScrollRaf()
				layoutCache.reset()
				layoutMeasurement = null
			}

		function setProcessedRowsValue(rows: VisibleRow[]) {
			processedRows = Array.isArray(rows) ? rows : []
			scheduleUpdate(true)
		}

		function setColumnsValue(next: UiTableColumn[]) {
			columns = Array.isArray(next) ? next : []
			scheduleUpdate(true)
		}

		function setZoomValue(_nextZoom: number) {
			// Zoom is fixed at 1 for the lite table; ignore external requests.
		}

		function setVirtualizationEnabledValue(enabled: boolean) {
			const normalized = Boolean(enabled)
			if (virtualizationFlag === normalized) return
			virtualizationFlag = normalized
			scheduleUpdate(true)
		}

		function setHorizontalVirtualizationEnabledValue(enabled: boolean) {
			const normalized = Boolean(enabled)
			if (horizontalVirtualizationFlag === normalized) return
			horizontalVirtualizationFlag = normalized
			horizontalOverscan = normalized ? Math.max(horizontalOverscan, horizontalMinOverscan) : 0
			scheduleUpdate(true)
		}

		function setRowHeightModeValue(mode: "fixed" | "auto") {
			if (rowHeightMode === mode) return
			rowHeightMode = mode
			scheduleUpdate(true)
		}

		function setBaseRowHeightValue(height: number) {
			const normalized = Number.isFinite(height) && height > 0 ? height : BASE_ROW_HEIGHT
			if (baseRowHeight === normalized) return
			baseRowHeight = normalized
			scheduleUpdate(true)
		}

		function setViewportMetricsValue(metrics: ViewportMetricsSnapshot | null) {
			viewportMetrics = metrics
			if (metrics) {
				const fallbackScrollWidth = cachedNativeScrollWidth >= 0 ? cachedNativeScrollWidth : metrics.containerWidth
				const fallbackScrollHeight = cachedNativeScrollHeight >= 0 ? cachedNativeScrollHeight : metrics.containerHeight
				layoutCache.updateContainer(
					{
						clientWidth: metrics.containerWidth,
						clientHeight: metrics.containerHeight,
						scrollWidth: fallbackScrollWidth,
						scrollHeight: fallbackScrollHeight,
						scrollLeft: lastScrollLeftSample,
						scrollTop: lastScrollTopSample,
					},
					null,
				)
				layoutCache.updateHeader({ height: metrics.headerHeight })
				measureLayout()
			}
			scheduleUpdate(true)
		}

		function setIsLoadingValue(value: boolean) {
			loading = Boolean(value)
		}

		function setImperativeCallbacksValue(callbacks: TableViewportImperativeCallbacks | null | undefined) {
			imperativeCallbacks = callbacks ?? {}
		}

		function setOnAfterScrollValue(callback: (() => void) | null | undefined) {
			onAfterScroll = callback ?? null
		}

		function setOnNearBottomValue(callback: (() => void) | null | undefined) {
			onNearBottom = callback ?? null
		}

		function setServerIntegrationValue(integration: TableViewportServerIntegration | null | undefined) {
			if (!integration) {
				serverIntegration = { rowModel: null, enabled: false }
			} else {
				serverIntegration = { ...integration }
			}

			virtualization.resetServerIntegration()
			scheduleUpdate(true)
		}

		function setDebugModeValue(enabled: boolean) {
			applyDebugMode(Boolean(enabled))
		}

		function handleScroll(event: Event) {
			scrollIo.handleScroll(event)
		}

		function updateViewportHeightValue() {
			scheduleUpdate(true)
		}

		function measureRowHeightValue() {
			scheduleUpdate(true)
		}

		function cancelScrollRaf() {
			frameScheduler.cancel()
			scrollIo.cancelAfterScrollTask()
			cancelPendingHeavyUpdate()
			pendingForce = false
			frameForce = false
			pendingHorizontalSettle = false
			horizontalOverscan = horizontalMinOverscan
			const timestamp = clock.now()
			horizontalOverscanController.reset(timestamp)
			virtualization.resetOverscan(timestamp)
			cachedNativeScrollHeight = -1
			cachedNativeScrollWidth = -1
		}

		function scrollToRowValue(index: number) {
			const total = totalRowCount.value
			if (total <= 0) return
			const clampedIndex = clamp(index, 0, Math.max(total - 1, 0))
			const rawTarget = clampedIndex * effectiveRowHeight.value
			const virtualizationActive = virtualizationEnabled.value || (virtualizationFlag && rowHeightMode === "fixed")
			let target: number
			if (virtualizationActive) {
				// Allow the final row to align with the top edge when virtualization synthesizes scroll height.
				const alignLimit = Math.max(0, (total - 1) * effectiveRowHeight.value)
				target = Math.min(rawTarget, alignLimit)
				if (!Number.isFinite(target)) {
					target = 0
				}
			} else {
				target = clampScrollTopValue(rawTarget)
			}
			pendingScrollTop = target
			scheduleUpdate(true)
			flushSchedulers()
		}

		function scrollToColumnValue(key: string) {
			const map = columnWidthMap.value
			if (!map.size) return
			let offset = 0
			for (const [columnKey, width] of map.entries()) {
				if (columnKey === key) break
				offset += width
			}
			pendingScrollLeft = Math.max(0, offset)
			scheduleUpdate(true)
		}

		function isRowVisibleValue(index: number): boolean {
			return index >= startIndex.value && index <= endIndex.value
		}

		function refreshValue(force?: boolean) {
			scheduleUpdate(force === true)
			if (force === true) {
				flushSchedulers()
			}
		}

		function disposeValue() {
			detach()
			flushMeasurementQueue()
			disposeDiagnostics()
			if (ownsScheduler) {
				scheduler.dispose()
			}
			frameScheduler.dispose()
			disposeSignals()
		}

		function scheduleAfterScroll() {
			scrollIo.scheduleAfterScroll()
		}

		function updatePinnedOffsets(meta: TableViewportHorizontalMeta) {
			const nextPinnedLeft = Math.max(0, Number.isFinite(meta.indexColumnWidth) ? meta.indexColumnWidth : 0)
			const nextPinnedRight = Math.max(0, Number.isFinite(meta.pinnedRightWidth) ? meta.pinnedRightWidth : 0)
			const pendingUpdate =
				scrollSyncState.pinnedOffsetLeft !== nextPinnedLeft ||
				scrollSyncState.pinnedOffsetRight !== nextPinnedRight

			if (!pendingUpdate) {
				return
			}

			if (scrollSyncTargets) {
				const nextState = resolveViewportSyncNextState({
					pinnedOffsetLeft: nextPinnedLeft,
					pinnedOffsetRight: nextPinnedRight,
				})
				applyViewportSyncTransforms(scrollSyncTargets, scrollSyncState, nextState)
			}

			scrollSyncState.pinnedOffsetLeft = nextPinnedLeft
			scrollSyncState.pinnedOffsetRight = nextPinnedRight
		}

		function applyColumnSnapshot(
			meta: TableViewportHorizontalMeta,
			start: number,
			end: number,
			payload: HorizontalVirtualizerPayload,
		) {
			columnSnapshot.columnWidthMap = columnWidthMap.value
			const { visibleStartIndex, visibleEndIndex } = updateColumnSnapshot({
				snapshot: columnSnapshot,
				meta: {
					scrollableColumns: meta.scrollableColumns,
					scrollableIndices: meta.scrollableIndices,
					metrics: meta.metrics,
					pinnedLeft: meta.pinnedLeft,
					pinnedRight: meta.pinnedRight,
					pinnedLeftWidth: meta.pinnedLeftWidth,
					pinnedRightWidth: meta.pinnedRightWidth,
					containerWidthForColumns: meta.containerWidthForColumns,
					indexColumnWidth: meta.indexColumnWidth,
					scrollDirection: meta.scrollDirection,
					zoom: meta.zoom,
				},
				range: { start, end },
				payload,
				getColumnKey,
				resolveColumnWidth,
			})

			const visibleColumnsSnapshot = columnSnapshot.visibleColumns
			visibleColumns.value = visibleColumnsSnapshot.map(entry => entry.column)
			visibleColumnEntries.value = visibleColumnsSnapshot.slice()

			const visibleScrollableSnapshot = columnSnapshot.visibleScrollable
			visibleScrollableColumns.value = visibleScrollableSnapshot.map(entry => entry.column)
			visibleScrollableEntries.value = visibleScrollableSnapshot.slice()

			const pinnedLeftSnapshot = columnSnapshot.pinnedLeft
			pinnedLeftColumns.value = pinnedLeftSnapshot.map(entry => entry.column)
			pinnedLeftEntries.value = pinnedLeftSnapshot.slice()

			const pinnedRightSnapshot = columnSnapshot.pinnedRight
			pinnedRightColumns.value = pinnedRightSnapshot.map(entry => entry.column)
			pinnedRightEntries.value = pinnedRightSnapshot.slice()

			leftPadding.value = payload.leftPadding
			rightPadding.value = payload.rightPadding
			columnWidthMap.value = new Map(columnSnapshot.columnWidthMap)

			visibleStartCol.value = visibleStartIndex
			visibleEndCol.value = visibleEndIndex
			scrollableRange.value = { start, end }

			const columnState = columnVirtualState.value
			columnState.start = start
			columnState.end = end
			columnState.visibleStart = payload.visibleStart
			columnState.visibleEnd = payload.visibleEnd
			columnState.overscanLeading = horizontalVirtualizer.getState().overscanLeading
			columnState.overscanTrailing = horizontalVirtualizer.getState().overscanTrailing
			columnState.poolSize = horizontalVirtualizer.getState().poolSize
			columnState.visibleCount = horizontalVirtualizer.getState().visibleCount
			columnState.totalCount = meta.scrollableColumns.length
			columnState.indexColumnWidth = meta.indexColumnWidth
			columnState.pinnedRightWidth = meta.pinnedRightWidth
			columnVirtualState.value = { ...columnState }

			updatePinnedOffsets(meta)
		}

		function clampScrollTopValue(value: number) {
			return virtualization.clampScrollTop(value)
		}

		function clampScrollLeftValue(value: number) {
			if (!Number.isFinite(value)) return 0
			const containerRef = container
			const metrics = columnSnapshot.metrics
			if (!metrics.widths.length) {
				lastAppliedScrollLeft = 0
				return 0
			}

			const averageWidth = metrics.totalWidth / Math.max(metrics.widths.length, 1)
			const bufferPx = COLUMN_VIRTUALIZATION_BUFFER * averageWidth
			const effectiveViewport = Math.max(
				0,
				columnSnapshot.containerWidthForColumns - columnSnapshot.pinnedLeftWidth - columnSnapshot.pinnedRightWidth,
			)
			const trailingGap = Math.max(0, effectiveViewport - columnSnapshot.visibleScrollableWidth)
			const nativeLimit = containerRef
				? Math.max(0, containerRef.scrollWidth - containerRef.clientWidth)
				: cachedNativeScrollWidth >= 0 && cachedContainerWidth >= 0
					? Math.max(0, cachedNativeScrollWidth - cachedContainerWidth)
					: null
			const resolvedNativeLimit = nativeLimit != null && Number.isFinite(nativeLimit) && nativeLimit > 0 ? nativeLimit : 0

			const maxScroll = virtualizationEnabled.value
				? computeHorizontalScrollLimit({
						totalScrollableWidth: columnSnapshot.totalScrollableWidth,
						viewportWidth: columnSnapshot.containerWidthForColumns,
						pinnedLeftWidth: columnSnapshot.pinnedLeftWidth,
						pinnedRightWidth: columnSnapshot.pinnedRightWidth,
						bufferPx,
						trailingGap,
						nativeScrollLimit: nativeLimit,
						tolerancePx: averageWidth + 1,
					})
				: resolvedNativeLimit

			if (!Number.isFinite(maxScroll) || maxScroll <= 0) {
				lastAppliedScrollLeft = 0
				return 0
			}

			const normalized = clampScrollOffset({ offset: value, limit: maxScroll })
			const previousApplied = lastAppliedScrollLeft
			const direction = value > previousApplied ? 1 : value < previousApplied ? -1 : lastScrollDirection

			lastScrollDirection = direction

			lastAppliedScrollLeft = normalized
			return normalized
		}

			function runUpdate(force: boolean) {
				if (!heavyFramePending && !heavyFrameInProgress && !force && !pendingForce) {
					return
				}
				if (heavyFrameInProgress) {
					pendingForce = pendingForce || force
					heavyFramePending = true
					frameScheduler.invalidate()
					return
				}
				heavyFrameInProgress = true
				heavyFramePending = false
				try {
					const containerRef = container
					if (!containerRef) {
						return
					}

					const rows = processedRows
					totalRowCount.value = rows.length

					const virtualizationByProp = virtualizationFlag
					const verticalVirtualizationEnabled = virtualizationByProp && rowHeightMode === "fixed"
					virtualizationEnabled.value = verticalVirtualizationEnabled

					const layoutScale = 1
					const resolvedRowHeight = baseRowHeight * layoutScale
					effectiveRowHeight.value = resolvedRowHeight

					const metrics = viewportMetrics
					const measurements = layoutMeasurement
					let containerHeight = cachedContainerHeight
					let containerWidthValue = cachedContainerWidth
					let headerHeightValue = cachedHeaderHeight

					if (metrics) {
						containerHeight = metrics.containerHeight
						containerWidthValue = metrics.containerWidth
						headerHeightValue = metrics.headerHeight
					} else if (measurements) {
						containerHeight = measurements.containerHeight
						containerWidthValue = measurements.containerWidth
						headerHeightValue = measurements.headerHeight
					}

					if (containerHeight <= 0) {
						containerHeight = cachedContainerHeight > 0 ? cachedContainerHeight : resolvedRowHeight
					}
					if (containerWidthValue <= 0) {
						containerWidthValue = cachedContainerWidth > 0 ? cachedContainerWidth : columnSnapshot.containerWidthForColumns
					}
					if (headerHeightValue < 0) {
						headerHeightValue = cachedHeaderHeight > 0 ? cachedHeaderHeight : 0
					}

					const viewportHeightValue = Math.max(containerHeight - headerHeightValue, resolvedRowHeight)
					const viewportWidthValue = containerWidthValue
					viewportHeight.value = viewportHeightValue
					viewportWidth.value = viewportWidthValue

					const pendingScrollTopRequest = pendingScrollTop
					const pendingScrollLeftRequest = pendingScrollLeft
					const snapshotScrollTop = measurements?.scrollTop
					const snapshotScrollLeft = measurements?.scrollLeft
					const fallbackScrollTop = Number.isFinite(snapshotScrollTop) ? (snapshotScrollTop as number) : lastScrollTopSample
					const fallbackScrollLeft = Number.isFinite(snapshotScrollLeft) ? (snapshotScrollLeft as number) : lastScrollLeftSample
					const normalizedFallbackScrollTop = Number.isFinite(fallbackScrollTop) ? fallbackScrollTop : 0
					const normalizedFallbackScrollLeft = Number.isFinite(fallbackScrollLeft) ? fallbackScrollLeft : 0
					const pendingTop = typeof pendingScrollTopRequest === "number" && Number.isFinite(pendingScrollTopRequest)
						? pendingScrollTopRequest
						: normalizedFallbackScrollTop
					const pendingLeft = typeof pendingScrollLeftRequest === "number" && Number.isFinite(pendingScrollLeftRequest)
						? pendingScrollLeftRequest
						: normalizedFallbackScrollLeft
					const measuredScrollTopFromPending = typeof pendingScrollTopRequest === "number"
					const measuredScrollLeftFromPending = typeof pendingScrollLeftRequest === "number"
					pendingScrollTop = null
					pendingScrollLeft = null

					const hadPendingScrollTop = pendingScrollTopRequest != null
					const hadPendingScrollLeft = pendingScrollLeftRequest != null
					const scrollTopDelta = Math.abs(pendingTop - lastScrollTopSample)
					const scrollLeftDelta = Math.abs(pendingLeft - lastScrollLeftSample)
					const shouldFastPath =
						!force &&
						!pendingHorizontalSettle &&
						!measuredScrollTopFromPending &&
						!measuredScrollLeftFromPending &&
						!hadPendingScrollTop &&
						!hadPendingScrollLeft &&
						scrollTopDelta <= verticalScrollEpsilon &&
						scrollLeftDelta <= horizontalScrollEpsilon

					if (shouldFastPath) {
						scrollTop.value = lastScrollTopSample
						scrollLeft.value = lastScrollLeftSample
						pendingHorizontalSettle = false
						scheduleAfterScroll()
						return
					}

					recordHeavyPass()

					const virtualizationPrepared = virtualization.prepare({
						rows,
						totalRowCount: rows.length,
						viewportHeight: viewportHeightValue,
						resolvedRowHeight,
						zoomFactor: 1,
						virtualizationEnabled: verticalVirtualizationEnabled,
						pendingScrollTop: pendingTop,
						lastScrollTopSample,
						pendingScrollTopRequest,
						measuredScrollTopFromPending,
						cachedNativeScrollHeight,
						containerHeight,
						serverIntegration,
						imperativeCallbacks,
					})

					if (!virtualizationPrepared) {
						recordVirtualizerSkip()
						scheduleAfterScroll()
						return
					}
					recordVirtualizerUpdate()
					// Two-phase (A6): virtualization apply happens later with pending writes.

					let nextScrollTop = virtualizationPrepared.scrollTop
					let syncScrollTopValue: number | null = virtualizationPrepared.syncedScrollTop
					let syncScrollLeftValue: number | null = null
					const nowTs = virtualizationPrepared.timestamp

			const horizontalMetaResult = buildHorizontalMeta({
				columns,
				layoutScale,
				resolvePinMode,
				viewportWidth: viewportWidthValue,
				cachedNativeScrollWidth,
				cachedContainerWidth,
				lastScrollDirection,
				smoothedHorizontalVelocity,
				lastSignature: lastHorizontalMetaSignature,
				version: horizontalMetaVersion,
				scrollWidth: measurements?.scrollWidth ?? cachedNativeScrollWidth,
			})
			horizontalMetaVersion = horizontalMetaResult.version
			lastHorizontalMetaSignature = horizontalMetaResult.signature
			const columnMeta = horizontalMetaResult.meta
			const totalPinnedWidth = columnMeta.pinnedLeftWidth + columnMeta.pinnedRightWidth + columnMeta.indexColumnWidth
			const contentWidthEstimate = Math.max(columnMeta.metrics.totalWidth + totalPinnedWidth, viewportWidthValue)
			const contentHeightEstimate = Math.max(totalRowCount.value * resolvedRowHeight, viewportHeightValue)
			layoutCache.updateContentDimensions(contentWidthEstimate, contentHeightEstimate)
			const fallbackWidth =
				columnMeta.metrics.widths[0] ??
				columnMeta.pinnedLeft[0]?.width ??
				columnMeta.pinnedRight[0]?.width ??
				60
			const averageColumnWidth = columnMeta.metrics.widths.length
				? Math.max(1, columnMeta.metrics.totalWidth / Math.max(columnMeta.metrics.widths.length, 1))
				: Math.max(1, fallbackWidth)
			lastAverageColumnWidth = averageColumnWidth

			const currentPendingLeft = Math.max(0, pendingLeft)
			const rawDeltaLeft = currentPendingLeft - lastScrollLeftSample
			const deltaLeft = Math.abs(rawDeltaLeft)
			const horizontalDirection = rawDeltaLeft === 0 ? lastScrollDirection : rawDeltaLeft > 0 ? 1 : -1
			const horizontalVirtualizationEnabled = horizontalVirtualizationFlag

			const metaVersionChanged = columnMeta.version !== lastAppliedHorizontalMetaVersion
			const horizontalUpdateForced =
				force || pendingScrollLeftRequest != null || measuredScrollLeftFromPending || metaVersionChanged

			pendingHorizontalSettle = false

			const horizontalCallbacks = {
				applyColumnSnapshot,
				onColumns: typeof imperativeCallbacks.onColumns === "function"
					? imperativeCallbacks.onColumns
					: undefined,
			} satisfies HorizontalUpdateCallbacks

			// Two-phase (A6): compute horizontal plan without DOM writes.
			const horizontalPrepared: HorizontalUpdatePrepared = prepareHorizontalViewport({
				columnMeta,
				horizontalVirtualizer,
				horizontalOverscanController,
				callbacks: horizontalCallbacks,
				columnSnapshot,
				layoutScale,
				viewportWidth: viewportWidthValue,
				nowTs,
				frameTimeValue: frameTime.value,
				averageColumnWidth,
				scrollDirection: horizontalDirection,
				horizontalVirtualizationEnabled,
				horizontalUpdateForced,
				currentPendingLeft,
				previousScrollLeftSample: lastScrollLeftSample,
				deltaLeft,
				horizontalScrollEpsilon,
				pendingScrollLeftRequest,
				measuredScrollLeftFromPending,
				currentScrollLeftMeasurement: normalizedFallbackScrollLeft,
				clampScrollLeft: clampScrollLeftValue,
				smoothedHorizontalVelocity,
				lastHorizontalSampleTime,
				horizontalOverscan,
				lastAppliedHorizontalMetaVersion,
			})

			const horizontalScrollLeftValue = horizontalPrepared.scrollLeftValue
			if (horizontalPrepared.syncScrollLeftValue != null) {
				syncScrollLeftValue = horizontalPrepared.syncScrollLeftValue
			}

			smoothedHorizontalVelocity = horizontalPrepared.smoothedHorizontalVelocity
			horizontalOverscan = horizontalPrepared.horizontalOverscan
			lastHorizontalSampleTime = horizontalPrepared.lastHorizontalSampleTime
			lastScrollDirection = horizontalPrepared.lastScrollDirection
			lastScrollLeftSample = horizontalPrepared.lastScrollLeftSample
			lastAppliedHorizontalMetaVersion = horizontalPrepared.lastAppliedHorizontalMetaVersion

			const virtualizationResult = virtualization.applyPrepared(virtualizationPrepared, {
				rows,
				serverIntegration,
				imperativeCallbacks,
			})
			const pendingVerticalScrollWrite = virtualizationResult.pendingScrollWrite
			const pendingHorizontalScrollWrite = horizontalPrepared.pendingScrollWrite

			applyHorizontalViewport({
				callbacks: horizontalCallbacks,
				prepared: horizontalPrepared,
			})

			const containerElement = containerRef
			if (containerElement) {
				if (
					pendingVerticalScrollWrite != null &&
					containerElement.scrollTop !== pendingVerticalScrollWrite
				) {
					containerElement.scrollTop = pendingVerticalScrollWrite
					recordLayoutWrite()
				}
				if (
					pendingHorizontalScrollWrite != null &&
					containerElement.scrollLeft !== pendingHorizontalScrollWrite
				) {
					containerElement.scrollLeft = pendingHorizontalScrollWrite
					recordLayoutWrite()
				}
			}
			// Verified (A5): heavy path leaves pinned/header transforms to sync layer.
			scrollLeft.value = horizontalScrollLeftValue
			nextScrollTop = virtualizationResult.scrollTop
			syncScrollTopValue = virtualizationResult.syncedScrollTop
			lastScrollTopSample = virtualizationResult.lastScrollTopSample
			pendingScrollTop = virtualizationResult.pendingScrollTop

			const resolvedScrollTop = syncScrollTopValue ?? nextScrollTop
			const resolvedScrollLeft = syncScrollLeftValue ?? horizontalScrollLeftValue
			updateCachedScrollOffsets(resolvedScrollTop, resolvedScrollLeft)
			lastHeavyScrollTop = resolvedScrollTop
			lastHeavyScrollLeft = resolvedScrollLeft

			emitImperativeScrollSync(resolvedScrollTop, resolvedScrollLeft, nowTs)

				if (onNearBottom && viewportHeightValue > 0 && totalRowCount.value > 0) {
					const threshold = Math.max(0, totalContentHeight.value - viewportHeightValue * 2)
					if (nextScrollTop >= threshold && !loading) {
						onNearBottom()
					}
				}

				lastScrollTopSample = nextScrollTop

				scheduleAfterScroll()
			} finally {
				heavyFrameInProgress = false
			}
		}

		return {
			input,
			core,
			derived,
			attach,
			detach,
			setProcessedRows: setProcessedRowsValue,
			setColumns: setColumnsValue,
			setZoom: setZoomValue,
			setVirtualizationEnabled: setVirtualizationEnabledValue,
			setHorizontalVirtualizationEnabled: setHorizontalVirtualizationEnabledValue,
			setRowHeightMode: setRowHeightModeValue,
			setBaseRowHeight: setBaseRowHeightValue,
			setViewportMetrics: setViewportMetricsValue,
			setIsLoading: setIsLoadingValue,
			setImperativeCallbacks: setImperativeCallbacksValue,
			setOnAfterScroll: setOnAfterScrollValue,
			setOnNearBottom: setOnNearBottomValue,
			setServerIntegration: setServerIntegrationValue,
			setDebugMode: setDebugModeValue,
			handleScroll,
			updateViewportHeight: updateViewportHeightValue,
			measureRowHeight: measureRowHeightValue,
			cancelScrollRaf,
			scrollToRow: scrollToRowValue,
			scrollToColumn: scrollToColumnValue,
			isRowVisible: isRowVisibleValue,
			clampScrollTopValue,
			setViewportSyncTargets: setViewportSyncTargetsValue,
			refresh: refreshValue,
			dispose: disposeValue,
		}
	}
