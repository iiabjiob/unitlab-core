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
import SwitchgearBindingsSummary from "./components/SwitchgearBindingsSummary.vue"
import ResizablePanel from "@/components/ui/ResizablePanel.vue"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"

const route = useRoute()
const router = useRouter()
const store = useSwitchgearStore()
const selectionStore = useSelectionStore()
const { isDesktop } = useViewport()

const switchgearId = computed(() => Number(route.params.id))
const switchgear = computed(() => (
  store.switchgears.find(item => item.id === switchgearId.value) ?? null
))

const deleteModalOpen = ref(false)
const bindingsEditorOpen = ref(false)

const deleteMessage = computed(() => (
  switchgear.value
    ? `Switchgear "${switchgear.value.name}" will be deleted.`
    : ""
))

watch(
  () => switchgear.value?.id ?? null,
  (id) => {
    selectionStore.selectSwitchgear(id)
    bindingsEditorOpen.value = false
  },
  { immediate: true },
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
  const deletingId = switchgear.value.id
  const before = [...store.switchgears]
  const currentIndex = before.findIndex(item => item.id === deletingId)

  bindingsEditorOpen.value = false
  await store.remove(deletingId)
  deleteModalOpen.value = false

  const after = store.switchgears
  if (after.length === 0) {
    await router.push({ name: "switchgears.list" })
    return
  }

  const fallbackIndex = currentIndex < 0
    ? 0
    : Math.min(currentIndex, after.length - 1)
  const fallback = after[fallbackIndex]

  await router.push({ name: "switchgears.detail", params: { id: fallback.id } })
}
</script>

<template>
  <div class="h-full flex flex-col pe-4">
    <template v-if="switchgear">
      <SwitchgearEditorHeader
        :switchgear="switchgear"
        @duplicate="handleDuplicate"
        @delete="requestDelete"
      />

      <SwitchgearControlToolbar :switchgear="switchgear" />

      <div class="mt-5 flex flex-col gap-4 rounded bg-white p-4 shadow dark:bg-neutral-800 sm:p-5 lg:flex-1 lg:min-h-0 lg:flex-row lg:gap-5 lg:overflow-hidden">
        <div class="flex flex-col lg:min-h-0 lg:flex-none">
          <ResizablePanel
            v-if="isDesktop"
            class="flex flex-col min-h-0 h-full"
            :switchgear="switchgear"
            placement="left"
            storageKey="switchgear-summary-width"
            :minSize="320"
            :defaultSize="360"
            :maxSize="800"
          >
            <SwitchgearBindingsSummary :switchgear="switchgear" @edit="bindingsEditorOpen = true" />
          </ResizablePanel>

          <div
            v-else
            class="rounded-lg border border-neutral-200 bg-neutral-50 p-4 dark:border-neutral-700 dark:bg-neutral-900"
          >
            <div>
              <SwitchgearBindingsSummary :switchgear="switchgear" @edit="bindingsEditorOpen = true" />
            </div>
          </div>
        </div>

        <div class="flex flex-col lg:flex-1 lg:min-h-0 lg:overflow-hidden">
          <SwitchgearBindingsEditor
            v-if="bindingsEditorOpen"
            class="mb-4"
            :switchgear="switchgear"
            @close="bindingsEditorOpen = false"
          />

          <div class="lg:flex-1 lg:min-h-0 lg:overflow-hidden">
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
