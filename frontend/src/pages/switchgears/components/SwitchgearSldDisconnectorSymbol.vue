<script setup lang="ts">
type SymbolState = "CLOSED" | "OPEN" | "INTERMEDIATE" | "UNKNOWN"

const props = defineProps<{
  x: number
  y: number
  width: number
  height: number
  state: SymbolState
  background: string
  stroke: string
  selected?: boolean
  unconfigured?: boolean
}>()
</script>

<template>
  <g class="switchgear-sld-node-symbol switchgear-sld-node-symbol--disconnector" :class="{ 'switchgear-sld-node-symbol--selected': props.selected }" fill="none" stroke-linecap="round" stroke-linejoin="round" :stroke-dasharray="props.unconfigured ? '4 3' : undefined">
    <rect :x="props.x + 1" :y="props.y + 1" :width="props.width - 2" :height="props.height - 2" rx="8" :fill="props.background" stroke="none" />
    <line :x1="props.x + 7" :y1="props.y + 8" :x2="props.x + 7" :y2="props.y + props.height - 8" :stroke="props.stroke" stroke-width="2.5" />
    <line :x1="props.x + props.width - 7" :y1="props.y + 8" :x2="props.x + props.width - 7" :y2="props.y + props.height - 8" :stroke="props.stroke" stroke-width="2.5" />
    <line v-if="props.state === 'CLOSED'" :x1="props.x + 7" :y1="props.y + props.height / 2" :x2="props.x + props.width - 7" :y2="props.y + props.height / 2" :stroke="props.stroke" stroke-width="2.5" />
    <line v-else-if="props.state === 'OPEN' || props.state === 'INTERMEDIATE'" :x1="props.x + 11" :y1="props.y + props.height - 11" :x2="props.x + props.width - 11" :y2="props.y + 11" :stroke="props.stroke" stroke-width="2.5" />
    <text v-else :x="props.x + props.width / 2" :y="props.y + props.height / 2 + 7" text-anchor="middle" font-size="22" font-weight="700" :fill="props.stroke" stroke="none">?</text>
  </g>
</template>
