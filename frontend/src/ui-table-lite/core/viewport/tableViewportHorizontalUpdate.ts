import type { AxisVirtualizer, AxisVirtualizerState } from "../virtualization/axisVirtualizer"
import type { HorizontalOverscanController } from "../virtualization/dynamicOverscan"
import type { ColumnVirtualizationSnapshot } from "../virtualization/columnSnapshot"
import type { HorizontalVirtualizerMeta, HorizontalVirtualizerPayload } from "../virtualization/horizontalVirtualizer"
import type { UiTableColumn } from "../types"
import type { TableViewportHorizontalMeta } from "./tableViewportHorizontalMeta"
import type { ImperativeColumnUpdatePayload } from "./tableViewportTypes"

export interface HorizontalUpdateCallbacks {
	applyColumnSnapshot: (
		meta: TableViewportHorizontalMeta,
		start: number,
		end: number,
		payload: HorizontalVirtualizerPayload,
	) => void
	logHorizontalDebug?: (details: {
		scrollLeft: number
		deltaLeft: number
		overscanColumns: number
		horizontalOverscan: number
		velocity: number
		direction: number
		horizontalState: AxisVirtualizerState<HorizontalVirtualizerPayload>
		columnMeta: TableViewportHorizontalMeta
		virtualizationEnabled: boolean
	}) => void
	onColumns?: (payload: ImperativeColumnUpdatePayload) => void
}

export interface HorizontalUpdatePrepared {
	shouldUpdate: boolean
	scrollLeftValue: number
	syncScrollLeftValue: number | null
	smoothedHorizontalVelocity: number
	horizontalOverscan: number
	lastHorizontalSampleTime: number
	lastScrollDirection: number
	lastScrollLeftSample: number
	lastAppliedHorizontalMetaVersion: number
	pendingScrollWrite: number | null
	columnSnapshot?: {
		meta: TableViewportHorizontalMeta
		start: number
		end: number
		payload: HorizontalVirtualizerPayload
	}
	debugPayload?: {
		scrollLeft: number
		deltaLeft: number
		overscanColumns: number
		horizontalOverscan: number
		velocity: number
		direction: number
		horizontalState: AxisVirtualizerState<HorizontalVirtualizerPayload>
		columnMeta: TableViewportHorizontalMeta
		virtualizationEnabled: boolean
	}
	columnCallbackPayload?: ImperativeColumnUpdatePayload
}

export interface HorizontalUpdateParams {
	columnMeta: TableViewportHorizontalMeta
	horizontalVirtualizer: AxisVirtualizer<HorizontalVirtualizerMeta<UiTableColumn>, HorizontalVirtualizerPayload>
	horizontalOverscanController: HorizontalOverscanController
	callbacks: HorizontalUpdateCallbacks
	columnSnapshot: ColumnVirtualizationSnapshot<UiTableColumn>
	layoutScale: number
	viewportWidth: number
	nowTs: number
	frameTimeValue: number
	averageColumnWidth: number
	scrollDirection: number
	horizontalVirtualizationEnabled: boolean
	horizontalUpdateForced: boolean
	currentPendingLeft: number
	previousScrollLeftSample: number
	deltaLeft: number
	horizontalScrollEpsilon: number
	pendingScrollLeftRequest: number | null
	measuredScrollLeftFromPending: boolean
	currentScrollLeftMeasurement: number
	clampScrollLeft: (value: number) => number
	smoothedHorizontalVelocity: number
	lastHorizontalSampleTime: number
	horizontalOverscan: number
	lastAppliedHorizontalMetaVersion: number
}

export interface HorizontalUpdateResult {
	shouldUpdate: boolean
	scrollLeftValue: number
	syncScrollLeftValue: number | null
	smoothedHorizontalVelocity: number
	horizontalOverscan: number
	lastHorizontalSampleTime: number
	lastScrollDirection: number
	lastScrollLeftSample: number
	lastAppliedHorizontalMetaVersion: number
}

function computeEffectiveVelocity(frameTimeValue: number, smoothedVelocity: number): number {
	if (frameTimeValue > 0) {
		const fps = Math.min(240, 1000 / frameTimeValue)
		return fps >= 30 ? smoothedVelocity : 0
	}
	return 0
}

export function prepareHorizontalViewport(params: HorizontalUpdateParams): HorizontalUpdatePrepared {
	const {
		columnMeta,
		horizontalVirtualizer,
		horizontalOverscanController,
		columnSnapshot,
		layoutScale,
		viewportWidth,
		nowTs,
		frameTimeValue,
		averageColumnWidth,
		scrollDirection,
		horizontalVirtualizationEnabled,
		horizontalUpdateForced,
		currentPendingLeft,
		previousScrollLeftSample,
		deltaLeft,
		horizontalScrollEpsilon,
		pendingScrollLeftRequest,
		measuredScrollLeftFromPending,
		currentScrollLeftMeasurement,
		clampScrollLeft,
	} = params

	let { smoothedHorizontalVelocity, lastHorizontalSampleTime, horizontalOverscan, lastAppliedHorizontalMetaVersion } = params

	const shouldUpdate = horizontalUpdateForced || deltaLeft > horizontalScrollEpsilon

	if (!shouldUpdate) {
		return {
			shouldUpdate: false,
			scrollLeftValue: previousScrollLeftSample,
			syncScrollLeftValue: null,
			smoothedHorizontalVelocity,
			horizontalOverscan,
			lastHorizontalSampleTime,
			lastScrollDirection: scrollDirection,
			lastScrollLeftSample: previousScrollLeftSample,
			lastAppliedHorizontalMetaVersion,
			pendingScrollWrite: null,
		}
	}

	if (lastHorizontalSampleTime > 0) {
		const deltaTime = Math.max(nowTs - lastHorizontalSampleTime, 1)
		const instantVelocity = (currentPendingLeft - previousScrollLeftSample) / (deltaTime / 1000)
		if (Number.isFinite(instantVelocity)) {
			const smoothing = 0.35
			smoothedHorizontalVelocity = smoothedHorizontalVelocity * (1 - smoothing) + instantVelocity * smoothing
		} else {
			smoothedHorizontalVelocity *= 0.5
		}
	} else {
		smoothedHorizontalVelocity *= 0.5
	}

	if (horizontalVirtualizationEnabled) {
		const { overscan } = horizontalOverscanController.update({
			timestamp: nowTs,
			delta: deltaLeft,
			viewportSize: columnMeta.effectiveViewport,
			itemSize: Math.max(averageColumnWidth, 1),
			totalItems: columnMeta.scrollableColumns.length,
			virtualizationEnabled: true,
		})
		horizontalOverscan = overscan
	} else {
		const totalScrollable = Math.max(0, columnMeta.scrollableColumns.length)
		horizontalOverscanController.reset(nowTs, totalScrollable)
		horizontalOverscan = totalScrollable
	}

	const effectiveScrollVelocity = computeEffectiveVelocity(frameTimeValue, smoothedHorizontalVelocity)
	columnMeta.scrollVelocity = effectiveScrollVelocity
	columnMeta.scrollDirection = scrollDirection

	const horizontalState = horizontalVirtualizer.update({
		axis: "horizontal",
		viewportSize: columnMeta.effectiveViewport,
		scrollOffset: currentPendingLeft,
		virtualizationEnabled: horizontalVirtualizationEnabled,
		estimatedItemSize: averageColumnWidth,
		totalCount: columnMeta.scrollableColumns.length,
		overscan: horizontalOverscan,
		meta: columnMeta,
	})

	const payload = horizontalState.payload ?? {
		visibleStart: horizontalState.startIndex,
		visibleEnd: horizontalState.endIndex,
		leftPadding: 0,
		rightPadding: 0,
		totalScrollableWidth: columnMeta.metrics.totalWidth,
		visibleScrollableWidth: columnMeta.metrics.totalWidth,
		averageWidth: averageColumnWidth,
		scrollSpeed: Math.abs(effectiveScrollVelocity),
		effectiveViewport: columnMeta.effectiveViewport,
	}

	let nextScrollLeft = clampScrollLeft(horizontalState.offset)
	const currentScrollLeft = currentScrollLeftMeasurement
	let syncScrollLeftValue: number | null = null

	const needsScrollLeftWrite =
		Math.abs(currentScrollLeft - nextScrollLeft) > horizontalScrollEpsilon ||
		pendingScrollLeftRequest != null ||
		measuredScrollLeftFromPending

	if (needsScrollLeftWrite) {
		syncScrollLeftValue = nextScrollLeft
	} else {
		nextScrollLeft = currentScrollLeft
	}

	const virtualizerState = horizontalVirtualizer.getState()
	lastHorizontalSampleTime = nowTs
	lastAppliedHorizontalMetaVersion = columnMeta.version

	const debugPayload = {
		scrollLeft: nextScrollLeft,
		deltaLeft,
		overscanColumns: horizontalOverscan,
		horizontalOverscan,
		velocity: horizontalOverscanController.getState().smoothedVelocity,
		direction: scrollDirection,
		horizontalState: virtualizerState,
		columnMeta,
		virtualizationEnabled: horizontalVirtualizationEnabled,
	}

	const columnPayload: ImperativeColumnUpdatePayload = {
		snapshot: columnSnapshot,
		scrollLeft: nextScrollLeft,
		viewportWidth,
		zoom: layoutScale,
		timestamp: nowTs,
	}

	return {
		shouldUpdate: true,
		scrollLeftValue: nextScrollLeft,
		syncScrollLeftValue,
		smoothedHorizontalVelocity,
		horizontalOverscan,
		lastHorizontalSampleTime,
		lastScrollDirection: scrollDirection,
		lastScrollLeftSample: nextScrollLeft,
		lastAppliedHorizontalMetaVersion,
		pendingScrollWrite: needsScrollLeftWrite ? nextScrollLeft : null,
		columnSnapshot: {
			meta: columnMeta,
			start: horizontalState.startIndex,
			end: horizontalState.endIndex,
			payload,
		},
		debugPayload,
		columnCallbackPayload: columnPayload,
	}
}

export interface HorizontalViewportApplyArgs {
	callbacks: HorizontalUpdateCallbacks
	prepared: HorizontalUpdatePrepared
}

export function applyHorizontalViewport({ callbacks, prepared }: HorizontalViewportApplyArgs): void {
	if (!prepared.shouldUpdate) {
		return
	}

	if (prepared.columnSnapshot) {
		callbacks.applyColumnSnapshot(
			prepared.columnSnapshot.meta,
			prepared.columnSnapshot.start,
			prepared.columnSnapshot.end,
			prepared.columnSnapshot.payload,
		)
	}

	if (prepared.debugPayload) {
		callbacks.logHorizontalDebug?.(prepared.debugPayload)
	}

	if (prepared.columnCallbackPayload && callbacks.onColumns) {
		callbacks.onColumns(prepared.columnCallbackPayload)
	}
}
