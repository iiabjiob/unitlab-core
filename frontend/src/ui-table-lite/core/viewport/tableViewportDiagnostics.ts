import type { WritableSignal } from "../runtime/signals"
import type { RafScheduler } from "../runtime/rafScheduler"
import type { ViewportClock } from "./tableViewportConfig"

interface DiagnosticsSignals {
	debugMode: WritableSignal<boolean>
	fps: WritableSignal<number>
	frameTime: WritableSignal<number>
	droppedFrames: WritableSignal<number>
	layoutReads: WritableSignal<number>
	layoutWrites: WritableSignal<number>
	syncScrollRate: WritableSignal<number>
	heavyUpdateRate: WritableSignal<number>
	virtualizerUpdates: WritableSignal<number>
	virtualizerSkips: WritableSignal<number>
}

export interface TableViewportDiagnosticsOptions {
	scheduler: RafScheduler
	clock: ViewportClock
	signals: DiagnosticsSignals
}

export interface TableViewportDiagnostics {
	recordLayoutRead: (count?: number) => void
	recordLayoutWrite: (count?: number) => void
	recordSyncScroll: () => void
	recordHeavyPass: () => void
	recordVirtualizerUpdate: () => void
	recordVirtualizerSkip: () => void
	setDebugMode: (enabled: boolean) => void
	isDebugEnabled: () => boolean
	dispose: () => void
}

const MAX_FPS = 240
const FRAME_DURATION_MS = 1000 / 60
const FPS_EMA_ALPHA = 0.2

const globalWindow: typeof window | undefined =
	typeof window !== "undefined" ? window : undefined

function resolveGlobalDebugDefault(): boolean {
	if (
		globalWindow &&
		(globalWindow as typeof window & { __UNITLAB_TABLE_DEBUG__?: boolean }).__UNITLAB_TABLE_DEBUG__
	) {
		return true
	}
	return false
}

export function createTableViewportDiagnostics(
	options: TableViewportDiagnosticsOptions,
): TableViewportDiagnostics {
	const { scheduler, clock, signals } = options
	const {
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
	} = signals

	const debugDefault = resolveGlobalDebugDefault()
	let runtimeOverride: boolean | null = debugMode.value ? debugMode.value : null
	let monitoring = false
	let rafHandle: number | null = null
	let fallbackTaskId: number | null = null
	let lastTimestamp: number | null = null
	let fpsEma = 0
	let droppedFrameCount = 0
	let frameLayoutReads = 0
	let frameLayoutWrites = 0
	let lastPublishedReads = layoutReads.value
	let lastPublishedWrites = layoutWrites.value
	let syncScrollCount = 0
	let heavyPassCount = 0
	let virtualizerUpdateCount = 0
	let virtualizerSkipCount = 0
	let lastPerformancePublish = clock.now()

	const hasNativeRaf = Boolean(globalWindow?.requestAnimationFrame)

	function recordLayoutRead(count = 1) {
		if (!debugMode.value) return
		frameLayoutReads += count
	}

	function recordLayoutWrite(count = 1) {
		if (!debugMode.value) return
		frameLayoutWrites += count
	}

	function recordSyncScroll() {
		if (!debugMode.value) return
		syncScrollCount += 1
	}

	function recordHeavyPass() {
		if (!debugMode.value) return
		heavyPassCount += 1
	}

	function recordVirtualizerUpdate() {
		if (!debugMode.value) return
		virtualizerUpdateCount += 1
	}

	function recordVirtualizerSkip() {
		if (!debugMode.value) return
		virtualizerSkipCount += 1
	}

	function scheduleFrameLoop() {
		if (!monitoring) return
		if (!debugMode.value) return
		if (rafHandle !== null || fallbackTaskId !== null) return
		if (hasNativeRaf && globalWindow) {
			rafHandle = globalWindow.requestAnimationFrame(handleFrame)
			return
		}
		fallbackTaskId = scheduler.schedule(() => handleFrame(clock.now()), {
			priority: "high",
		})
	}

	function cancelScheduledFrame() {
		if (rafHandle !== null && globalWindow?.cancelAnimationFrame) {
			globalWindow.cancelAnimationFrame(rafHandle)
		}
		rafHandle = null
		if (fallbackTaskId !== null) {
			scheduler.cancel(fallbackTaskId)
		}
		fallbackTaskId = null
	}

	function publishLayoutMetrics() {
		const reads = frameLayoutReads
		const writes = frameLayoutWrites
		frameLayoutReads = 0
		frameLayoutWrites = 0

		if (reads !== lastPublishedReads) {
			layoutReads.value = reads
			lastPublishedReads = reads
		}

		if (writes !== lastPublishedWrites) {
			layoutWrites.value = writes
			lastPublishedWrites = writes
		}
	}

	function updateTiming(delta: number) {
		if (delta <= 0) return
		frameTime.value = delta
		const instantaneousFps = Math.min(MAX_FPS, 1000 / delta)
		fpsEma = fpsEma === 0 ? instantaneousFps : fpsEma + FPS_EMA_ALPHA * (instantaneousFps - fpsEma)
		const smoothedFps = Math.min(MAX_FPS, fpsEma)
		if (fps.value !== smoothedFps) {
			fps.value = smoothedFps
		}

		const droppedFramesThisTick =
			delta > FRAME_DURATION_MS ? Math.max(1, Math.ceil(delta / FRAME_DURATION_MS) - 1) : 0
		if (droppedFramesThisTick > 0) {
			droppedFrameCount += droppedFramesThisTick
		}
		if (droppedFrames.value !== droppedFrameCount) {
			droppedFrames.value = droppedFrameCount
		}
	}

	function handleFrame(timestamp: number) {
		rafHandle = null
		fallbackTaskId = null
		if (!monitoring || !debugMode.value) {
			stopMonitoring()
			return
		}

		if (lastTimestamp !== null) {
			const delta = Math.max(0, timestamp - lastTimestamp)
			updateTiming(delta)
		}
		lastTimestamp = timestamp
		publishLayoutMetrics()
		publishPerformanceMetrics(timestamp)
		scheduleFrameLoop()
	}

	function publishPerformanceMetrics(nowTimestamp: number) {
		const elapsed = Math.max(1, nowTimestamp - lastPerformancePublish)
		const rate = (count: number): number => (count <= 0 ? 0 : (count * 1000) / elapsed)
		const nextSyncRate = rate(syncScrollCount)
		const nextHeavyRate = rate(heavyPassCount)
		const nextVirtualizerUpdates = rate(virtualizerUpdateCount)
		const nextVirtualizerSkips = rate(virtualizerSkipCount)

		if (syncScrollRate.value !== nextSyncRate) {
			syncScrollRate.value = nextSyncRate
		}
		if (heavyUpdateRate.value !== nextHeavyRate) {
			heavyUpdateRate.value = nextHeavyRate
		}
		if (virtualizerUpdates.value !== nextVirtualizerUpdates) {
			virtualizerUpdates.value = nextVirtualizerUpdates
		}
		if (virtualizerSkips.value !== nextVirtualizerSkips) {
			virtualizerSkips.value = nextVirtualizerSkips
		}

		syncScrollCount = 0
		heavyPassCount = 0
		virtualizerUpdateCount = 0
		virtualizerSkipCount = 0
		lastPerformancePublish = nowTimestamp
	}

	function stopMonitoring() {
		if (!monitoring) return
		monitoring = false
		cancelScheduledFrame()
		lastTimestamp = null
		fpsEma = 0
		droppedFrameCount = 0
		frameLayoutReads = 0
		frameLayoutWrites = 0
		lastPublishedReads = 0
		lastPublishedWrites = 0
		syncScrollCount = 0
		heavyPassCount = 0
		virtualizerUpdateCount = 0
		virtualizerSkipCount = 0
		lastPerformancePublish = clock.now()
		fps.value = 0
		frameTime.value = 0
		droppedFrames.value = 0
		layoutReads.value = 0
		layoutWrites.value = 0
		syncScrollRate.value = 0
		heavyUpdateRate.value = 0
		virtualizerUpdates.value = 0
		virtualizerSkips.value = 0
	}

	function startMonitoring() {
		if (monitoring) return
		monitoring = true
		lastTimestamp = null
		fpsEma = 0
		droppedFrameCount = 0
		fps.value = 0
		frameTime.value = 0
		droppedFrames.value = 0
		syncScrollCount = 0
		heavyPassCount = 0
		virtualizerUpdateCount = 0
		virtualizerSkipCount = 0
		lastPerformancePublish = clock.now()
		scheduleFrameLoop()
	}

	function desiredDebugState(): boolean {
		return runtimeOverride ?? debugDefault
	}

	function applyDebugState() {
		const next = desiredDebugState()
		if (debugMode.value !== next) {
			debugMode.value = next
		}
		if (next) {
			startMonitoring()
		} else {
			stopMonitoring()
		}
	}

	function setDebugMode(enabled: boolean) {
		const normalized = Boolean(enabled)
		if (runtimeOverride === normalized && debugMode.value === normalized) return
		runtimeOverride = normalized
		applyDebugState()
	}

	function isDebugEnabled() {
		return debugMode.value
	}

	function dispose() {
		runtimeOverride = false
		stopMonitoring()
	}

	applyDebugState()

	return {
		recordLayoutRead,
		recordLayoutWrite,
		recordSyncScroll,
		recordHeavyPass,
		recordVirtualizerUpdate,
		recordVirtualizerSkip,
		setDebugMode,
		isDebugEnabled,
		dispose,
	}
}
