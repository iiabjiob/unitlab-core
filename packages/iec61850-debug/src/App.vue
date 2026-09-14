<script setup lang="ts">
import { computed, ref } from "vue"

type Diagnostic = { level: "error" | "warning" | "info"; message: string }
type XmlSummary = {
  ieds: number
  accessPoints: number
  logicalDevices: number
  logicalNodes: number
  dataSets: number
  reportControls: number
}

const fileName = ref<string | null>(null)
const xmlText = ref<string | null>(null)
const error = ref<string | null>(null)

const documentNode = computed(() => {
  if (!xmlText.value) return null
  const parsed = new DOMParser().parseFromString(xmlText.value, "application/xml")
  const parserError = parsed.querySelector("parsererror")
  if (parserError) return null
  return parsed
})

const summary = computed<XmlSummary | null>(() => {
  const doc = documentNode.value
  if (!doc) return null
  return {
    ieds: doc.getElementsByTagName("IED").length,
    accessPoints: doc.getElementsByTagName("AccessPoint").length,
    logicalDevices: doc.getElementsByTagName("LDevice").length,
    logicalNodes: doc.getElementsByTagName("LN").length + doc.getElementsByTagName("LN0").length,
    dataSets: doc.getElementsByTagName("DataSet").length,
    reportControls: doc.getElementsByTagName("ReportControl").length,
  }
})

const diagnostics = computed<Diagnostic[]>(() => {
  if (!xmlText.value) return []
  if (!documentNode.value) return [{ level: "error", message: "The file is not valid XML." }]
  if (!summary.value?.ieds) return [{ level: "warning", message: "No IED elements were found." }]
  return [{ level: "info", message: "XML loaded locally. No UnitLab state or hardware action is involved." }]
})

async function loadFile(event: Event) {
  const input = event.target as HTMLInputElement
  const file = input.files?.[0]
  input.value = ""
  if (!file) return
  fileName.value = file.name
  error.value = null
  try {
    xmlText.value = await file.text()
  } catch (cause) {
    xmlText.value = null
    error.value = cause instanceof Error ? cause.message : "Unable to read the file."
  }
}
</script>

<template>
  <main class="debug-app">
    <header class="topbar">
      <div>
        <p class="eyebrow">Standalone engineering tool</p>
        <h1>IEC 61850 Debug</h1>
        <p class="subtitle">Inspect SCD structure without loading UnitLab or touching a device.</p>
      </div>
      <label class="file-button">
        Open SCD
        <input type="file" accept=".scd,.sed,.ssd,.xml,application/xml,text/xml" @change="loadFile">
      </label>
    </header>

    <section v-if="error" class="panel error">{{ error }}</section>
    <section v-else-if="!summary" class="panel empty">
      <strong>Choose an SCD file to begin.</strong>
      <span>The first version is intentionally local and read-only.</span>
    </section>

    <template v-else>
      <section class="file-state panel">
        <span class="dot"></span>
        <strong>{{ fileName }}</strong>
        <span>loaded locally</span>
      </section>

      <section class="metrics">
        <div v-for="(value, label) in summary" :key="label" class="metric panel">
          <span>{{ label }}</span>
          <strong>{{ value }}</strong>
        </div>
      </section>

      <section class="panel diagnostics">
        <h2>Diagnostics</h2>
        <div v-for="item in diagnostics" :key="item.message" class="diagnostic" :class="item.level">
          <span>{{ item.level }}</span>
          <p>{{ item.message }}</p>
        </div>
      </section>
    </template>
  </main>
</template>
