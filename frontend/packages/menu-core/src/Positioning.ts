import type { Placement, PositionOptions, PositionResult, Rect } from "./Types"

const DEFAULT_VIEWPORT = 10_000

export function computePosition(
  anchor: Rect,
  panel: Rect,
  options: PositionOptions = {}
): PositionResult {
  const gutter = options.gutter ?? 6
  const viewportPadding = options.viewportPadding ?? 8
  const preferSide = options.preferSide ?? "right"
  const align = options.align ?? "start"
  const viewportWidth = options.viewportWidth ?? DEFAULT_VIEWPORT
  const viewportHeight = options.viewportHeight ?? DEFAULT_VIEWPORT

  const preferredLeft =
    preferSide === "right"
      ? anchor.x + anchor.width + gutter
      : anchor.x - panel.width - gutter

  const wouldOverflowRight = preferredLeft + panel.width > viewportWidth - viewportPadding
  const wouldOverflowLeft = preferredLeft < viewportPadding

  const placement: Placement =
    preferSide === "right"
      ? wouldOverflowRight && !wouldOverflowLeft
        ? "left"
        : "right"
      : wouldOverflowLeft && !wouldOverflowRight
        ? "right"
        : "left"

  let left =
    placement === "right"
      ? anchor.x + anchor.width + gutter
      : anchor.x - panel.width - gutter

  left = clamp(left, viewportPadding, viewportWidth - viewportPadding - panel.width)

  let top: number
  switch (align) {
    case "center":
      top = anchor.y + anchor.height / 2 - panel.height / 2
      break
    case "end":
      top = anchor.y + anchor.height - panel.height
      break
    default:
      top = anchor.y
  }

  top = clamp(top, viewportPadding, viewportHeight - viewportPadding - panel.height)

  return { left, top, placement }
}

function clamp(value: number, min: number, max: number) {
  if (min > max) return value
  return Math.min(Math.max(value, min), max)
}
