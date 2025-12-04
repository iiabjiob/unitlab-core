import { COLUMN_VIRTUALIZATION_BUFFER } from "../dom/gridUtils"
import { INDEX_COLUMN_WIDTH } from "../utils/constants"
import type { UiTableColumn } from "../types"
import {
	computeColumnLayout,
	type ColumnLayoutMetric,
	type ColumnLayoutOutput,
} from "../virtualization/horizontalLayout"
import type { HorizontalVirtualizerMeta } from "../virtualization/horizontalVirtualizer"
import type { TableViewportControllerOptions } from "./tableViewportTypes"

type LayoutOutput = ColumnLayoutOutput<UiTableColumn>

export interface TableViewportHorizontalMeta extends HorizontalVirtualizerMeta<UiTableColumn> {
	scrollableColumns: UiTableColumn[]
	scrollableIndices: LayoutOutput["scrollableIndices"]
	metrics: LayoutOutput["scrollableMetrics"]
	pinnedLeft: ColumnLayoutMetric<UiTableColumn>[]
	pinnedRight: ColumnLayoutMetric<UiTableColumn>[]
	indexColumnWidth: number
	effectiveViewport: number
	version: number
	scrollVelocity: number
}

export interface BuildHorizontalMetaInput {
	columns: UiTableColumn[]
	layoutScale: number
	resolvePinMode: TableViewportControllerOptions["resolvePinMode"]
	viewportWidth: number
	cachedNativeScrollWidth: number
	cachedContainerWidth: number
	lastScrollDirection: number
	smoothedHorizontalVelocity: number
	lastSignature: string
	version: number
	scrollWidth?: number
}

export interface BuildHorizontalMetaResult {
	meta: TableViewportHorizontalMeta
	version: number
	signature: string
}

export function buildHorizontalMeta({
	columns,
	layoutScale,
	resolvePinMode,
	viewportWidth,
	cachedNativeScrollWidth,
	cachedContainerWidth,
	lastScrollDirection,
	smoothedHorizontalVelocity,
	lastSignature,
	version,
	scrollWidth,
}: BuildHorizontalMetaInput): BuildHorizontalMetaResult {
	const layout = computeColumnLayout({
		columns,
		zoom: layoutScale,
		resolvePinMode,
	})

	const indexColumnWidth = INDEX_COLUMN_WIDTH * layoutScale
	const containerWidthForColumns = Math.max(0, viewportWidth - indexColumnWidth)

	let nativeScrollLimit = 0
	const measuredWidth = Number.isFinite(scrollWidth) ? (scrollWidth as number) : -1
	if (measuredWidth >= 0 && viewportWidth >= 0) {
		nativeScrollLimit = Math.max(0, measuredWidth - viewportWidth)
	} else if (cachedNativeScrollWidth >= 0 && cachedContainerWidth >= 0) {
		nativeScrollLimit = Math.max(0, cachedNativeScrollWidth - cachedContainerWidth)
	}

	const effectiveViewport = Math.max(0, containerWidthForColumns - layout.pinnedLeftWidth - layout.pinnedRightWidth)
	const signature = `${layout.scrollableColumns.length}|${layout.scrollableMetrics.totalWidth}|${layout.zoom}|${containerWidthForColumns}|${layout.pinnedLeftWidth}|${layout.pinnedRightWidth}`
	const nextVersion = signature === lastSignature ? version : version + 1

	const meta: TableViewportHorizontalMeta = {
		scrollableColumns: layout.scrollableColumns,
		scrollableIndices: layout.scrollableIndices,
		metrics: layout.scrollableMetrics,
		pinnedLeft: layout.pinnedLeft,
		pinnedRight: layout.pinnedRight,
		pinnedLeftWidth: layout.pinnedLeftWidth,
		pinnedRightWidth: layout.pinnedRightWidth,
		zoom: layout.zoom,
		containerWidthForColumns,
		scrollDirection: lastScrollDirection,
		nativeScrollLimit,
		buffer: COLUMN_VIRTUALIZATION_BUFFER,
		indexColumnWidth,
		effectiveViewport,
		version: nextVersion,
		scrollVelocity: smoothedHorizontalVelocity,
	}

	return {
		meta,
		version: nextVersion,
		signature,
	}
}
