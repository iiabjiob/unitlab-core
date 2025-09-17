<!-- src/components/switchgear/SwitchgearActions.vue -->
<template>
  <div class="grid grid-cols-2 gap-2">
    <UiButton type="secondary" size="sm" :disabled="isCmdDisabled('OPEN')" @click.stop="() => setDoPair('OPEN')">Open</UiButton>
    <UiButton type="secondary" size="sm" :disabled="isCmdDisabled('CLOSED')" @click.stop="() => setDoPair('CLOSED')">Close</UiButton>
    <UiButton type="secondary" size="sm" :disabled="isCmdDisabled('UNKNOWN')" @click.stop="() => setDoPair('UNKNOWN')">Unknown</UiButton>
    <UiButton type="secondary" size="sm" :disabled="isCmdDisabled('INTERMEDIATE')" @click.stop="() => setDoPair('INTERMEDIATE')">Intermediate</UiButton>
  </div>
</template>

<script setup lang="ts">
import UiButton from "@/components/ui/UiButton.vue"
import type { SwitchgearState } from "@/composables/useSwitchgear"

const props = defineProps<{
  isCmdDisabled: (s: SwitchgearState) => boolean
  setDoPair: (s: SwitchgearState) => void
}>()
</script>
