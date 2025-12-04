export interface FillHandleStylePayload {
  width: string
  height: string
  left: string
  top: string
  x: number
  y: number
  widthValue: number
  heightValue: number
}

type MutableFillHandleStyle = FillHandleStylePayload

const pooledStyles = new WeakSet<MutableFillHandleStyle>()
const stylesInPool = new WeakSet<MutableFillHandleStyle>()

function createFillHandleStyle(): MutableFillHandleStyle {
  const style: MutableFillHandleStyle = {
    width: "",
    height: "",
    left: "",
    top: "",
    x: 0,
    y: 0,
    widthValue: 0,
    heightValue: 0,
  }
  pooledStyles.add(style)
  return style
}

function resetFillHandleStyle(style: MutableFillHandleStyle): void {
  style.width = ""
  style.height = ""
  style.left = ""
  style.top = ""
  style.x = 0
  style.y = 0
  style.widthValue = 0
  style.heightValue = 0
}

class FillHandleStylePool {
  private readonly pool: MutableFillHandleStyle[] = []

  acquire(): MutableFillHandleStyle {
    const style = this.pool.pop()
    if (style) {
      stylesInPool.delete(style)
      return style
    }
    return createFillHandleStyle()
  }

  release(style: MutableFillHandleStyle): void {
    if (stylesInPool.has(style)) {
      return
    }
    resetFillHandleStyle(style)
    stylesInPool.add(style)
    this.pool.push(style)
  }

  clear(): void {
    this.pool.length = 0
  }
}

const fillHandleStylePool = new FillHandleStylePool()

function asMutableFillHandleStyle(style: unknown): MutableFillHandleStyle | null {
  if (!style || typeof style !== "object") {
    return null
  }
  if (pooledStyles.has(style as MutableFillHandleStyle)) {
    return style as MutableFillHandleStyle
  }
  return null
}

export function acquireFillHandleStyle(): FillHandleStylePayload {
  return fillHandleStylePool.acquire()
}

export function releaseFillHandleStyle(style: FillHandleStylePayload | null | undefined): void {
  const mutable = asMutableFillHandleStyle(style)
  if (!mutable) {
    return
  }
  fillHandleStylePool.release(mutable)
}

export function resetFillHandleStylePool(): void {
  fillHandleStylePool.clear()
}
