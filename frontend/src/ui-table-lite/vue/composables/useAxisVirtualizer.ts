import { shallowRef } from "vue"
import type { Ref } from "vue"
import type { UiTableColumn } from "../../core/types"
import {
  createAxisVirtualizer as createCoreAxisVirtualizer,
  type AxisOrientation,
  type AxisVirtualizerContext,
  type AxisVirtualizerInternalContext,
  type AxisVirtualizerRange,
  type AxisVirtualizerState,
  type AxisVirtualizerStrategy,
} from "../../core/virtualization/axisVirtualizer"
import {
  createVerticalAxisStrategy,
  type VerticalVirtualizerMeta as CoreVerticalVirtualizerMeta,
  type VerticalVirtualizerPayload,
} from "../../core/virtualization/verticalVirtualizer"
import {
  createHorizontalAxisStrategy,
  type HorizontalVirtualizerMeta as CoreHorizontalVirtualizerMeta,
  type HorizontalVirtualizerPayload,
} from "../../core/virtualization/horizontalVirtualizer"

export type {
  AxisOrientation,
  AxisVirtualizerContext,
  AxisVirtualizerInternalContext,
  AxisVirtualizerRange,
  AxisVirtualizerState,
  AxisVirtualizerStrategy,
}
export type { ColumnMetric } from "../core/virtualization/types"
export type { ColumnPinMode } from "../../core/virtualization/types"

export interface AxisVirtualizer<TMeta, TPayload> {
  state: Ref<AxisVirtualizerState<TPayload>>
  update(context: AxisVirtualizerContext<TMeta>): AxisVirtualizerState<TPayload>
  getOffsetForIndex(index: number, context: AxisVirtualizerInternalContext<TMeta>): number
  isIndexVisible(index: number): boolean
}

export function useAxisVirtualizer<TMeta, TPayload>(
  axis: AxisOrientation,
  strategy: AxisVirtualizerStrategy<TMeta, TPayload>,
  initialPayload: TPayload,
): AxisVirtualizer<TMeta, TPayload> {
  const coreVirtualizer = createCoreAxisVirtualizer(axis, strategy, initialPayload)
  const state = shallowRef<AxisVirtualizerState<TPayload>>(coreVirtualizer.getState())

  function update(context: AxisVirtualizerContext<TMeta>): AxisVirtualizerState<TPayload> {
    const nextState = coreVirtualizer.update(context)
    state.value = nextState
    return nextState
  }

  return {
    state,
    update,
    getOffsetForIndex(index, context) {
      return coreVirtualizer.getOffsetForIndex(index, context)
    },
    isIndexVisible(index) {
      return coreVirtualizer.isIndexVisible(index)
    },
  }
}

export interface VerticalVirtualizerMeta extends CoreVerticalVirtualizerMeta {
  container: HTMLDivElement | null
}

export function createVerticalVirtualizer() {
  const strategy = createVerticalAxisStrategy() as AxisVirtualizerStrategy<VerticalVirtualizerMeta, VerticalVirtualizerPayload>
  return useAxisVirtualizer<VerticalVirtualizerMeta, VerticalVirtualizerPayload>(
    "vertical",
    strategy,
    undefined,
  )
}

export interface HorizontalVirtualizerMeta
  extends CoreHorizontalVirtualizerMeta<UiTableColumn> {
  container: HTMLDivElement | null
}

export type { HorizontalVirtualizerPayload }
  from "../../core/virtualization/horizontalVirtualizer"

export function createHorizontalVirtualizer() {
  const strategy = createHorizontalAxisStrategy<UiTableColumn>() as AxisVirtualizerStrategy<
    HorizontalVirtualizerMeta,
    HorizontalVirtualizerPayload
  >
  return useAxisVirtualizer<HorizontalVirtualizerMeta, HorizontalVirtualizerPayload>(
    "horizontal",
    strategy,
    {
      visibleStart: 0,
      visibleEnd: 0,
      leftPadding: 0,
      rightPadding: 0,
      totalScrollableWidth: 0,
      visibleScrollableWidth: 0,
      averageWidth: 0,
      scrollSpeed: 0,
      effectiveViewport: 0,
    },
  )
}
