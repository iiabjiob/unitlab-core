<script setup lang="ts">
import { computed, nextTick, ref, watch, type ComponentPublicInstance } from "vue"
import { RouterLink } from "vue-router"
import { useTreeviewController, type TreeviewNode } from "@affino/treeview-vue"

import { Iec61850SclAPI, type Iec61850SclImportResponse } from "@/api/iec61850Client.api"
import UiButton from "@/components/ui/UiButton.vue"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useToastStore } from "@/stores/toastStore"
import {
  buildIec61850NativeTreeDocument,
  type Iec61850NativeTreeDocument,
  type Iec61850NativeTreeRow,
} from "./iec61850NativeTree"

type NodeValue = string

const fileInput = ref<HTMLInputElement | null>(null)
const fileName = ref<string | null>(null)
const selectedIed = ref("KINTE15FMP")
const loading = ref(false)
const error = ref<string | null>(null)
const importResponse = ref<Iec61850SclImportResponse | null>(null)
const document = ref<Iec61850NativeTreeDocument | null>(null)
const selectedValue = ref<NodeValue | null>(null)
const treeSearch = ref("")
const itemElements = new Map<NodeValue, HTMLButtonElement>()

const workspaceStore = useWorkspaceStore()
const toastStore = useToastStore()

const tree = useTreeviewController<NodeValue>({
  nodes: [],
  loop: true,
})

const rows = computed(() => document.value?.rows ?? [])
const rowByValue = computed(() => {
  const map = new Map<NodeValue, Iec61850NativeTreeRow>()
  rows.value.forEach(row => map.set(row.value, row))
  return map
})
const treeNodes = computed<TreeviewNode<NodeValue>[]>(() => rows.value.map(row => ({
  value: row.value,
  parent: row.parent,
  text: [row.kind, row.label, row.meta, row.detail.title, row.detail.subtitle]
    .filter(Boolean)
    .join(" "),
})))
const parentByValue = computed(() => {
  const map = new Map<NodeValue, NodeValue | null>()
  rows.value.forEach(row => map.set(row.value, row.parent))
  return map
})
const expandedSet = computed(() => new Set(tree.state.value.expanded))
const visibleRows = computed(() => rows.value.filter(row => isNodeVisible(row.value)))
const selectedRow = computed(() => selectedValue.value ? rowByValue.value.get(selectedValue.value) ?? null : rows.value[0] ?? null)
const stats = computed(() => document.value?.stats ?? {
  logicalDevices: 0,
  logicalNodes: 0,
  dataSets: 0,
  reports: 0,
  signals: 0,
  errors: 0,
  warnings: 0,
})
const statusLabel = computed(() => {
  if (loading.value) return "Compiling SCD on backend"
  if (error.value) return "Backend compile failed"
  if (!importResponse.value) return "No backend import loaded"
  return `${importResponse.value.selected_ied} · ${stats.value.logicalDevices} LD · ${stats.value.reports} reports · ${stats.value.signals} leaves`
})
const treeCountLabel = computed(() => rows.value.length ? `${visibleRows.value.length}/${rows.value.length} nodes` : "")

watch(treeNodes, (nodes) => {
  tree.registerNodes(nodes)
  if (!nodes.length) {
    selectedValue.value = null
    return
  }
  expandDefaults()
  selectNode(nodes[0].value)
}, { immediate: true })

watch(treeSearch, (query) => {
  tree.setSearchQuery(query)
})

watch(() => tree.state.value.active, async (active) => {
  if (!active) return
  await nextTick()
  itemElements.get(active)?.focus({ preventScroll: true })
})

function openFileDialog() {
  fileInput.value?.click()
}

async function onFileSelected(event: Event) {
  const input = event.target as HTMLInputElement | null
  const file = input?.files?.[0]
  if (!file) return

  loading.value = true
  error.value = null
  fileName.value = file.name
  importResponse.value = null
  document.value = null
  selectedValue.value = null

  try {
    await workspaceStore.bootstrap()
    const workspaceId = workspaceStore.activeWorkspaceId
    if (!workspaceId) throw new Error("Active workspace is not selected")
    const response = await Iec61850SclAPI.importScl(workspaceId, file, selectedIed.value)
    importResponse.value = response
    document.value = buildIec61850NativeTreeDocument(response)
    toastStore.success(`SCL compiled: ${response.selected_ied}`)
  } catch (caught) {
    error.value = caught instanceof Error ? caught.message : "SCL backend import failed"
    toastStore.error(error.value)
  } finally {
    loading.value = false
    if (input) input.value = ""
  }
}

function expandDefaults() {
  for (const row of rows.value) {
    if (row.kind === "import" || row.kind.endsWith("group")) {
      tree.expand(row.value)
    }
  }
}

function selectNode(value: NodeValue) {
  selectedValue.value = value
  tree.focus(value)
  tree.select(value)
}

function onRowClick(row: Iec61850NativeTreeRow) {
  selectNode(row.value)
  if (!row.isLeaf) tree.toggle(row.value)
}

function bindItemElement(value: NodeValue) {
  return (element: Element | ComponentPublicInstance | null) => {
    const resolved = element instanceof Element ? element : (element?.$el instanceof Element ? element.$el : null)
    if (resolved instanceof HTMLButtonElement) {
      itemElements.set(value, resolved)
      return
    }
    itemElements.delete(value)
  }
}

function nodeLevel(value: NodeValue): number {
  let level = 1
  let cursor = parentByValue.value.get(value) ?? null
  while (cursor) {
    level += 1
    cursor = parentByValue.value.get(cursor) ?? null
  }
  return level
}

function isNodeVisible(value: NodeValue): boolean {
  let cursor = parentByValue.value.get(value) ?? null
  while (cursor) {
    if (!expandedSet.value.has(cursor)) return false
    cursor = parentByValue.value.get(cursor) ?? null
  }
  return true
}

function isSelected(value: NodeValue): boolean {
  return selectedValue.value === value
}

function isExpanded(value: NodeValue): boolean {
  return tree.isExpanded(value)
}

function shortHash(value: string | null | undefined): string {
  return value ? value.slice(0, 12) : ""
}
</script>

<template>
  <div class="iec61850-native-page">
    <header class="iec61850-native-page__header">
      <div>
        <p class="iec61850-native-page__eyebrow">IEC 61850 backend compiler</p>
        <h1 class="iec61850-native-page__title">Native SCL Tree</h1>
        <p class="iec61850-native-page__status">{{ statusLabel }}</p>
      </div>
      <div class="iec61850-native-page__actions">
        <RouterLink class="iec61850-native-page__nav-link" to="/61850-debug">Frontend parser</RouterLink>
        <RouterLink class="iec61850-native-page__nav-link" to="/61850-debug/client">MMS client test</RouterLink>
        <label class="iec61850-native-page__ied-field">
          <span>Selected IED</span>
          <input v-model="selectedIed" type="text" autocomplete="off" spellcheck="false">
        </label>
        <UiButton variant="secondary" size="sm" :disabled="loading" @click="openFileDialog">
          {{ importResponse ? "Compile another SCD" : "Choose SCD" }}
        </UiButton>
        <input
          ref="fileInput"
          class="iec61850-native-page__file-input"
          type="file"
          accept=".scd,.ssd,.xml,text/xml,application/xml"
          autocomplete="off"
          @change="onFileSelected"
        >
      </div>
    </header>

    <section v-if="error" class="iec61850-native-page__alert">{{ error }}</section>

    <section class="iec61850-native-page__summary" aria-label="Native compiler summary">
      <div class="iec61850-native-page__metric"><span>LD</span><strong>{{ stats.logicalDevices }}</strong></div>
      <div class="iec61850-native-page__metric"><span>LN</span><strong>{{ stats.logicalNodes }}</strong></div>
      <div class="iec61850-native-page__metric"><span>DataSets</span><strong>{{ stats.dataSets }}</strong></div>
      <div class="iec61850-native-page__metric"><span>Reports</span><strong>{{ stats.reports }}</strong></div>
      <div class="iec61850-native-page__metric"><span>Signals</span><strong>{{ stats.signals }}</strong></div>
      <div class="iec61850-native-page__metric"><span>Errors</span><strong>{{ stats.errors }}</strong></div>
      <div class="iec61850-native-page__metric"><span>Warnings</span><strong>{{ stats.warnings }}</strong></div>
      <div class="iec61850-native-page__metric iec61850-native-page__metric--wide"><span>Hash</span><strong>{{ shortHash(importResponse?.source_hash) }}</strong></div>
    </section>

    <main class="iec61850-native-page__workspace">
      <section class="iec61850-native-page__tree-panel" aria-label="Native compiler tree">
        <div class="iec61850-native-page__panel-header">
          <span>Compiled MMS model</span>
          <span>{{ treeCountLabel }}</span>
        </div>
        <div v-if="!document" class="iec61850-native-page__empty">
          Upload SCD here to inspect backend compiler output without touching the frontend parser page.
        </div>
        <template v-else>
          <input v-model="treeSearch" class="iec61850-native-page__search" type="search" placeholder="Search compiled model" autocomplete="off">
          <div class="iec61850-native-page__tree" role="tree" aria-label="Compiled IEC 61850 model">
            <button
              v-for="row in visibleRows"
              :key="row.value"
              :ref="bindItemElement(row.value)"
              type="button"
              class="iec61850-native-page__tree-row"
              :class="[`is-${row.kind}`, { 'is-selected': isSelected(row.value) }]"
              role="treeitem"
              :aria-level="nodeLevel(row.value)"
              :aria-expanded="row.isLeaf ? undefined : isExpanded(row.value)"
              :aria-selected="isSelected(row.value)"
              @click="onRowClick(row)"
            >
              <span class="iec61850-native-page__indent" :style="{ width: `${(nodeLevel(row.value) - 1) * 14}px` }"></span>
              <span class="iec61850-native-page__toggle">{{ row.isLeaf ? "•" : (isExpanded(row.value) ? "▾" : "▸") }}</span>
              <span class="iec61850-native-page__kind">{{ row.kind.replace(/-.*/, "") }}</span>
              <span class="iec61850-native-page__label">{{ row.label }}</span>
              <span v-if="row.meta" class="iec61850-native-page__meta">{{ row.meta }}</span>
            </button>
          </div>
        </template>
      </section>

      <aside class="iec61850-native-page__detail" aria-label="Selected node detail">
        <div class="iec61850-native-page__panel-header">
          <span>Details</span>
          <span>{{ fileName ?? "" }}</span>
        </div>
        <div v-if="!selectedRow" class="iec61850-native-page__empty">No node selected.</div>
        <template v-else>
          <h2>{{ selectedRow.detail.title }}</h2>
          <p>{{ selectedRow.detail.subtitle }}</p>
          <dl>
            <template v-for="row in selectedRow.detail.rows" :key="`${row.label}:${row.value}`">
              <dt>{{ row.label }}</dt>
              <dd>{{ row.value || "—" }}</dd>
            </template>
          </dl>
        </template>
      </aside>
    </main>
  </div>
</template>

<style scoped>
.iec61850-native-page {
  display: flex;
  min-height: calc(100vh - 32px);
  flex-direction: column;
  gap: 12px;
  padding: 16px;
  color: #172033;
  background: #f4f6f8;
}

.iec61850-native-page__header,
.iec61850-native-page__summary,
.iec61850-native-page__workspace,
.iec61850-native-page__tree-panel,
.iec61850-native-page__detail,
.iec61850-native-page__alert {
  border: 1px solid #d7dde6;
  border-radius: 8px;
  background: #ffffff;
}

.iec61850-native-page__header {
  display: flex;
  align-items: flex-end;
  justify-content: space-between;
  gap: 16px;
  padding: 14px 16px;
}

.iec61850-native-page__eyebrow,
.iec61850-native-page__status {
  margin: 0;
  color: #637083;
  font-size: 12px;
}

.iec61850-native-page__title {
  margin: 2px 0 4px;
  font-size: 22px;
  line-height: 1.2;
}

.iec61850-native-page__actions {
  display: flex;
  flex-wrap: wrap;
  align-items: flex-end;
  justify-content: flex-end;
  gap: 8px;
}

.iec61850-native-page__nav-link {
  color: #24589a;
  font-size: 13px;
  text-decoration: none;
}

.iec61850-native-page__ied-field {
  display: grid;
  gap: 3px;
  color: #637083;
  font-size: 11px;
}

.iec61850-native-page__ied-field input,
.iec61850-native-page__search {
  height: 32px;
  border: 1px solid #cbd4df;
  border-radius: 6px;
  padding: 0 10px;
  background: #ffffff;
  color: #172033;
}

.iec61850-native-page__file-input {
  display: none;
}

.iec61850-native-page__alert {
  border-color: #efb4b4;
  padding: 10px 12px;
  color: #9f2424;
  background: #fff5f5;
}

.iec61850-native-page__summary {
  display: grid;
  grid-template-columns: repeat(8, minmax(90px, 1fr));
  gap: 1px;
  overflow: hidden;
}

.iec61850-native-page__metric {
  display: grid;
  gap: 4px;
  padding: 10px 12px;
  background: #fbfcfe;
}

.iec61850-native-page__metric span {
  color: #637083;
  font-size: 11px;
}

.iec61850-native-page__metric strong {
  overflow: hidden;
  font-size: 17px;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.iec61850-native-page__workspace {
  display: grid;
  min-height: 560px;
  grid-template-columns: minmax(360px, 1.1fr) minmax(320px, 0.9fr);
  gap: 0;
  overflow: hidden;
}

.iec61850-native-page__tree-panel,
.iec61850-native-page__detail {
  min-width: 0;
  border: 0;
  border-radius: 0;
}

.iec61850-native-page__detail {
  border-left: 1px solid #d7dde6;
}

.iec61850-native-page__panel-header {
  display: flex;
  min-height: 42px;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  border-bottom: 1px solid #d7dde6;
  padding: 0 12px;
  color: #637083;
  font-size: 12px;
}

.iec61850-native-page__empty {
  padding: 18px;
  color: #637083;
  font-size: 13px;
}

.iec61850-native-page__search {
  width: calc(100% - 24px);
  margin: 10px 12px;
}

.iec61850-native-page__tree {
  height: 492px;
  overflow: auto;
  padding: 4px 8px 12px;
}

.iec61850-native-page__tree-row {
  display: flex;
  width: 100%;
  height: 30px;
  align-items: center;
  gap: 6px;
  border: 0;
  border-radius: 6px;
  padding: 0 8px;
  background: transparent;
  color: #172033;
  font: inherit;
  text-align: left;
}

.iec61850-native-page__tree-row:hover,
.iec61850-native-page__tree-row.is-selected {
  background: #e9f1fb;
}

.iec61850-native-page__toggle {
  width: 14px;
  color: #637083;
  text-align: center;
}

.iec61850-native-page__kind {
  width: 72px;
  flex: 0 0 auto;
  color: #637083;
  font-size: 11px;
  text-transform: uppercase;
}

.iec61850-native-page__label {
  min-width: 0;
  flex: 1 1 auto;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.iec61850-native-page__meta {
  flex: 0 0 auto;
  color: #637083;
  font-size: 12px;
}

.iec61850-native-page__detail h2 {
  margin: 16px 16px 4px;
  font-size: 18px;
}

.iec61850-native-page__detail p {
  margin: 0 16px 14px;
  overflow-wrap: anywhere;
  color: #637083;
  font-size: 13px;
}

.iec61850-native-page__detail dl {
  display: grid;
  grid-template-columns: 132px minmax(0, 1fr);
  gap: 0;
  margin: 0;
  padding: 0 16px 16px;
  font-size: 13px;
}

.iec61850-native-page__detail dt,
.iec61850-native-page__detail dd {
  border-top: 1px solid #eef1f5;
  margin: 0;
  padding: 8px 0;
}

.iec61850-native-page__detail dt {
  color: #637083;
}

.iec61850-native-page__detail dd {
  min-width: 0;
  overflow-wrap: anywhere;
}

@media (max-width: 900px) {
  .iec61850-native-page__header {
    align-items: stretch;
    flex-direction: column;
  }

  .iec61850-native-page__summary {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }

  .iec61850-native-page__workspace {
    grid-template-columns: 1fr;
  }

  .iec61850-native-page__detail {
    border-top: 1px solid #d7dde6;
    border-left: 0;
  }
}
</style>
