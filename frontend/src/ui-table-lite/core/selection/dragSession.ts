import {
  clampGridSelectionPoint,
  type GridSelectionContext,
  type GridSelectionPoint,
  type GridSelectionPointLike,
} from "./selectionState"

export interface PointerEventSnapshot {
  clientX: number
  clientY: number
  buttons?: number
}

export interface SelectionDragSession<TRowKey = unknown> {
  anchor: GridSelectionPoint<TRowKey>
  lastEvent: PointerEventSnapshot | null
  isActive: boolean
}

export function startSelectionDragSession<TRowKey = unknown>(
  input: {
    anchorPoint: GridSelectionPointLike<TRowKey>
    context: GridSelectionContext<TRowKey>
  },
): SelectionDragSession<TRowKey> {
  const anchor = clampGridSelectionPoint(input.anchorPoint, input.context)
  return {
    anchor,
    lastEvent: null,
    isActive: true,
  }
}

export function updateSelectionDragSession<TRowKey = unknown>(
  session: SelectionDragSession<TRowKey>,
  input: {
    clientX: number
    clientY: number
    resolvePoint: (clientX: number, clientY: number) => GridSelectionPointLike<TRowKey> | null
    context: GridSelectionContext<TRowKey>
  },
): GridSelectionPoint<TRowKey> | null {
  const { resolvePoint, context } = input
  session.lastEvent = {
    clientX: input.clientX,
    clientY: input.clientY,
  }
  const point = resolvePoint(input.clientX, input.clientY)
  if (!point) {
    return null
  }
  return clampGridSelectionPoint(point, context)
}

export function endSelectionDragSession<TRowKey = unknown>(session: SelectionDragSession<TRowKey>) {
  session.isActive = false
  session.lastEvent = null
}
