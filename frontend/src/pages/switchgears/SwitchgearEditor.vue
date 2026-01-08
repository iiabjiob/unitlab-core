<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { useRoute, useRouter } from "vue-router"

import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useSelectionStore } from "@/stores/selectionStore"
import { useViewport } from "@/composables/useViewport"

import SwitchgearEditorHeader from "./components/SwitchgearEditorHeader.vue"
import SwitchgearExecutionLog from "./components/SwitchgearExecutionLog.vue"
import SwitchgearControlToolbar from "./components/SwitchgearControlToolbar.vue"
import SwitchgearBindingsEditor from "./components/SwitchgearBindingsEditor.vue"
import ResizablePanel from "@/components/ui/ResizablePanel.vue"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"

const route = useRoute()
const router = useRouter()
const store = useSwitchgearStore()
const selectionStore = useSelectionStore()

const switchgearId = computed(() => Number(route.params.id))

const switchgear = computed(() =>
  store.switchgears.find(s => s.id === switchgearId.value)
)

watch(
  () => switchgear.value?.id ?? null,
  (id) => {
    selectionStore.selectSwitchgear(id)
  },
  { immediate: true }
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

const { isDesktop } = useViewport()

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

      <div class="mt-5 flex flex-1 min-h-0 flex-col gap-4 overflow-hidden rounded bg-white p-4 shadow dark:bg-neutral-800 sm:p-5 lg:flex-row lg:gap-5">

        <!-- BINDINGS PANEL -->
        <div class="flex flex-col min-h-0 lg:flex-none">
          <ResizablePanel
            v-if="isDesktop"
            class="flex flex-col min-h-0 h-full"
            :switchgear="switchgear"
            placement="left"
            storageKey="switchgear-list-width"
            :minSize="380"
            :defaultSize="380"
            :maxSize="800"
          >
            <SwitchgearBindingsEditor :switchgear="switchgear" />
          </ResizablePanel>

          <div
            v-else
            class="flex-1 min-h-0 overflow-hidden rounded-lg border border-neutral-200 bg-neutral-50 p-4 dark:border-neutral-700 dark:bg-neutral-900"
          >
            <div class="h-full min-h-0 overflow-y-auto">
              <SwitchgearBindingsEditor :switchgear="switchgear" />
            </div>
          </div>
        </div>

        <!-- LOG PANEL -->
        <div class="flex flex-1 min-h-0 flex-col overflow-hidden">
          <div class="flex-1 min-h-0 overflow-hidden">
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
