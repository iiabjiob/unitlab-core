import type { SelectionOverlayRect } from "@/ui-table/core/selection/selectionOverlay"
import type { FillHandleStylePayload } from "@/ui-table/core/selection/fillHandleStylePool"

export type UiTableOverlayRect = SelectionOverlayRect

export interface UiTableOverlayRectGroups {
  selection?: readonly UiTableOverlayRect[]
  activeSelection?: readonly UiTableOverlayRect[]
  fillPreview?: readonly UiTableOverlayRect[]
  cutPreview?: readonly UiTableOverlayRect[]
  cursor?: UiTableOverlayRect | null
}

export interface UiTableOverlayTransformInput {
  viewportWidth: number
  viewportHeight: number
  pinnedLeftTranslateX: number
  pinnedRightTranslateX: number
}

export interface UiTableOverlayHandle {
  overlayRef: HTMLDivElement | null
  viewportRef: HTMLDivElement | null
  updateRects(payload: UiTableOverlayRectGroups): void
  updateTransforms(snapshot: UiTableOverlayTransformInput): void
  updateScrollTransform(scrollTop: number, scrollLeft: number): void
  updateFillHandleStyle(style: FillHandleStylePayload | null | undefined): void
}
