<script setup lang="ts">
import { computed } from "vue"
import SidebarListItem from "@/components/ui/SidebarListItem.vue"
import type { Switchgear } from "@/types/switchgear"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import SwitchgearPositionIcon from "./SwitchgearPositionIcon.vue"

const props = defineProps<{
  switchgear: Switchgear
  active: boolean
}>()

const switchgearStore = useSwitchgearStore()
const positionState = computed(() => switchgearStore.resolveSwitchgearState(props.switchgear))

const emit = defineEmits<{ (e: "select", id: number): void }>()

function handleSelect() {
  emit("select", props.switchgear.id)
}
</script>

<template>
  <SidebarListItem :active="active" class="relative pr-8" @select="handleSelect">
    <span class="truncate text-sm font-medium">
      {{ switchgear.name }}
    </span>
    <template #suffix>
      <span class="absolute right-3 top-1/2 -translate-y-1/2">
        <SwitchgearPositionIcon :state="positionState" />
      </span>
    </template>
    <template #subtitle>
      {{ switchgear.switchgear_type }}
    </template>
  </SidebarListItem>
</template>
