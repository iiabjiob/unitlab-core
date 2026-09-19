<script setup lang="ts">
import { computed, nextTick, onMounted, ref, watch } from "vue"
import { useRoute, useRouter } from "vue-router"

import { useSelectionStore } from "@/stores/selectionStore"
import SwitchgearSingleLineDiagramPackage from "./components/SwitchgearSingleLineDiagramPackage.vue"

const route = useRoute()
const router = useRouter()
const selectionStore = useSelectionStore()
const packageRef = ref<InstanceType<typeof SwitchgearSingleLineDiagramPackage> | null>(null)
const selectedSwitchgearId = computed(() => {
  return selectionStore.lastSwitchgearId
})

async function openImportFromQuery() {
  if (route.query.import !== "1" || !packageRef.value) {
    return
  }
  await nextTick()
  packageRef.value?.openScdFileDialog()
  const query = { ...route.query }
  delete query.import
  await router.replace({ query })
}

watch(() => route.query.import, () => {
  void openImportFromQuery()
})

onMounted(() => {
  void openImportFromQuery()
})

function openSwitchgearSettings(id: number) {
  void router.push({ name: "switchgears.detail", params: { id } })
}
</script>

<template>
  <div class="switchgear-sld-page">
    <SwitchgearSingleLineDiagramPackage
      ref="packageRef"
      :selected-switchgear-id="selectedSwitchgearId"
      :selection-version="selectionStore.switchgearSelectionVersion"
      @edit-switchgear-bindings="openSwitchgearSettings"
    />
  </div>
</template>

<style scoped>
.switchgear-sld-page {
  display: flex;
  height: 100%;
  min-height: 0;
  min-width: 0;
  flex-direction: column;
  overflow: hidden;
}
</style>
