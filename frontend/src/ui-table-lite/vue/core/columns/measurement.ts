const AUTO_RESIZE_PADDING = 12
const SELECT_TRIGGER_BUFFER = 24

let measurementCanvas: HTMLCanvasElement | null = null
let measurementContext: CanvasRenderingContext2D | null = null

function getMeasurementContext() {
  if (typeof document === "undefined") {
    return null
  }

  if (!measurementCanvas) {
    measurementCanvas = document.createElement("canvas")
  }

  if (!measurementContext) {
    measurementContext = measurementCanvas.getContext("2d")
  }

  return measurementContext
}

function parseSize(value: string | null | undefined) {
  const parsed = Number.parseFloat(value ?? "")
  return Number.isFinite(parsed) ? parsed : 0
}

function resolveGap(style: CSSStyleDeclaration) {
  const columnGap = parseSize(style.columnGap)
  if (columnGap) return columnGap
  const gapValue = style.gap ?? "0"
  if (!gapValue) return 0
  const [first] = gapValue.split(" ")
  return parseSize(first)
}

function findContentTargets(element: HTMLElement) {
  const targets = element.querySelectorAll<HTMLElement>("[data-auto-resize-target]")
  if (targets.length) return Array.from(targets)
  return [element]
}

function applyTextTransform(text: string, transform: string) {
  switch (transform) {
    case "uppercase":
      return text.toLocaleUpperCase()
    case "lowercase":
      return text.toLocaleLowerCase()
    case "capitalize":
      return text.replace(/(^|\s)\p{L}/gu, match => match.toLocaleUpperCase())
    default:
      return text
  }
}

function measureTextContent(element: HTMLElement) {
  const rawText = element.textContent ?? ""
  const text = rawText.trim()
  if (!text) {
    return Math.max(element.scrollWidth, element.getBoundingClientRect().width)
  }

  const ctx = getMeasurementContext()
  if (!ctx) {
    return Math.max(element.scrollWidth, element.getBoundingClientRect().width)
  }

  const style = window.getComputedStyle(element)
  const fontParts = [
    style.fontStyle,
    style.fontVariant,
    style.fontWeight,
    style.fontStretch,
    style.fontSize,
    style.fontFamily,
  ].filter(Boolean)

  ctx.font = fontParts.join(" ") || ctx.font

  const transformedText = applyTextTransform(text, style.textTransform)
  let width = ctx.measureText(transformedText).width
  const letterSpacing = parseSize(style.letterSpacing)
  if (letterSpacing) {
    width += letterSpacing * Math.max(0, transformedText.length - 1)
  }

  const wordSpacing = parseSize(style.wordSpacing)
  if (wordSpacing && transformedText.trim()) {
    const wordCount = Math.max(0, transformedText.trim().split(/\s+/u).length - 1)
    width += wordSpacing * wordCount
  }

  return width
}

function measureBaseWidth(element: HTMLElement) {
  const style = window.getComputedStyle(element)
  const padding = parseSize(style.paddingLeft) + parseSize(style.paddingRight)
  const border = parseSize(style.borderLeftWidth) + parseSize(style.borderRightWidth)
  const contentTargets = findContentTargets(element)
  let contentWidth = 0

  for (const target of contentTargets) {
    const targetStyle = window.getComputedStyle(target)
    const targetWidth = measureTextContent(target)
    const targetMargins = parseSize(targetStyle.marginLeft) + parseSize(targetStyle.marginRight)
    contentWidth = Math.max(contentWidth, targetWidth + targetMargins)
  }

  if (!contentWidth) {
    contentWidth = Math.max(element.scrollWidth, element.getBoundingClientRect().width)
  }

  return contentWidth + padding + border
}

export function measureBodyCellWidth(element: HTMLElement) {
  const base = measureBaseWidth(element)
  const accessory = element.querySelector<HTMLElement>(".ui-table__cell-select-trigger")
  let width = base + AUTO_RESIZE_PADDING

  if (accessory) {
    const accessoryStyle = window.getComputedStyle(accessory)
    const accessoryWidth =
      accessory.offsetWidth +
      parseSize(accessoryStyle.marginLeft) +
      parseSize(accessoryStyle.marginRight)
    width = Math.max(width, base + accessoryWidth + AUTO_RESIZE_PADDING)
  } else if (element.dataset.editorType === "select") {
    width = Math.max(width, base + SELECT_TRIGGER_BUFFER)
  }

  return width
}

export function measureHeaderWidth(element: HTMLElement) {
  let width = measureBaseWidth(element)
  const additionalTargets = element.querySelectorAll<HTMLElement>("[data-auto-resize-target]")

  for (const target of additionalTargets) {
    const targetStyle = window.getComputedStyle(target)
    const targetWidth =
      measureTextContent(target) + parseSize(targetStyle.marginLeft) + parseSize(targetStyle.marginRight)
    width = Math.max(width, targetWidth)
  }

  const menuButton = element.querySelector<HTMLElement>(".ui-table__icon-button")
  if (!menuButton) {
    return width + AUTO_RESIZE_PADDING
  }

  const buttonStyle = window.getComputedStyle(menuButton)
  const parentStyle = menuButton.parentElement ? window.getComputedStyle(menuButton.parentElement) : null
  const gap = parentStyle ? resolveGap(parentStyle) : 0
  const buttonWidth =
    menuButton.offsetWidth + parseSize(buttonStyle.marginLeft) + parseSize(buttonStyle.marginRight)

  return width + buttonWidth + gap + AUTO_RESIZE_PADDING
}
