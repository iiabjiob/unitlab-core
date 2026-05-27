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
  <div class="switchgear-editor-header">
    <div class="switchgear-editor-header__left">
      <SwitchgearPositionIcon :state="positionState" size="lg" />
      <div class="switchgear-editor-header__main">
        <div class="switchgear-editor-header__title">
          {{ switchgear.name }}
        </div>

        <div class="switchgear-editor-header__meta">
          <span class="switchgear-editor-header__eyebrow">
            Switchgear
          </span>
          <span>·</span>
          <span class="switchgear-editor-header__type">{{ switchgear.switchgear_type }}</span>
          <span>·</span>
          <OnlineStatusComponent
            :status="unitOnline"
            :description="unitStatusDescription"
            neutral-offline
          />
        </div>
      </div>
    </div>

    <UiMenu>
      <UiMenuTrigger asChild>
        <UiButton variant="icon" aria-label="Switchgear actions">
          <EllipsisHorizontalIcon size="24" />
        </UiButton>
      </UiMenuTrigger>

      <UiMenuContent>
        <UiMenuItem class="switchgear-editor-header__menu-item" @select="promptRename">
          Rename
        </UiMenuItem>
        <UiMenuItem class="switchgear-editor-header__menu-item" @select="emit('duplicate')">
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

<style scoped>
.switchgear-editor-header {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  padding: 0.75rem 1rem;
  border-bottom: 1px solid var(--color-neutral-300);
}

.switchgear-editor-header__left {
  display: flex;
  align-items: flex-start;
  gap: 0.75rem;
}

.switchgear-editor-header__main {
  display: flex;
  flex-direction: column;
  gap: 0.25rem;
}

.switchgear-editor-header__title {
  color: var(--color-neutral-900);
  font-size: var(--text-lg);
  font-weight: 500;
  letter-spacing: 0;
}

.switchgear-editor-header__meta {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.75rem;
  color: var(--color-neutral-500);
  font-size: var(--text-sm);
}

.switchgear-editor-header__eyebrow {
  color: var(--color-neutral-400);
  font-size: var(--text-xs);
  letter-spacing: 0;
  text-transform: uppercase;
}

.switchgear-editor-header__type {
  font-size: 0.6875rem;
  letter-spacing: 0;
  text-transform: uppercase;
}

.switchgear-editor-header__menu-item {
  color: var(--color-neutral-900);
}

:global(.dark .switchgear-editor-header) {
  border-bottom-color: var(--color-neutral-800);
}

:global(.dark .switchgear-editor-header__title) {
  color: var(--color-white);
}

:global(.dark .switchgear-editor-header__meta) {
  color: var(--color-neutral-400);
}

:global(.dark .switchgear-editor-header__eyebrow) {
  color: var(--color-neutral-500);
}

:global(.dark .switchgear-editor-header__menu-item) {
  color: var(--color-neutral-200);
}
</style>
