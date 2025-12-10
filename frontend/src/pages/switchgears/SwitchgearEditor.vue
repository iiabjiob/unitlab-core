<script setup lang="ts">
import { computed, ref } from "vue"
import { useRoute, useRouter } from "vue-router"

import { useSwitchgearStore } from "@/stores/switchgearStore"

import SwitchgearEditorHeader from "./components/SwitchgearEditorHeader.vue"
import SwitchgearExecutionLog from "./components/SwitchgearExecutionLog.vue"
import SwitchgearControlToolbar from "./components/SwitchgearControlToolbar.vue"
import SwitchgearBindingsEditor from "./components/SwitchgearBindingsEditor.vue"
import ResizablePanel from "@/components/ui/ResizablePanel.vue"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"

const route = useRoute()
const router = useRouter()
const store = useSwitchgearStore()

const switchgearId = computed(() => Number(route.params.id))

const switchgear = computed(() =>
  store.switchgears.find(s => s.id === switchgearId.value)
)

const deleteModalOpen = ref(false)
const deleteMessage = computed(() =>
  switchgear.value
    ? `Switchgear "${switchgear.value.name}" will be deleted.`
    : ""
)

async function handleDuplicate() {
  if (!switchgear.value) return
  const duplicated = await store.duplicate(switchgear.value.id)
  await router.push({ name: "switchgears.detail", params: { id: duplicated.id } })
}

function requestDelete() {
  deleteModalOpen.value = true
}

function cancelDelete() {
  deleteModalOpen.value = false
}

async function confirmDelete() {
  if (!switchgear.value) return
  await store.remove(switchgear.value.id)
  deleteModalOpen.value = false
  await router.push({ name: "switchgears.list" })
}

</script>

<template>
  <div class="h-full flex flex-col">
    <template v-if="switchgear">
      <!-- HEADER -->
      <SwitchgearEditorHeader
        :switchgear="switchgear"
        @duplicate="handleDuplicate"
        @delete="requestDelete"
      />

      <!-- CONTROL TOOLBAR -->
      <SwitchgearControlToolbar :switchgear="switchgear" />

      <div class="flex flex-1 overflow-hidden bg-white dark:bg-neutral-800 shadow rounded p-5">

        <!-- BINDINGS PANEL -->
        <ResizablePanel
          :switchgear="switchgear"
          placement="left"
          storageKey="switchgear-list-width"
          :minSize="380"
          :defaultSize="380"
          :maxSize="800"
        >
          <SwitchgearBindingsEditor :switchgear="switchgear" />
        </ResizablePanel>

        <!-- LOG PANEL -->
        <div class="flex flex-col flex-1 overflow-hidden">
          <div class="flex-1 overflow-y-auto">
            <SwitchgearExecutionLog :switchgear="switchgear" />
          </div>
        </div>

      </div>
    </template>

    <div v-else class="flex-1 flex items-center justify-center text-neutral-500">
      Switchgear not found.
    </div>

    <ConfirmModal
      v-if="switchgear"
      :open="deleteModalOpen"
      title="Delete switchgear"
      :message="deleteMessage"
      confirm-label="Delete"
      cancel-label="Cancel"
      @confirm="confirmDelete"
      @cancel="cancelDelete"
    />
  </div>
</template>
