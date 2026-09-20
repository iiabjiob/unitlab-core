<script setup lang="ts">
type SymbolState = "CLOSED" | "OPEN" | "INTERMEDIATE" | "UNKNOWN"

const props = defineProps<{
  x: number
  y: number
  width: number
  height: number
  state: SymbolState
  fill: string
  background: string
  stroke: string
  selected?: boolean
}>()
</script>

<template>
  <g class="switchgear-sld-node-symbol switchgear-sld-node-symbol--breaker" :class="{ 'switchgear-sld-node-symbol--selected': props.selected }">
    <rect :x="props.x + 1" :y="props.y + 1" :width="props.width - 2" :height="props.height - 2" rx="8" :fill="props.background" stroke="none" />
    <rect :x="props.x + 4" :y="props.y + 4" :width="props.width - 8" :height="props.height - 8" rx="6" :fill="props.fill" :stroke="props.stroke" stroke-width="2.5" />
    <line v-if="props.state === 'INTERMEDIATE'" :x1="props.x + 11" :y1="props.y + props.height - 11" :x2="props.x + props.width - 11" :y2="props.y + 11" :stroke="props.stroke" />
    <text v-else-if="props.state === 'UNKNOWN'" :x="props.x + props.width / 2" :y="props.y + props.height / 2 + 7" text-anchor="middle" font-size="22" font-weight="700" :fill="props.stroke" stroke="none">?</text>
  </g>
</template>
