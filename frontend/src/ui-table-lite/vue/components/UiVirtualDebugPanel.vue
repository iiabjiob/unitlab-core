<template>
  <aside class="ui-table__virtual-debug-panel" aria-hidden="true">
    <header class="ui-table__virtual-debug-header">
      <span class="ui-table__virtual-debug-title">Viewport Debug</span>
      <span class="ui-table__virtual-debug-fps" :class="{ 'is-idle': metrics.fps < 20 }">{{ fpsLabel }} fps</span>
    </header>

    <section class="ui-table__virtual-debug-section">
      <h3 class="ui-table__virtual-debug-section-title">Performance</h3>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Frame time</span>
        <span class="ui-table__virtual-debug-badge ui-table__virtual-debug-badge--primary">{{ frameTimeLabel }}</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Dropped frames</span>
        <span class="ui-table__virtual-debug-badge ui-table__virtual-debug-badge--warning">{{ droppedFrameLabel }}</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Layout I/O</span>
        <span class="ui-table__virtual-debug-value">{{ layoutReadsLabel }}R · {{ layoutWritesLabel }}W</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Sync path</span>
        <span>{{ syncScrollRateLabel }}/s</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Heavy path</span>
        <span>{{ heavyUpdateRateLabel }}/s</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Virtualiser</span>
        <span>{{ virtualizerUpdateLabel }}/s · {{ virtualizerSkipLabel }}/s skip</span>
      </div>
    </section>

    <section class="ui-table__virtual-debug-section">
      <h3 class="ui-table__virtual-debug-section-title">Rows</h3>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Visible range</span>
        <span>#{{ formatInt(metrics.rows.visibleStartIndex) }} – #{{ formatRangeEnd(metrics.rows.visibleEndIndex) }}</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Pool span</span>
        <span>#{{ formatInt(metrics.rows.startIndex) }} – #{{ formatRangeEnd(metrics.rows.endIndex) }}</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Counts</span>
        <span>{{ formatInt(metrics.rows.visibleCount) }} visible · {{ formatInt(metrics.rows.poolSize) }} pooled</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Overscan</span>
        <span>{{ formatInt(metrics.rows.overscanLeading) }} ↑ · {{ formatInt(metrics.rows.overscanTrailing) }} ↓ ({{ overscanLabel }} rows)</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Pool utilization</span>
        <span class="ui-table__virtual-debug-value">{{ rowUtilizationLabel }}% · {{ rowPoolRatioLabel }}</span>
      </div>
      <div class="ui-table__virtual-debug-bar">
        <div class="ui-table__virtual-debug-bar-track" />
        <div class="ui-table__virtual-debug-bar-pool" :style="poolRangeStyle" />
        <div class="ui-table__virtual-debug-bar-visible" :style="visibleRangeStyle" />
      </div>
    </section>

    <section class="ui-table__virtual-debug-section">
      <h3 class="ui-table__virtual-debug-section-title">Row Pool</h3>
      <div class="ui-table__virtual-debug-pool ui-table__virtual-debug-pool--rows">
        <div
          v-for="item in rowPoolItems"
          :key="item.poolIndex"
          class="ui-table__virtual-debug-pool-item"
          :class="[
            `ui-table__virtual-debug-pool-item--${item.segment}`,
            { 'ui-table__virtual-debug-pool-item--empty': !item.active }
          ]"
        >
          <span class="ui-table__virtual-debug-pool-label">{{ formatRowPoolLabel(item) }}</span>
        </div>
      </div>
    </section>

    <section v-if="serverState" class="ui-table__virtual-debug-section">
      <h3 class="ui-table__virtual-debug-section-title">Server Row Model</h3>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Status</span>
        <span>{{ serverStatusLabel }}</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Progress</span>
        <span class="ui-table__virtual-debug-badge ui-table__virtual-debug-badge--primary">{{ serverProgressLabel }}</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Cache</span>
        <span>{{ serverCacheLabel }}</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Pending</span>
        <span>{{ serverPendingLabel }}</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Hit ratio</span>
        <span>{{ serverHitLabel }}</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Prefetch threshold</span>
        <span>{{ serverThresholdLabel }}</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Loaded ranges</span>
        <span>{{ serverRangesLabel }}</span>
      </div>
    </section>

    <section class="ui-table__virtual-debug-section">
      <h3 class="ui-table__virtual-debug-section-title">Columns</h3>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Visible window</span>
        <span>#{{ formatInt(metrics.columns.visibleStart) }} – #{{ formatInt(metrics.columns.visibleEnd) }}</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Pinned</span>
        <span>{{ formatInt(metrics.columns.pinnedLeft) }} left · {{ formatInt(metrics.columns.pinnedRight) }} right</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Scrollable</span>
        <span>{{ formatInt(metrics.columns.scrollable) }} cols · {{ formatPx(metrics.columns.scrollableWidth) }}</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Total width</span>
        <span>{{ formatPx(metrics.columns.totalWidth) }}</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Pool utilization</span>
        <span class="ui-table__virtual-debug-value">{{ columnUtilizationLabel }}% · {{ columnPoolRatioLabel }}</span>
      </div>
    </section>

    <section class="ui-table__virtual-debug-section">
      <h3 class="ui-table__virtual-debug-section-title">Column Pool</h3>
      <div class="ui-table__virtual-debug-pool ui-table__virtual-debug-pool--columns">
        <div
          v-for="item in columnPoolItems"
          :key="item.key"
          class="ui-table__virtual-debug-pool-item"
          :class="[
            `ui-table__virtual-debug-pool-item--${item.segment}`,
            `ui-table__virtual-debug-pool-item--${item.band}`,
            { 'ui-table__virtual-debug-pool-item--empty': !item.active }
          ]"
        >
          <span class="ui-table__virtual-debug-pool-label">{{ formatColumnPoolLabel(item) }}</span>
        </div>
      </div>
    </section>

    <section class="ui-table__virtual-debug-section">
      <h3 class="ui-table__virtual-debug-section-title">Viewport</h3>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Scroll</span>
        <span>{{ formatPx(metrics.scrollTop) }} · {{ formatPx(metrics.scrollLeft) }}</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Viewport</span>
        <span>{{ formatPx(metrics.viewportWidth) }} × {{ formatPx(metrics.viewportHeight) }}</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Content height</span>
        <span>{{ formatPx(metrics.totalContentHeight) }}</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Row height</span>
        <span>{{ formatPx(metrics.rows.estimatedRowHeight) }}</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Status</span>
        <span>{{ metrics.virtualizationEnabled ? "Virtualized" : "Full render" }}</span>
      </div>
    </section>

    <section class="ui-table__virtual-debug-section">
      <h3 class="ui-table__virtual-debug-section-title">DOM</h3>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Total elements</span>
        <span>{{ domElementLabel }}</span>
      </div>
      <div class="ui-table__virtual-debug-metric-row">
        <span>Last sampled</span>
        <span>{{ domLastUpdatedLabel }}</span>
      </div>
    </section>
  </aside>
</template>

<script setup lang="ts">
import { computed } from "vue"
import type {
  VirtualDebugMetrics,
  VirtualDebugRowPoolItem,
  VirtualDebugColumnPoolItem,
} from "../composables/useVirtualDebug"

const props = defineProps<{
  metrics: VirtualDebugMetrics
}>()

const intFormatter = new Intl.NumberFormat(undefined, { maximumFractionDigits: 0 })
const decimalFormatter = new Intl.NumberFormat(undefined, { minimumFractionDigits: 0, maximumFractionDigits: 1 })

const fpsLabel = computed(() => decimalFormatter.format(props.metrics.fps))
const frameTimeLabel = computed(() => formatMs(props.metrics.frameTime))
const droppedFrameLabel = computed(() => formatInt(props.metrics.droppedFrames))
const rowUtilizationLabel = computed(() => decimalFormatter.format(props.metrics.rows.poolUtilization * 100))
const rowPoolRatioLabel = computed(() => formatPoolRatio(props.metrics.rows.visibleCount, props.metrics.rows.poolSize))
const columnUtilizationLabel = computed(() => decimalFormatter.format(props.metrics.columns.poolUtilization * 100))
const columnPoolRatioLabel = computed(() => formatPoolRatio(props.metrics.columns.visibleCount, props.metrics.columns.poolSize))
const layoutReadsLabel = computed(() => formatInt(props.metrics.layoutReads))
const layoutWritesLabel = computed(() => formatInt(props.metrics.layoutWrites))
const syncScrollRateLabel = computed(() => formatRate(props.metrics.syncScrollRate))
const heavyUpdateRateLabel = computed(() => formatRate(props.metrics.heavyUpdateRate))
const virtualizerUpdateLabel = computed(() => formatRate(props.metrics.virtualizerUpdates))
const virtualizerSkipLabel = computed(() => formatRate(props.metrics.virtualizerSkips))
const overscanLabel = computed(() => intFormatter.format(props.metrics.rows.overscanSize))
const domElementLabel = computed(() => formatInt(props.metrics.dom.totalElements))
const domLastUpdatedLabel = computed(() => formatLastUpdated(props.metrics.dom.lastUpdated))
const serverState = computed(() => props.metrics.server ?? null)
const serverStatusLabel = computed(() => (serverState.value?.enabled ? "Enabled" : "Disabled"))
const serverProgressLabel = computed(() => {
  const progress = serverState.value?.progress
  if (progress == null || !Number.isFinite(progress)) {
    return "—"
  }
  const clamped = Math.max(0, Math.min(progress, 1))
  return `${decimalFormatter.format(clamped * 100)}%`
})
const serverCacheLabel = computed(() => {
  if (!serverState.value) return "—"
  const { cacheBlocks, cachedRows } = serverState.value
  return `${formatInt(cacheBlocks)} blocks · ${formatInt(cachedRows)} rows`
})
const serverPendingLabel = computed(() => {
  if (!serverState.value) return "—"
  const { pendingBlocks, pendingRequests, abortedRequests } = serverState.value
  return `${formatInt(pendingBlocks)} blocks · ${formatInt(pendingRequests)} req · ${formatInt(abortedRequests)} aborted`
})
const serverHitLabel = computed(() => {
  if (!serverState.value) return "—"
  const { cacheHits, cacheMisses } = serverState.value
  const total = cacheHits + cacheMisses
  if (total <= 0) {
    return `${formatInt(cacheHits)} hits`
  }
  const ratio = total > 0 ? (cacheHits / total) * 100 : 0
  return `${formatInt(cacheHits)} hits (${decimalFormatter.format(ratio)}%)`
})
const serverThresholdLabel = computed(() => {
  if (!serverState.value) return "—"
  const threshold = Math.max(0, Math.min(serverState.value.effectivePreloadThreshold, 1))
  return `${decimalFormatter.format(threshold * 100)}%`
})
const serverRangesLabel = computed(() => {
  const ranges = serverState.value?.loadedRanges ?? []
  if (!ranges.length) {
    return "—"
  }
  const limit = 4
  const formatted = ranges.slice(0, limit).map(range => formatRange(range.start, range.end))
  if (ranges.length > limit) {
    formatted.push(`+${formatInt(ranges.length - limit)} more`)
  }
  return formatted.join(", ")
})

const rowPoolItems = computed(() => props.metrics.rowPool)
const columnPoolItems = computed(() => props.metrics.columnPool)

const poolRangeStyle = computed(() => ({
  left: `${Math.max(0, Math.min(props.metrics.rows.poolRange.startPercent, 100))}%`,
  width: `${Math.max(0, Math.min(props.metrics.rows.poolRange.sizePercent, 100))}%`,
}))

const visibleRangeStyle = computed(() => ({
  left: `${Math.max(0, Math.min(props.metrics.rows.visibleRange.startPercent, 100))}%`,
  width: `${Math.max(0, Math.min(props.metrics.rows.visibleRange.sizePercent, 100))}%`,
}))

function formatInt(value: number) {
  return intFormatter.format(Math.max(0, Number.isFinite(value) ? value : 0))
}

function formatRate(value: number) {
  if (!Number.isFinite(value) || value <= 0) {
    return decimalFormatter.format(0)
  }
  return decimalFormatter.format(value)
}

function formatMs(value: number) {
  if (!Number.isFinite(value) || value <= 0) {
    return "0.0ms"
  }
  return `${decimalFormatter.format(value)}ms`
}

function formatPoolRatio(visible: number, pool: number) {
  const safeVisible = Number.isFinite(visible) && visible > 0 ? visible : 0
  const safePool = Number.isFinite(pool) && pool > 0 ? pool : 0
  return `${formatInt(safeVisible)}/${formatInt(safePool)}`
}

function formatPx(value: number) {
  if (!Number.isFinite(value)) {
    return "0px"
  }
  return `${Math.round(value)}px`
}

function formatRangeEnd(value: number) {
  if (!Number.isFinite(value)) {
    return formatInt(0)
  }
  return formatInt(Math.max(value - 1, 0))
}

function formatRange(start: number, end: number) {
  const normalizedStart = Number.isFinite(start) ? Math.max(0, start) : 0
  const normalizedEnd = Number.isFinite(end) ? Math.max(normalizedStart, end) : normalizedStart
  return `#${formatInt(normalizedStart)}–#${formatInt(normalizedEnd)}`
}

function formatLastUpdated(timestamp: number) {
  if (!Number.isFinite(timestamp) || timestamp <= 0) {
    return "—"
  }
  const deltaMs = Math.max(0, Date.now() - timestamp)
  const deltaSeconds = Math.round(deltaMs / 1000)
  if (deltaSeconds < 1) {
    return "just now"
  }
  if (deltaSeconds < 60) {
    return `${deltaSeconds}s ago`
  }
  const deltaMinutes = Math.round(deltaSeconds / 60)
  if (deltaMinutes < 60) {
    return `${deltaMinutes}m ago`
  }
  const deltaHours = Math.round(deltaMinutes / 60)
  return `${deltaHours}h ago`
}

function formatRowPoolLabel(item: VirtualDebugRowPoolItem) {
  const original = item.originalIndex ?? item.displayIndex ?? item.rowIndex
  return `#${formatInt(item.rowIndex)}[${formatInt(original)}]`
}

function formatColumnPoolLabel(item: VirtualDebugColumnPoolItem) {
  const original = item.originalIndex ?? item.colIndex
  return `#${formatInt(item.colIndex)}[${formatInt(original)}]`
}
</script>
