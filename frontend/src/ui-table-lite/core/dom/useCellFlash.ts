type FlashType = "copy" | "paste" | "fill" | "undo" | "redo"

export interface CellFlashController {
  flash(cellElements: HTMLElement[], type: FlashType): void
}

export function useCellFlash(): CellFlashController {
  function flash(cellElements: HTMLElement[], type: FlashType) {
    const classMap: Record<FlashType, string> = {
      copy: "cell-flash-copy",
      paste: "cell-flash-paste",
      fill: "cell-flash-fill",
      undo: "cell-flash-undo",
      redo: "cell-flash-redo",
    }

    const className = classMap[type]
    if (!className) return

    cellElements.forEach(cell => {
      cell.classList.remove(className)
      void cell.offsetWidth
      cell.classList.add(className)
      setTimeout(() => cell.classList.remove(className), 500)
    })
  }

  return { flash }
}
