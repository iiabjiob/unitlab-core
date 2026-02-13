<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { useRoute, useRouter } from "vue-router"
import SidebarListItem from "@/components/ui/SidebarListItem.vue"
import RenameModal from "@/components/ui/RenameModal.vue"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"
import type { SequenceDef } from "@/types/sequences"
import { useSequenceStore } from "@/stores/sequenceStore"
import {
  UiMenu,
  UiMenuContent,
  UiMenuItem,
  type MenuController,
} from "@affino/menu-vue"

const props = defineProps<{
  sequence: SequenceDef
  active: boolean
}>()

const emit = defineEmits<{ (e: "select", id: number): void }>()

const store = useSequenceStore()
const router = useRouter()
const route = useRoute()

const renameOpen = ref(false)
const renameValue = ref(props.sequence.name)
const renaming = ref(false)
const deleteOpen = ref(false)
const menuRef = ref<{ controller?: MenuController } | null>(null)

watch(
  () => props.sequence.name,
  (value) => {
    if (!renameOpen.value) {
      renameValue.value = value
    }
  },
)

const deleteMessage = computed(() => `Instruction "${props.sequence.name}" will be deleted with all steps.`)

function handleSelect() {
  emit("select", props.sequence.id)
}

function openRename() {
  renameValue.value = props.sequence.name
  renameOpen.value = true
}

function cancelRename() {
  renameOpen.value = false
  renameValue.value = props.sequence.name
}

async function confirmRename() {
  const trimmed = renameValue.value.trim()
  if (!trimmed || trimmed === props.sequence.name) {
    renameOpen.value = false
    return
  }

  renaming.value = true
  try {
    await store.updateSequence(props.sequence.id, { name: trimmed })
    renameOpen.value = false
  } finally {
    renaming.value = false
  }
}

async function duplicateSequence() {
  const duplicated = await store.duplicateSequence(props.sequence.id)
  await router.push({ name: "instructions.detail", params: { id: duplicated.id } })
}

async function confirmDelete() {
  await store.deleteSequence(props.sequence.id)
  deleteOpen.value = false
  if (Number(route.params.id) === props.sequence.id) {
    await router.push({ name: "instructions.list" })
  }
}

function openContextMenu(event: MouseEvent) {
  event.preventDefault()
  event.stopPropagation()
  const controller = menuRef.value?.controller
  if (!controller) return
  controller.setAnchor({ x: event.clientX, y: event.clientY, width: 0, height: 0 })
  controller.open("pointer")
}
</script>

<template>
  <UiMenu ref="menuRef">
    <SidebarListItem :active="active" class="relative" @select="handleSelect" @contextmenu="openContextMenu($event)">
      <span class="truncate text-sm">
        {{ sequence.name }}
      </span>
    </SidebarListItem>
    <UiMenuContent>
      <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="openRename">
        Rename
      </UiMenuItem>
      <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="duplicateSequence">
        Duplicate
      </UiMenuItem>
      <UiMenuItem danger @select="deleteOpen = true">
        Delete
      </UiMenuItem>
    </UiMenuContent>
  </UiMenu>

  <RenameModal
    :open="renameOpen"
    title="Rename instruction"
    v-model="renameValue"
    :loading="renaming"
    @cancel="cancelRename"
    @confirm="confirmRename"
  />

  <ConfirmModal
    :open="deleteOpen"
    title="Delete instruction"
    :message="deleteMessage"
    confirm-label="Delete"
    cancel-label="Cancel"
    @cancel="deleteOpen = false"
    @confirm="confirmDelete"
  />
</template>