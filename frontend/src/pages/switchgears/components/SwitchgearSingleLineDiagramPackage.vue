<script setup lang="ts">
import { computed, ref, watch } from "vue"

import WorkspacePlaceholder from "@/components/ui/WorkspacePlaceholder.vue"
import { localSettingsKeys, readLocalSetting } from "@/services/localSettingsStorage"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"

import SwitchgearSingleLineDiagramPackageCanvas from "./SwitchgearSingleLineDiagramPackageCanvas.vue"
import { buildSwitchgearSldPackageSceneModel, normalizeStoredDiagramState } from "../utils/switchgearSldPackageScene"
import type { StoredDiagramState } from "../utils/switchgearSldDiagramTypes"

const props = defineProps<{
  active?: boolean
}>()

const workspaceStore = useWorkspaceStore()
const switchgearStore = useSwitchgearStore()
const storedState = ref<StoredDiagramState | null>(null)

const workspaceId = computed(() => workspaceStore.activeWorkspaceId)
const storageKey = computed(() => (
  workspaceId.value ? localSettingsKeys.switchgearDiagram(workspaceId.value) : null
))
const sceneModel = computed(() => buildSwitchgearSldPackageSceneModel(
  switchgearStore.switchgears,
  storedState.value,
))
const hasContent = computed(() => (
  sceneModel.value.stats.nodes > 0
  || sceneModel.value.stats.edges > 0
  || sceneModel.value.stats.statics > 0
  || sceneModel.value.stats.texts > 0
))

watch(
  () => [workspaceId.value, props.active] as const,
  () => {
    if (props.active === false) {
      return
    }
    loadStoredState()
  },
  { immediate: true },
)

function loadStoredState() {
  if (!workspaceId.value || !storageKey.value) {
    storedState.value = null
    return
  }

  storedState.value = readLocalSetting<StoredDiagramState | null>(
    storageKey.value,
    null,
    {
      legacyKeys: [`unitlab.switchgears.sld.${workspaceId.value}`],
      validate: normalizeStoredDiagramState,
    },
  )
}
</script>

<template>
  <section class="switchgear-sld-package">
    <WorkspacePlaceholder
      v-if="!workspaceId"
      tag="SLD"
      title="Select a workspace"
      description="Choose a workspace to compare and edit the package-based SLD projection."
    />
    <WorkspacePlaceholder
      v-else-if="!hasContent"
      tag="SLD"
      title="No SLD content"
      description="Load or draw a single line diagram in the legacy editor, then reopen this tab for package editing."
    />
    <SwitchgearSingleLineDiagramPackageCanvas
      v-else-if="storageKey"
      :key="sceneModel.sceneKey"
      :model="sceneModel"
      :workspace-id="workspaceId"
      :storage-key="storageKey"
      :initial-stored-state="storedState"
    />
  </section>
</template>

<style scoped>
.switchgear-sld-package {
  display: flex;
  min-height: 0;
  flex: 1 1 auto;
}

.switchgear-sld-package > :deep(*) {
  min-height: 0;
  flex: 1 1 auto;
}
</style>
