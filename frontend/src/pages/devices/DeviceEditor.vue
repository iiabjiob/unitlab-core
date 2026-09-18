<script setup lang="ts">
import { computed, onBeforeUnmount, watch } from "vue"
import { useTabsController } from "@affino/tabs-vue"
import { useRoute } from "vue-router"

import { useDeviceStore } from "@/stores/deviceStore"
import { useChannelStore } from "@/stores/channelStore"
import { useSelectionStore } from "@/stores/selectionStore"
import { useRealtimeScopeStore } from "@/stores/realtimeScopeStore"

import DeviceEditorHeader from "./components/DeviceEditorHeader.vue"
import DeviceExecutionLog from "./components/DeviceExecutionLog.vue"
import DeviceDiagnosticsPanel from "./components/DeviceDiagnosticsPanel.vue"
import EditorWorkspaceLayout from "@/components/layout/EditorWorkspaceLayout.vue"
import DeviceChannelsList from "./components/DeviceChannelsList.vue"

const route = useRoute()
const store = useDeviceStore()
const channelStore = useChannelStore()
const selectionStore = useSelectionStore()
const realtimeScopeStore = useRealtimeScopeStore()
const scopeId = "devices:editor"

type DeviceDetailTab = "log" | "diag"
const detailTabs = useTabsController<DeviceDetailTab>("log")
const activeDetailTab = computed<DeviceDetailTab>(() => (detailTabs.state.value.value === "diag" ? "diag" : "log"))

const deviceId = computed(() => Number(route.params.id))

const device = computed(() =>
  store.devices.find(d => d.id === deviceId.value)
)

watch(
  () => device.value?.id ?? null,
  async (id) => {
    selectionStore.selectDevice(id)
    if (!id) {
      realtimeScopeStore.clearRealtimeUnitScope(scopeId)
      return
    }
    try {
      await channelStore.ensureDeviceChannelsLoaded(id)
    } catch {
      realtimeScopeStore.clearRealtimeUnitScope(scopeId)
      return
    }
    const target = store.devices.find(item => item.id === id)
    if (!target) {
      realtimeScopeStore.clearRealtimeUnitScope(scopeId)
      return
    }
    realtimeScopeStore.setRealtimeUnitScope(scopeId, [target.unit_id])
  },
  { immediate: true }
)

onBeforeUnmount(() => {
  realtimeScopeStore.clearRealtimeUnitScope(scopeId)
})

</script>

<template>
  <div class="device-editor">
    <DeviceEditorHeader
      v-if="device"
      :device="device"
    />

    <EditorWorkspaceLayout
      storage-key="device-channels-list-width"
      :min-size="380"
      :default-size="380"
      :max-size="800"
    >
      <template #sidebar>
        <template v-if="device">
          <DeviceChannelsList :device="device" />
        </template>
      </template>

      <template #main>
        <div v-if="device" class="device-editor__detail-panel">
          <div class="device-editor__tabs">
            <button
              type="button"
              class="device-editor__tab"
              :class="{ 'is-active': activeDetailTab === 'log' }"
              @click="detailTabs.select('log')"
            >
              Log
            </button>
            <button
              type="button"
              class="device-editor__tab"
              :class="{ 'is-active': activeDetailTab === 'diag' }"
              @click="detailTabs.select('diag')"
            >
              Diagnostics
            </button>
          </div>
          <div class="device-editor__detail-content">
            <DeviceExecutionLog v-if="activeDetailTab === 'log'" :device="device" />
            <DeviceDiagnosticsPanel v-else :device="device" />
          </div>
        </div>
      </template>
    </EditorWorkspaceLayout>

  </div>
</template>

<style scoped>
.device-editor {
  display: flex;
  height: 100%;
  flex-direction: column;
  padding-inline-end: 1rem;
}

.device-editor__detail-panel {
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-lg);
  background: var(--color-neutral-50);
}

.device-editor__detail-panel {
  display: flex;
  min-height: 18rem;
  flex-direction: column;
}

.device-editor__detail-content {
  display: flex;
  flex: 1 1 0;
  min-height: 0;
  flex-direction: column;
}

.device-editor__detail-content > :deep(.device-execution-log) {
  flex: 1 1 0;
  min-height: 0;
}

.device-editor__tabs {
  display: flex;
  align-items: center;
  gap: 0.25rem;
  padding: 0.5rem;
  border-bottom: 1px solid var(--color-neutral-200);
  background: color-mix(in srgb, var(--color-white) 80%, transparent);
}

.device-editor__tab {
  padding: 0.375rem 0.75rem;
  border: 0;
  border-radius: var(--radius-md);
  background: transparent;
  color: var(--color-neutral-600);
  font: inherit;
  font-size: var(--text-xs);
  transition: background-color 0.15s ease, color 0.15s ease;
}

.device-editor__tab:hover {
  background: color-mix(in srgb, var(--color-neutral-200) 60%, transparent);
}

.device-editor__tab.is-active {
  background: var(--color-neutral-200);
  color: var(--color-neutral-900);
}

@media (min-width: 1500px) {
  .device-editor {
    min-height: 0;
    overflow: hidden;
  }

  .device-editor__detail-panel,
  .device-editor__detail-content {
    min-height: 0;
    flex: 1 1 0;
    overflow: hidden;
  }

  .device-editor__detail-content {
    display: flex;
    flex-direction: column;
    min-height: 0;
  }
}

@media (max-width: 1499px) {
  .device-editor__detail-panel {
    min-height: 18rem;
    overflow: hidden;
  }

  .device-editor__detail-content {
    flex: 1 1 0;
  }
}

:global(.dark .device-editor__detail-panel) {
  border-color: var(--color-neutral-700);
  background: color-mix(in srgb, var(--color-neutral-900) 60%, transparent);
}

:global(.dark .device-editor__tabs) {
  border-bottom-color: var(--color-neutral-700);
  background: color-mix(in srgb, var(--color-neutral-900) 70%, transparent);
}

:global(.dark .device-editor__tab) {
  color: var(--color-neutral-300);
}

:global(.dark .device-editor__tab:hover) {
  background: var(--color-neutral-800);
}

:global(.dark .device-editor__tab.is-active) {
  background: var(--color-neutral-700);
  color: var(--color-neutral-50);
}
</style>
