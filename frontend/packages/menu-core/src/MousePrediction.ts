import type { MousePredictionConfig, Point, Rect } from "./Types"

const DEFAULTS: Required<MousePredictionConfig> = {
  history: 8,
  verticalTolerance: 48,
  headingThreshold: 0.2,
}

export class MousePrediction {
  private readonly points: Point[] = []
  private readonly config: Required<MousePredictionConfig>

  constructor(config?: MousePredictionConfig) {
    this.config = { ...DEFAULTS, ...config }
  }

  push(point: Point) {
    const now = typeof performance !== "undefined" ? performance.now() : Date.now()
    this.points.push({ ...point, time: point.time ?? now })
    if (this.points.length > this.config.history) {
      this.points.shift()
    }
  }

  clear() {
    this.points.length = 0
  }

  isMovingToward(target: Rect, origin: Rect) {
    if (!target || !origin) return false
    if (this.points.length < 2) return false

    const last = this.points[this.points.length - 1]
    const prevIndex = Math.max(0, this.points.length - 4)
    const prev = this.points[prevIndex]
    const prevInstant = this.points[this.points.length - 2]

    const dx = last.x - prev.x
    const dy = last.y - prev.y
    const instantDx = last.x - prevInstant.x

    const opensRight = target.x >= origin.x + origin.width
    const directionSign = opensRight ? 1 : -1

    if (instantDx * directionSign < -2) return false

    const movementMag = Math.sqrt(dx * dx + dy * dy)
    if (movementMag === 0) return false

    const targetCenterX = target.x + target.width / 2
    const targetCenterY = target.y + target.height / 2
    const toCenterX = targetCenterX - last.x
    const toCenterY = targetCenterY - last.y
    const targetMag = Math.sqrt(toCenterX * toCenterX + toCenterY * toCenterY)

    const heading =
      targetMag === 0
        ? true
        : (dx * toCenterX + dy * toCenterY) / (movementMag * targetMag) >
          this.config.headingThreshold

    const insideVertical =
      last.y >= target.y - this.config.verticalTolerance &&
      last.y <= target.y + target.height + this.config.verticalTolerance

    const horizontalProgress = opensRight
      ? last.x >= origin.x + origin.width - 6
      : last.x <= origin.x + 6

    const driftBias = Math.abs(dx) > Math.abs(dy) * 0.4

    return insideVertical && (heading || horizontalProgress || driftBias)
  }
}

export function predictMouseDirection(
  points: Point[],
  target: Rect,
  origin: Rect,
  config?: MousePredictionConfig
) {
  const predictor = new MousePrediction(config)
  points.forEach((point) => predictor.push(point))
  return predictor.isMovingToward(target, origin)
}
