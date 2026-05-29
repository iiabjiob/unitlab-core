<script setup lang="ts">
import { computed, ref } from "vue"
import { RouterLink } from "vue-router"

import {
  getSldBayLayoutTemplates,
  type SldBayLayoutTemplate,
  type SldBayLayoutTemplatePoint,
  type SldBayLayoutTemplateSlot,
  type SldBayLayoutTemplateWire,
} from "@/modules/scd-sld-core"

const templates = getSldBayLayoutTemplates()
const selectedTemplateId = ref(templates[0]?.id ?? "")

const unitSize = 34
const stagePadding = 56

const selectedTemplate = computed<SldBayLayoutTemplate | null>(() => (
  templates.find(template => template.id === selectedTemplateId.value) ?? templates[0] ?? null
))

const svgWidth = computed(() => (
  selectedTemplate.value ? selectedTemplate.value.grid.widthUnits * unitSize + stagePadding * 2 : 0
))

const svgHeight = computed(() => (
  selectedTemplate.value ? selectedTemplate.value.grid.heightUnits * unitSize + stagePadding * 2 : 0
))

const viewBox = computed(() => `0 0 ${svgWidth.value} ${svgHeight.value}`)

const propertyRows = computed(() => {
  const template = selectedTemplate.value
  if (!template) return []
  return [
    ["id", template.id],
    ["interpretation", template.interpretation],
    ["orientation", template.orientation],
    ["busbarCount", template.busbarCount],
    ["outgoingSide", template.outgoingSide],
    ["earthSwitchPlacement", template.earthSwitchPlacement],
    ["grid", `${template.grid.widthUnits} x ${template.grid.heightUnits}`],
  ]
})

const modelJson = computed(() => (
  selectedTemplate.value ? JSON.stringify(selectedTemplate.value, null, 2) : ""
))

function selectTemplate(template: SldBayLayoutTemplate) {
  selectedTemplateId.value = template.id
}

function isSelected(template: SldBayLayoutTemplate): boolean {
  return selectedTemplateId.value === template.id
}

function svgPoint(point: SldBayLayoutTemplatePoint): SldBayLayoutTemplatePoint {
  return {
    x: stagePadding + point.x * unitSize,
    y: stagePadding + point.y * unitSize,
  }
}

function polylinePoints(wire: SldBayLayoutTemplateWire): string {
  return wire.points
    .map(point => svgPoint(point))
    .map(point => `${point.x},${point.y}`)
    .join(" ")
}

function slotTransform(slot: SldBayLayoutTemplateSlot): string {
  const point = svgPoint(slot.point)
  return `translate(${point.x} ${point.y})`
}

function labelTransform(point: SldBayLayoutTemplatePoint): string {
  const resolved = svgPoint(point)
  return `translate(${resolved.x} ${resolved.y})`
}

function slotClass(slot: SldBayLayoutTemplateSlot): string {
  return `is-${slot.role.replace(/[A-Z]/g, match => `-${match.toLowerCase()}`)}`
}

function wireClass(wire: SldBayLayoutTemplateWire): string {
  return wire.weight === "bold" ? "is-bold" : "is-normal"
}
</script>

<template>
  <div class="iec61850-template-page">
    <header class="iec61850-template-page__header">
      <div class="iec61850-template-page__title-block">
        <p class="iec61850-template-page__eyebrow">IEC 61850</p>
        <h1 class="iec61850-template-page__title">Bay Template Preview</h1>
        <p class="iec61850-template-page__status">
          Inspect reusable SLD bay layout templates, slots, wires and unit coordinates.
        </p>
      </div>

      <RouterLink class="iec61850-template-page__nav-link" to="/61850-debug">
        Topology debug
      </RouterLink>
    </header>

    <main class="iec61850-template-page__workspace">
      <section class="iec61850-template-page__template-list" aria-label="Available SLD templates">
        <div class="iec61850-template-page__panel-header">
          <span>Templates</span>
          <span class="iec61850-template-page__panel-count">{{ templates.length }} available</span>
        </div>

        <div class="iec61850-template-page__template-items">
          <button
            v-for="template in templates"
            :key="template.id"
            type="button"
            class="iec61850-template-page__template-item"
            :class="{ 'is-selected': isSelected(template) }"
            @click="selectTemplate(template)"
          >
            <span class="iec61850-template-page__template-name">{{ template.name }}</span>
            <span class="iec61850-template-page__template-meta">
              {{ template.interpretation }} · {{ template.busbarCount }} bus
            </span>
          </button>
        </div>
      </section>

      <section class="iec61850-template-page__preview-panel" aria-label="Template visual preview">
        <div class="iec61850-template-page__panel-header">
          <span>{{ selectedTemplate?.name ?? "Preview" }}</span>
          <span v-if="selectedTemplate" class="iec61850-template-page__panel-count">
            {{ selectedTemplate.grid.widthUnits }} x {{ selectedTemplate.grid.heightUnits }} units
          </span>
        </div>

        <div v-if="selectedTemplate" class="iec61850-template-page__preview-stage">
          <svg
            class="iec61850-template-page__svg"
            :viewBox="viewBox"
            role="img"
            :aria-label="`${selectedTemplate.name} SLD layout preview`"
          >
            <defs>
              <pattern id="iec61850-template-grid" :width="unitSize" :height="unitSize" patternUnits="userSpaceOnUse">
                <circle cx="1.5" cy="1.5" r="1.5" class="iec61850-template-page__grid-dot" />
              </pattern>
            </defs>

            <rect class="iec61850-template-page__svg-background" x="0" y="0" :width="svgWidth" :height="svgHeight" />
            <rect class="iec61850-template-page__svg-grid" x="0" y="0" :width="svgWidth" :height="svgHeight" />

            <g class="iec61850-template-page__wires">
              <polyline
                v-for="wire in selectedTemplate.wires"
                :key="wire.id"
                :points="polylinePoints(wire)"
                class="iec61850-template-page__wire"
                :class="wireClass(wire)"
              />
            </g>

            <g
              v-for="slot in selectedTemplate.slots"
              :key="slot.id"
              class="iec61850-template-page__slot"
              :class="slotClass(slot)"
              :transform="slotTransform(slot)"
            >
              <path
                v-if="slot.role === 'feederTerminal'"
                class="iec61850-template-page__slot-shape"
                d="M 0 18 L 0 -18 M -8 -6 L 0 -18 L 8 -6"
              />
              <rect
                v-else-if="slot.role === 'circuitBreaker'"
                class="iec61850-template-page__slot-shape"
                x="-15"
                y="-15"
                width="30"
                height="30"
                rx="5"
              />
              <circle
                v-else-if="slot.role === 'earthSwitch'"
                class="iec61850-template-page__slot-shape"
                cx="0"
                cy="0"
                r="16"
              />
              <rect
                v-else
                class="iec61850-template-page__slot-shape"
                x="-15"
                y="-15"
                width="30"
                height="30"
              />

              <g v-if="slot.role === 'earthSwitch'" class="iec61850-template-page__ground-symbol" transform="translate(28 0)">
                <path d="M 0 -12 L 0 4 M -9 4 L 9 4 M -6 9 L 6 9 M -3 14 L 3 14" />
              </g>

              <text class="iec61850-template-page__slot-label" x="22" y="4">{{ slot.label }}</text>
              <text class="iec61850-template-page__slot-coordinate" x="22" y="18">
                {{ slot.point.x }}, {{ slot.point.y }}
              </text>
            </g>

            <g
              v-for="label in selectedTemplate.labels"
              :key="label.id"
              class="iec61850-template-page__template-label"
              :transform="labelTransform(label.point)"
            >
              <text>{{ label.text }}</text>
            </g>
          </svg>
        </div>
      </section>

      <aside class="iec61850-template-page__model-panel" aria-label="Template model properties">
        <div class="iec61850-template-page__panel-header">
          <span>Model</span>
          <span v-if="selectedTemplate" class="iec61850-template-page__panel-count">{{ selectedTemplate.slots.length }} slots</span>
        </div>

        <div v-if="selectedTemplate" class="iec61850-template-page__model-content">
          <section class="iec61850-template-page__model-section">
            <h2>Properties</h2>
            <dl class="iec61850-template-page__property-list">
              <template v-for="[label, value] in propertyRows" :key="label">
                <dt>{{ label }}</dt>
                <dd>{{ value }}</dd>
              </template>
            </dl>
          </section>

          <section class="iec61850-template-page__model-section">
            <h2>Slots</h2>
            <div class="iec61850-template-page__slot-table">
              <div class="iec61850-template-page__slot-table-row is-header">
                <span>Role</span>
                <span>Kind</span>
                <span>XY</span>
              </div>
              <div
                v-for="slot in selectedTemplate.slots"
                :key="slot.id"
                class="iec61850-template-page__slot-table-row"
              >
                <span>{{ slot.role }}</span>
                <span>{{ slot.kind }}</span>
                <span>{{ slot.point.x }}, {{ slot.point.y }}</span>
              </div>
            </div>
          </section>

          <section class="iec61850-template-page__model-section">
            <h2>Raw template</h2>
            <pre class="iec61850-template-page__json">{{ modelJson }}</pre>
          </section>
        </div>
      </aside>
    </main>
  </div>
</template>

<style scoped>
.iec61850-template-page {
  display: flex;
  height: 100%;
  min-height: 0;
  flex-direction: column;
  gap: 0.75rem;
  padding: 1rem;
  overflow: hidden;
  color: var(--color-neutral-800);
}

.iec61850-template-page__header,
.iec61850-template-page__template-list,
.iec61850-template-page__preview-panel,
.iec61850-template-page__model-panel {
  border: 1px solid color-mix(in srgb, var(--runtime-accent) 12%, var(--color-neutral-200));
  border-radius: var(--radius-lg);
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--color-white) 96%, var(--runtime-accent-soft)), color-mix(in srgb, var(--color-white) 88%, var(--color-neutral-50)));
  box-shadow:
    inset 0 1px 0 rgb(255 255 255 / 0.72),
    0 14px 28px rgb(15 23 42 / 0.05);
}

.iec61850-template-page__header {
  display: flex;
  flex: 0 0 auto;
  align-items: center;
  justify-content: space-between;
  gap: 1rem;
  padding: 1rem;
}

.iec61850-template-page__title-block {
  min-width: 0;
}

.iec61850-template-page__eyebrow,
.iec61850-template-page__status,
.iec61850-template-page__panel-header,
.iec61850-template-page__panel-count {
  color: var(--color-neutral-500);
  font-size: 0.6875rem;
  font-weight: 700;
  letter-spacing: 0;
  text-transform: uppercase;
}

.iec61850-template-page__eyebrow,
.iec61850-template-page__status,
.iec61850-template-page__title {
  margin: 0;
}

.iec61850-template-page__title {
  margin-top: 0.125rem;
  color: var(--color-neutral-950);
  font-size: 1.5rem;
  line-height: 1.2;
}

.iec61850-template-page__status {
  margin-top: 0.375rem;
  text-transform: none;
}

.iec61850-template-page__nav-link {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-height: 2rem;
  padding: 0.4rem 0.75rem;
  border: 1px solid color-mix(in srgb, var(--runtime-accent) 26%, var(--color-neutral-300));
  border-radius: var(--radius-sm);
  background: color-mix(in srgb, var(--color-white) 80%, transparent);
  color: var(--color-neutral-800);
  font-size: var(--text-sm);
  font-weight: 700;
  text-decoration: none;
}

.iec61850-template-page__workspace {
  display: grid;
  flex: 1 1 auto;
  min-height: 0;
  grid-template-columns: minmax(16rem, 0.55fr) minmax(30rem, 1.35fr) minmax(26rem, 0.95fr);
  gap: 0.75rem;
}

.iec61850-template-page__template-list,
.iec61850-template-page__preview-panel,
.iec61850-template-page__model-panel {
  display: flex;
  min-height: 0;
  flex-direction: column;
  overflow: hidden;
}

.iec61850-template-page__panel-header {
  display: flex;
  flex: 0 0 auto;
  align-items: center;
  justify-content: space-between;
  gap: 0.75rem;
  padding: 0.75rem 1rem;
  border-bottom: 1px solid color-mix(in srgb, var(--color-neutral-200) 72%, transparent);
}

.iec61850-template-page__panel-count {
  color: var(--color-neutral-400);
  font-weight: 600;
  text-transform: none;
}

.iec61850-template-page__template-items {
  display: grid;
  gap: 0.5rem;
  overflow: auto;
  padding: 0.75rem;
}

.iec61850-template-page__template-item {
  display: grid;
  gap: 0.25rem;
  min-width: 0;
  padding: 0.75rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 80%, transparent);
  border-radius: var(--radius-md);
  background: color-mix(in srgb, var(--color-white) 76%, transparent);
  color: var(--color-neutral-700);
  cursor: pointer;
  text-align: left;
}

.iec61850-template-page__template-item:hover {
  border-color: color-mix(in srgb, var(--runtime-accent) 18%, var(--color-neutral-200));
  background: color-mix(in srgb, var(--runtime-accent) 7%, var(--color-white));
}

.iec61850-template-page__template-item.is-selected {
  border-color: color-mix(in srgb, var(--runtime-accent) 36%, var(--color-neutral-200));
  background: color-mix(in srgb, var(--runtime-accent) 12%, var(--color-white));
  color: var(--color-neutral-950);
}

.iec61850-template-page__template-name {
  overflow: hidden;
  font-size: var(--text-sm);
  font-weight: 800;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.iec61850-template-page__template-meta {
  overflow: hidden;
  color: var(--color-neutral-500);
  font-family: var(--font-mono);
  font-size: 0.6875rem;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.iec61850-template-page__preview-stage {
  flex: 1 1 auto;
  min-height: 0;
  overflow: auto;
  padding: 1rem;
}

.iec61850-template-page__svg {
  display: block;
  width: 100%;
  min-width: 34rem;
  height: 100%;
  min-height: 34rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 82%, transparent);
  border-radius: var(--radius-md);
  background: var(--color-white);
}

.iec61850-template-page__svg-background {
  fill: color-mix(in srgb, var(--color-white) 96%, var(--color-neutral-50));
}

.iec61850-template-page__svg-grid {
  fill: url("#iec61850-template-grid");
}

.iec61850-template-page__grid-dot {
  fill: color-mix(in srgb, var(--color-neutral-400) 58%, transparent);
}

.iec61850-template-page__wire {
  fill: none;
  stroke: var(--color-neutral-950);
  stroke-linecap: square;
  stroke-linejoin: miter;
}

.iec61850-template-page__wire.is-normal {
  stroke-width: 4;
}

.iec61850-template-page__wire.is-bold {
  stroke-width: 7;
}

.iec61850-template-page__slot-shape {
  stroke: none;
}

.iec61850-template-page__slot.is-feeder-terminal .iec61850-template-page__slot-shape {
  fill: none;
  stroke: var(--color-neutral-950);
  stroke-width: 5;
  stroke-linecap: square;
  stroke-linejoin: miter;
}

.iec61850-template-page__slot.is-circuit-breaker .iec61850-template-page__slot-shape {
  fill: #f5c84b;
}

.iec61850-template-page__slot.is-bus-disconnector .iec61850-template-page__slot-shape,
.iec61850-template-page__slot.is-line-disconnector .iec61850-template-page__slot-shape {
  fill: #54bce8;
}

.iec61850-template-page__slot.is-earth-switch .iec61850-template-page__slot-shape {
  fill: #54bce8;
}

.iec61850-template-page__ground-symbol {
  fill: none;
  stroke: var(--color-neutral-950);
  stroke-width: 3;
  stroke-linecap: square;
}

.iec61850-template-page__slot-label,
.iec61850-template-page__template-label text {
  fill: var(--color-neutral-950);
  font-size: 13px;
  font-weight: 700;
}

.iec61850-template-page__slot-coordinate {
  fill: var(--color-neutral-500);
  font-family: var(--font-mono);
  font-size: 11px;
}

.iec61850-template-page__model-content {
  display: grid;
  gap: 0.75rem;
  overflow: auto;
  padding: 1rem;
}

.iec61850-template-page__model-section {
  overflow: hidden;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 82%, transparent);
  border-radius: var(--radius-md);
  background: color-mix(in srgb, var(--color-white) 80%, transparent);
}

.iec61850-template-page__model-section h2 {
  margin: 0;
  padding: 0.5rem 0.75rem;
  border-bottom: 1px solid color-mix(in srgb, var(--color-neutral-200) 74%, transparent);
  color: var(--color-neutral-500);
  font-size: 0.6875rem;
  letter-spacing: 0;
  text-transform: uppercase;
}

.iec61850-template-page__property-list {
  display: grid;
  grid-template-columns: minmax(8rem, 0.44fr) minmax(0, 1fr);
  margin: 0;
}

.iec61850-template-page__property-list dt,
.iec61850-template-page__property-list dd,
.iec61850-template-page__slot-table-row span {
  min-width: 0;
  margin: 0;
  padding: 0.5rem 0.75rem;
  border-top: 1px solid color-mix(in srgb, var(--color-neutral-200) 54%, transparent);
  font-size: 0.75rem;
  line-height: 1.35;
}

.iec61850-template-page__property-list dt {
  color: var(--color-neutral-500);
}

.iec61850-template-page__property-list dd,
.iec61850-template-page__slot-table-row span,
.iec61850-template-page__json {
  color: var(--color-neutral-850, var(--color-neutral-900));
  font-family: var(--font-mono);
}

.iec61850-template-page__slot-table {
  display: grid;
}

.iec61850-template-page__slot-table-row {
  display: grid;
  grid-template-columns: minmax(8rem, 1fr) minmax(5rem, 0.55fr) minmax(4rem, 0.45fr);
}

.iec61850-template-page__slot-table-row.is-header span {
  color: var(--color-neutral-500);
  font-family: inherit;
  font-weight: 700;
  text-transform: uppercase;
}

.iec61850-template-page__json {
  max-height: 20rem;
  margin: 0;
  overflow: auto;
  padding: 0.75rem;
  font-size: 0.6875rem;
  line-height: 1.45;
}

:global(.dark .iec61850-template-page) {
  color: var(--color-neutral-100);
}

:global(.dark .iec61850-template-page__header),
:global(.dark .iec61850-template-page__template-list),
:global(.dark .iec61850-template-page__preview-panel),
:global(.dark .iec61850-template-page__model-panel) {
  border-color: color-mix(in srgb, var(--runtime-accent) 14%, var(--color-neutral-800));
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--color-neutral-900) 88%, var(--runtime-accent-soft)), color-mix(in srgb, var(--color-neutral-950) 86%, var(--color-neutral-900)));
  box-shadow:
    inset 0 1px 0 rgb(255 255 255 / 0.04),
    0 16px 30px rgb(0 0 0 / 0.18);
}

:global(.dark .iec61850-template-page__title),
:global(.dark .iec61850-template-page__nav-link) {
  color: var(--color-neutral-50);
}

:global(.dark .iec61850-template-page__panel-header),
:global(.dark .iec61850-template-page__model-section h2) {
  border-color: var(--color-neutral-800);
}

:global(.dark .iec61850-template-page__template-item),
:global(.dark .iec61850-template-page__model-section),
:global(.dark .iec61850-template-page__nav-link) {
  border-color: var(--color-neutral-800);
  background: color-mix(in srgb, var(--color-neutral-950) 58%, transparent);
}

:global(.dark .iec61850-template-page__template-item) {
  color: var(--color-neutral-200);
}

:global(.dark .iec61850-template-page__template-item:hover) {
  border-color: color-mix(in srgb, var(--runtime-accent) 18%, var(--color-neutral-700));
  background: color-mix(in srgb, var(--color-neutral-800) 58%, transparent);
}

:global(.dark .iec61850-template-page__template-item.is-selected) {
  border-color: color-mix(in srgb, var(--runtime-accent) 34%, var(--color-neutral-700));
  background: color-mix(in srgb, var(--runtime-accent) 14%, var(--color-neutral-900));
  color: var(--color-neutral-50);
}

:global(.dark .iec61850-template-page__svg) {
  border-color: var(--color-neutral-800);
  background: var(--color-neutral-950);
}

:global(.dark .iec61850-template-page__svg-background) {
  fill: color-mix(in srgb, var(--color-neutral-950) 92%, var(--color-neutral-900));
}

:global(.dark .iec61850-template-page__grid-dot) {
  fill: color-mix(in srgb, var(--color-neutral-500) 56%, transparent);
}

:global(.dark .iec61850-template-page__wire),
:global(.dark .iec61850-template-page__ground-symbol),
:global(.dark .iec61850-template-page__slot.is-feeder-terminal .iec61850-template-page__slot-shape) {
  stroke: var(--color-neutral-100);
}

:global(.dark .iec61850-template-page__slot-label),
:global(.dark .iec61850-template-page__template-label text) {
  fill: var(--color-neutral-100);
}

:global(.dark .iec61850-template-page__slot-coordinate),
:global(.dark .iec61850-template-page__template-meta) {
  fill: var(--color-neutral-400);
  color: var(--color-neutral-400);
}

:global(.dark .iec61850-template-page__property-list dt),
:global(.dark .iec61850-template-page__property-list dd),
:global(.dark .iec61850-template-page__slot-table-row span) {
  border-color: color-mix(in srgb, var(--color-neutral-800) 74%, transparent);
}

:global(.dark .iec61850-template-page__property-list dd),
:global(.dark .iec61850-template-page__slot-table-row span),
:global(.dark .iec61850-template-page__json) {
  color: var(--color-neutral-100);
}

@media (max-width: 1199px) {
  .iec61850-template-page {
    overflow: auto;
  }

  .iec61850-template-page__workspace {
    grid-template-columns: minmax(0, 1fr);
  }

  .iec61850-template-page__template-list,
  .iec61850-template-page__preview-panel,
  .iec61850-template-page__model-panel {
    min-height: 22rem;
  }
}
</style>
