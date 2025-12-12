<script setup lang="ts">
import { ref, computed, watch } from "vue"
import type { Switchgear } from "@/types/switchgear"
import UiButton from "@/components/ui/UiButton.vue"
import OnlineStatusComponent from "@/components/misc/OnlineStatusComponent.vue"
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
} from "@/components/ui/menu"
import EllipsisHorizontalIcon from "@/components/icons/EllipsisHorizontalIcon.vue"
import { useSwitchgearStore } from "@/stores/switchgearStore"

const props = defineProps<{
  switchgear: Switchgear
}>()

const emit = defineEmits<{
  (e: "delete"): void
}>()

const store = useSwitchgearStore()

const editing = ref(false)
const tempName = ref(props.switchgear.name)

watch(
  () => props.switchgear.name,
  name => {
    if (!editing.value) tempName.value = name
  }
)

function startEdit() {
  editing.value = true
  tempName.value = props.switchgear.name
}

async function saveEdit() {
  const trimmed = tempName.value.trim()
  editing.value = false
  if (trimmed && trimmed !== props.switchgear.name) {
    await store.updateField(props.switchgear.id, { name: trimmed })
  }
}

function cancelEdit() {
  editing.value = false
  tempName.value = props.switchgear.name
}

const unitOnline = computed(() => (store.isUnitOnline(props.switchgear) ? "online" : "offline"))
</script>

<template>
  <div class="px-4 py-3 flex items-start justify-between border-b border-neutral-300 dark:border-neutral-800">
    <!-- LEFT SIDE -->
    <div class="flex flex-col gap-1">

      <!-- Switchgear name -->
      <div class="flex items-center gap-2">
        <div
          v-if="!editing"
          class="text-lg font-medium tracking-tight hover:text-blue-400 cursor-pointer"
          @dblclick="startEdit"
        >
          {{ switchgear.name }}
        </div>

        <input
          v-else
          v-model="tempName"
          @keydown.enter="saveEdit"
          @keydown.esc="cancelEdit"
          @blur="saveEdit"
          class="px-2 py-1 text-sm rounded bg-neutral-800 border border-neutral-600 
                 text-neutral-200 focus:ring-1 focus:ring-blue-500"
          autofocus
        />

        <OnlineStatusComponent :status="unitOnline" />
      </div>

      <!-- Metadata -->
      <div class="text-xs text-neutral-500 leading-normal space-y-0.5">
        <div class="uppercase tracking-wide text-[11px]">
          Type · {{ switchgear.switchgear_type }}
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
        <UiMenuItem danger @select="emit('delete')">
          Delete
        </UiMenuItem>
      </UiMenuContent>
    </UiMenu>
  </div>
</template>
