export function computeFloatingPosition(options: {
  anchorRect: DOMRect
  panelRect: DOMRect
  preferredSide: "bottom" | "right"
  offset?: number
  padding?: number
}) {
  const { anchorRect, panelRect, preferredSide, offset = 6, padding = 8 } = options

  let left = 0
  let top = 0

  if (preferredSide === "bottom") {
    const preferredTop = anchorRect.bottom + offset
    const alternativeTop = anchorRect.top - panelRect.height - offset

    const overflowBottom = preferredTop + panelRect.height > window.innerHeight - padding
    top = overflowBottom && alternativeTop >= padding ? alternativeTop : preferredTop

    left = anchorRect.left
  }

  if (preferredSide === "right") {
    let candidateLeft = anchorRect.right + offset
    const overflowRight = candidateLeft + panelRect.width > window.innerWidth - padding

    if (overflowRight) {
      candidateLeft = anchorRect.left - panelRect.width - offset
    }

    left = candidateLeft
    top = anchorRect.top
  }

  // Clamp
  left = Math.min(
    Math.max(left, padding),
    window.innerWidth - panelRect.width - padding
  )

  top = Math.min(
    Math.max(top, padding),
    window.innerHeight - panelRect.height - padding
  )

  return { left, top }
}
