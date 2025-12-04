import type { ViewportSyncState, ViewportSyncTargets } from "./tableViewportTypes"

export interface ViewportSyncAdapter {
  getSyncTargets(): ViewportSyncTargets | null
  getAppliedState(): ViewportSyncState
}

function isAdapter(target: unknown): target is ViewportSyncAdapter {
  if (!target) {
    return false
  }
  const candidate = target as ViewportSyncAdapter
  return (
    typeof candidate.getSyncTargets === "function" &&
    typeof candidate.getAppliedState === "function"
  )
}

function setTransform(target: HTMLElement | null | undefined, value: string): void {
  if (!target) return
  if (target.style.transform !== value) {
    target.style.transform = value
  }
}

function formatAxis(value: number): string {
  if (Math.abs(value) < 0.0001) {
    return "0"
  }
  return `${value}px`
}

function shouldUpdate(current: number, next: number): boolean {
  // Small epsilon to avoid sub-pixel noise leading to redundant DOM updates
  return Math.abs(current - next) > 0.01
}

function applyTransforms(
  targets: ViewportSyncTargets | null,
  state: ViewportSyncState,
  next: ViewportSyncState,
): void {
  if (!targets) {
    // No targets yet, just update the state snapshot
    state.scrollLeft = next.scrollLeft
    state.scrollTop = next.scrollTop
    state.pinnedOffsetLeft = next.pinnedOffsetLeft
    state.pinnedOffsetRight = next.pinnedOffsetRight
    return
  }

  const {
    bodyLayer,
    headerLayer,
    pinnedLeftLayer,
    pinnedRightLayer,
    overlayRoot,
  } = targets

  const scrollChanged =
    shouldUpdate(state.scrollLeft, next.scrollLeft) ||
    shouldUpdate(state.scrollTop, next.scrollTop)

  const pinnedLeftChanged =
    scrollChanged || shouldUpdate(state.pinnedOffsetLeft, next.pinnedOffsetLeft)

  const pinnedRightChanged =
    scrollChanged || shouldUpdate(state.pinnedOffsetRight, next.pinnedOffsetRight)

  if (scrollChanged) {
    const scrollX = -next.scrollLeft
    const scrollY = -next.scrollTop

    // Body and overlay share exactly the same transform (Google Sheets style)
    const bodyAndOverlayTransform = `translate3d(${scrollX}px, ${scrollY}px, 0)`
    setTransform(bodyLayer, bodyAndOverlayTransform)
    setTransform(overlayRoot, bodyAndOverlayTransform)

    // Header only scrolls horizontally, it stays pinned to the top
    const headerTransform = `translate3d(${scrollX}px, 0px, 0)`
    setTransform(headerLayer, headerTransform)

    if (headerLayer) {
      if (headerLayer.scrollLeft !== next.scrollLeft) {
        headerLayer.scrollLeft = next.scrollLeft
      }
      const headerScrollHost = headerLayer.parentElement
      if (headerScrollHost && headerScrollHost.scrollLeft !== next.scrollLeft) {
        headerScrollHost.scrollLeft = next.scrollLeft
      }
    }
  }

  if (pinnedLeftChanged) {
    // Pinned left: only vertical scroll is applied via transform.
    // Horizontal position comes from layout / pinnedOffsetLeft.
    const leftTransform = `translate3d(${formatAxis(next.pinnedOffsetLeft)}, ${formatAxis(-next.scrollTop)}, 0)`
    setTransform(pinnedLeftLayer, leftTransform)
  }

  if (pinnedRightChanged) {
    // Pinned right: same idea, but offset is negative on X side.
    const rightTransform = `translate3d(${formatAxis(-next.pinnedOffsetRight)}, ${formatAxis(-next.scrollTop)}, 0)`
    setTransform(pinnedRightLayer, rightTransform)
  }

  // Commit applied state snapshot (used to decide future updates)
  state.scrollLeft = next.scrollLeft
  state.scrollTop = next.scrollTop
  state.pinnedOffsetLeft = next.pinnedOffsetLeft
  state.pinnedOffsetRight = next.pinnedOffsetRight
}

export function applyViewportSyncTransforms(
  adapter: ViewportSyncAdapter | null,
  next: ViewportSyncState,
): void
export function applyViewportSyncTransforms(
  targets: ViewportSyncTargets | null,
  state: ViewportSyncState,
  next: ViewportSyncState,
): void
export function applyViewportSyncTransforms(
  adapterOrTargets: ViewportSyncAdapter | ViewportSyncTargets | null,
  argB: ViewportSyncState,
  argC?: ViewportSyncState,
): void {
  if (isAdapter(adapterOrTargets)) {
    const adapter = adapterOrTargets
    const targets = adapter.getSyncTargets()
    const state = adapter.getAppliedState()
    applyTransforms(targets, state, argB)
    return
  }

  const targets = adapterOrTargets as ViewportSyncTargets | null
  const state = argB
  const next = argC ?? argB
  applyTransforms(targets, state, next)
}

export function resetViewportSyncTransforms(
  targets: ViewportSyncTargets | null,
  state: ViewportSyncState,
): void {
  // Reset visual transforms to identity while keeping the API symmetrical
  const identity = "translate3d(0, 0, 0)"

  if (targets) {
    const { bodyLayer, headerLayer, pinnedLeftLayer, pinnedRightLayer, overlayRoot } = targets
    setTransform(bodyLayer, identity)
    setTransform(headerLayer, identity)
    setTransform(pinnedLeftLayer, identity)
    setTransform(pinnedRightLayer, identity)
    setTransform(overlayRoot, identity)
  }

  state.scrollLeft = 0
  state.scrollTop = 0
  state.pinnedOffsetLeft = 0
  state.pinnedOffsetRight = 0
}
