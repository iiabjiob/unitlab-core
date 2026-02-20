<script setup lang="ts">
import { ref, computed, watch } from "vue"
import type { Switchgear } from "@/types/switchgear"
import UiButton from "@/components/ui/UiButton.vue"
import OnlineStatusComponent from "@/components/misc/OnlineStatusComponent.vue"
import RenameModal from "@/components/ui/RenameModal.vue"
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
} from "@/components/ui/menu"
import EllipsisHorizontalIcon from "@/components/icons/EllipsisHorizontalIcon.vue"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import SwitchgearPositionIcon from "./SwitchgearPositionIcon.vue"

const props = defineProps<{
  switchgear: Switchgear
}>()

const emit = defineEmits<{
  (e: "delete"): void
  (e: "duplicate"): void
}>()

const store = useSwitchgearStore()
const renameOpen = ref(false)
const renameValue = ref(props.switchgear.name)
const renaming = ref(false)

watch(
  () => props.switchgear.name,
  (name) => {
    if (!renameOpen.value) renameValue.value = name
  },
)

function promptRename() {
  renameValue.value = props.switchgear.name
  renameOpen.value = true
}

function cancelRename() {
  renameOpen.value = false
  renameValue.value = props.switchgear.name
}

async function confirmRename() {
  const trimmed = renameValue.value.trim()
  if (!trimmed || trimmed === props.switchgear.name) {
    renameOpen.value = false
    renameValue.value = props.switchgear.name
    return
  }
  renaming.value = true
  try {
    await store.updateField(props.switchgear.id, { name: trimmed })
    renameOpen.value = false
  } finally {
    renaming.value = false
  }
}

const unitOnline = computed(() => (store.isUnitOnline(props.switchgear) ? "online" : "offline"))
const unitStatusDescription = computed(() => {
  if (store.isUnitOnline(props.switchgear)) return null
  return "Unit is offline. Control commands are disabled until reconnect.\nCheck power, wiring and Wi-Fi link to the AP."
})
const positionState = computed(() => store.resolveSwitchgearState(props.switchgear))
</script>

<template>
  <div class="px-4 py-3 flex items-start justify-between border-b border-neutral-300 dark:border-neutral-800">
    <!-- LEFT SIDE -->
    <div class="flex items-start gap-3">
      <SwitchgearPositionIcon :state="positionState" size="lg" />
      <div class="flex flex-col gap-1">

        <div class="text-lg font-medium tracking-tight text-neutral-900 dark:text-white">
          {{ switchgear.name }}
        </div>

        <div class="flex flex-wrap items-center gap-3 text-sm text-neutral-500 dark:text-neutral-400">
          <span class="text-xs uppercase tracking-[0.3em] text-neutral-400 dark:text-neutral-500">
            Switchgear
          </span>
          <span>·</span>
          <span class="uppercase tracking-wide text-[11px]">{{ switchgear.switchgear_type }}</span>
          <span>·</span>
          <OnlineStatusComponent
            :status="unitOnline"
            :description="unitStatusDescription"
            neutral-offline
          />
        </div>
      </div>
    </div>

    <!-- ACTIONS -->
    <UiMenu>
      <UiMenuTrigger asChild>
        <UiButton variant="icon">
          <EllipsisHorizontalIcon size="24" />
        </UiButton>
      </UiMenuTrigger>

      <UiMenuContent>
        <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="promptRename">
          Rename
        </UiMenuItem>
        <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="emit('duplicate')">
          Duplicate
        </UiMenuItem>
        <UiMenuItem danger @select="emit('delete')">
          Delete
        </UiMenuItem>
      </UiMenuContent>
    </UiMenu>
  </div>

  <RenameModal
    :open="renameOpen"
    title="Rename switchgear"
    v-model="renameValue"
    :loading="renaming"
    @cancel="cancelRename"
    @confirm="confirmRename"
  />
</template>
