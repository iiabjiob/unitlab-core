import { shallowRef } from "vue"
import type { ShallowRef } from "vue"

export interface TableOverlayScrollSnapshot {
  viewportWidth: number
  viewportHeight: number
  scrollLeft: number
  scrollTop: number
  pinnedOffsetLeft: number
  pinnedOffsetRight: number
}

export interface TableOverlayScrollEmitter {
  snapshot: ShallowRef<TableOverlayScrollSnapshot>
  emit(next: Partial<TableOverlayScrollSnapshot>): void
  subscribe(callback: (snapshot: TableOverlayScrollSnapshot) => void): () => void
}

const DEFAULT_SNAPSHOT: TableOverlayScrollSnapshot = {
  viewportWidth: 0,
  viewportHeight: 0,
  scrollLeft: 0,
  scrollTop: 0,
  pinnedOffsetLeft: 0,
  pinnedOffsetRight: 0,
}

const FLOAT_EPSILON = 1e-4

function normalizeFinite(value: number | null | undefined): number {
  if (!Number.isFinite(value ?? NaN)) {
    return 0
  }
  return value as number
}

function equalWithinEpsilon(a: number, b: number): boolean {
  return Math.abs(a - b) <= FLOAT_EPSILON
}

function sanitizeSnapshot(partial: Partial<TableOverlayScrollSnapshot>): TableOverlayScrollSnapshot {
  return {
    viewportWidth: Math.max(0, normalizeFinite(partial.viewportWidth ?? 0)),
    viewportHeight: Math.max(0, normalizeFinite(partial.viewportHeight ?? 0)),
    scrollLeft: normalizeFinite(partial.scrollLeft ?? 0),
    scrollTop: normalizeFinite(partial.scrollTop ?? 0),
    pinnedOffsetLeft: Math.max(0, normalizeFinite(partial.pinnedOffsetLeft ?? 0)),
    pinnedOffsetRight: Math.max(0, normalizeFinite(partial.pinnedOffsetRight ?? 0)),
  }
}

export function createTableOverlayScrollEmitter(
  initial?: Partial<TableOverlayScrollSnapshot>,
): TableOverlayScrollEmitter {
  const initialSnapshot = sanitizeSnapshot({ ...DEFAULT_SNAPSHOT, ...(initial ?? {}) })
  const snapshot = shallowRef<TableOverlayScrollSnapshot>({ ...initialSnapshot })
  const subscribers = new Set<(snapshot: TableOverlayScrollSnapshot) => void>()

  const emit = (next: Partial<TableOverlayScrollSnapshot>) => {
    const current = snapshot.value
    const sanitized = sanitizeSnapshot({ ...current, ...next })
    if (
      equalWithinEpsilon(current.viewportWidth, sanitized.viewportWidth) &&
      equalWithinEpsilon(current.viewportHeight, sanitized.viewportHeight) &&
      equalWithinEpsilon(current.scrollLeft, sanitized.scrollLeft) &&
      equalWithinEpsilon(current.scrollTop, sanitized.scrollTop) &&
      equalWithinEpsilon(current.pinnedOffsetLeft, sanitized.pinnedOffsetLeft) &&
      equalWithinEpsilon(current.pinnedOffsetRight, sanitized.pinnedOffsetRight)
    ) {
      return
    }
    snapshot.value = sanitized
    subscribers.forEach(callback => {
      callback(sanitized)
    })
  }

  const subscribe = (callback: (snapshot: TableOverlayScrollSnapshot) => void) => {
    subscribers.add(callback)
    callback(snapshot.value)
    return () => {
      subscribers.delete(callback)
    }
  }

  return {
    snapshot,
    emit,
    subscribe,
  }
}
