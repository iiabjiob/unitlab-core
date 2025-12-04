import { ROW_POOL_OVERSCAN, SCROLL_EDGE_PADDING, VIRTUALIZATION_BUFFER } from "../utils/constants"

export interface ViewportFrameBudget {
  frameDurationMs: number
  minVelocitySampleMs: number
  teleportMultiplier: number
}

export interface AxisVirtualizationConstants {
  scrollEpsilon: number
  minOverscan: number
  edgePadding: number
  velocityOverscanRatio: number
  viewportOverscanRatio: number
  overscanDecay: number
  maxViewportMultiplier: number
}

export interface TableVirtualizationConstants {
  frame: ViewportFrameBudget
  vertical: AxisVirtualizationConstants
  horizontal: AxisVirtualizationConstants
}

const defaultFrameBudget: ViewportFrameBudget = Object.freeze({
  frameDurationMs: 16.7,
  minVelocitySampleMs: 8,
  teleportMultiplier: 2.5,
})

const defaultVerticalConstants: AxisVirtualizationConstants = Object.freeze({
  scrollEpsilon: 1.25,
  minOverscan: ROW_POOL_OVERSCAN + VIRTUALIZATION_BUFFER,
  edgePadding: SCROLL_EDGE_PADDING,
  velocityOverscanRatio: 0.85,
  viewportOverscanRatio: 0.55,
  overscanDecay: 0.58,
  maxViewportMultiplier: 3,
})

const defaultHorizontalConstants: AxisVirtualizationConstants = Object.freeze({
  scrollEpsilon: 2.25,
  minOverscan: 2,
  edgePadding: SCROLL_EDGE_PADDING,
  velocityOverscanRatio: 0.9,
  viewportOverscanRatio: 0.75,
  overscanDecay: 0.58,
  maxViewportMultiplier: 3,
})

export const DEFAULT_TABLE_VIRTUALIZATION_CONSTANTS: TableVirtualizationConstants = Object.freeze({
  frame: defaultFrameBudget,
  vertical: defaultVerticalConstants,
  horizontal: defaultHorizontalConstants,
})

export const FRAME_BUDGET_CONSTANTS = DEFAULT_TABLE_VIRTUALIZATION_CONSTANTS.frame
export const VERTICAL_VIRTUALIZATION_CONSTANTS = DEFAULT_TABLE_VIRTUALIZATION_CONSTANTS.vertical
export const HORIZONTAL_VIRTUALIZATION_CONSTANTS = DEFAULT_TABLE_VIRTUALIZATION_CONSTANTS.horizontal
