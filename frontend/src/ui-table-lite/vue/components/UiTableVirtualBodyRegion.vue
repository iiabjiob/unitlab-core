<template>
  <div ref="containerRef" class="ui-table__virtual-container" :style="virtualContainerStyle">
    <template v-for="pooled in pooledRows" :key="pooled.poolIndex">
      <div
        class="ui-table__row-layer"
        :data-testid="props.section === 'main' ? 'ui-table-row-layer' : null"
        :style="rowLayerInlineStyle(pooled)"
      >
        <UiTableRowSurface
          v-if="renderableEntries.length"
          :pooled="pooled"
          :entries="renderableEntries"
          :region="rowRegion"
        />
      </div>
    </template>
  </div>
</template>

<script setup lang="ts">
import { computed, inject, ref } from "vue"
import type { CSSProperties } from "vue"
import UiTableRowSurface from "./UiTableRowSurface.vue"
import { UiTableBodyContextKey } from "../context"
import type { RowPoolItem } from "../composables/useTableViewport"

const props = defineProps<{
  section: "pinned-left" | "main" | "pinned-right"
}>()

const body = inject(UiTableBodyContextKey)
if (!body) {
  throw new Error("UiTableVirtualBodyRegion requires UiTableBodyContext")
}

const pooledRows = computed(() => body.pooledRows.value)

const renderableEntries = computed(() => {
  if (props.section === "pinned-left") {
    return body.headerPinnedLeftEntries.value
  }
  if (props.section === "pinned-right") {
    return body.headerPinnedRightEntries.value
  }
  return body.headerMainEntries.value.length ? body.headerMainEntries.value : body.headerRenderableEntries.value
})

const rowRegion = computed(() => {
  if (props.section === "main") {
    return "center" as const
  }
  return props.section
})

const sectionWidth = computed(() => {
  const region = rowRegion.value
  const rawWidth = body.getRegionSurfaceWidth(region)
  const normalizedWidth = rawWidth > 0 ? rawWidth : 0

  if (region === "center") {
    const viewportWidth = body.viewportWidthDom?.value ?? 0
    const effectiveViewport = viewportWidth > 0 ? viewportWidth : 0
    const effectiveWidth = Math.max(normalizedWidth, effectiveViewport)
    return effectiveWidth > 0 ? effectiveWidth : null
  }

  return normalizedWidth > 0 ? normalizedWidth : null
})

const virtualContainerStyle = computed<CSSProperties>(() => {
  const base = body.virtualContainerStyle.value
  const width = sectionWidth.value
  if (width == null) {
    return base
  }
  const widthPx = `${width}px`
  return {
    ...base,
    width: widthPx,
    minWidth: widthPx,
  }
})

const containerRef = ref<HTMLDivElement | null>(null)

const rowLayerInlineStyle = (pooled: RowPoolItem): CSSProperties => {
  return body.rowLayerStyle(pooled) as CSSProperties
}
</script>
