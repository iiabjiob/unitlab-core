<template>
  <div
    class="ui-table__header-group"
    :class="{
      'ui-table__header-group--expandable': isExpandable,
      'ui-table__header-group--collapsed': isExpandable && !group.isExpanded(),
      'ui-table__header-group--padding': group.isPadding(),
    }"
    :style="groupStyle"
    role="columnheader"
    :aria-colspan="ariaColspan"
    :aria-level="level + 1"
  >
    <button
      v-if="isExpandable"
      type="button"
      class="ui-table__header-group-toggle"
      @click="toggle"
      :aria-expanded="group.isExpanded()"
      :aria-label="`Toggle ${headerLabel}`"
    >
      <span class="ui-table__header-group-caret" :class="{ 'is-expanded': group.isExpanded() }"></span>
    </button>
    <span class="ui-table__header-group-label">
      {{ headerLabel }}
    </span>
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import type { ColumnGroup } from "../../core/columns/columnGroup"

const props = defineProps<{
  group: ColumnGroup
  startLine: number
  endLine: number
  level: number
}>()

const headerLabel = computed(() => props.group.getColGroupDef().headerName || "")
const isExpandable = computed(() => props.group.isExpandable())
const ariaColspan = computed(() => Math.max(1, props.endLine - props.startLine))

const groupStyle = computed(() => ({
  gridColumnStart: props.startLine,
  gridColumnEnd: props.endLine,
}))

function toggle() {
  props.group.toggleExpanded()
}
</script>

<style scoped>
.ui-table__header-group {
  display: flex;
  align-items: center;
  gap: 0.25rem;
  padding: 0 0.5rem;
  min-height: var(--ui-table-header-group-height, 28px);
  border-bottom: 1px solid var(--ui-table-border-color, rgba(0, 0, 0, 0.08));
  background: var(--ui-table-header-bg, rgba(250, 250, 250, 0.8));
  font-size: 11px;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  position: relative;
  user-select: none;
  transition: background-color 0.15s ease, color 0.15s ease;
  box-sizing: border-box;
}

.ui-table__header-group {
  display: flex;
}

.ui-table__header-group--expandable {
  cursor: pointer;
}

.ui-table__header-group--expandable:hover {
  background: var(--ui-table-header-hover-bg, rgba(241, 245, 249, 0.8));
}

.ui-table__header-group--padding {
  opacity: 0.6;
  font-weight: 500;
}

.ui-table__header-group-toggle {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 18px;
  height: 18px;
  border-radius: 9999px;
  border: none;
  background: transparent;
  color: inherit;
  cursor: pointer;
  transition: background-color 0.15s ease;
}

.ui-table__header-group-toggle:hover {
  background: rgba(0, 0, 0, 0.05);
}

.ui-table__header-group-caret {
  display: inline-block;
  width: 0;
  height: 0;
  border-left: 4px solid transparent;
  border-right: 4px solid transparent;
  border-top: 6px solid currentColor;
  transform-origin: center;
  transition: transform 0.2s ease;
}

.ui-table__header-group-caret.is-expanded {
  transform: rotate(-180deg);
}

.ui-table__header-group-label {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
</style>
