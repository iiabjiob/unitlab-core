<!-- src/components/switchgear/SwitchgearActions.vue -->
<template>
  <div class="grid grid-cols-2 gap-2">
    <ButtonComponent type="secondary" :disabled="isCmdDisabled('OPEN')" @click="() => setDoPair('OPEN')">Open</ButtonComponent>
    <ButtonComponent type="secondary" :disabled="isCmdDisabled('CLOSED')" @click="() => setDoPair('CLOSED')">Close</ButtonComponent>
    <ButtonComponent type="secondary" :disabled="isCmdDisabled('UNKNOWN')" @click="() => setDoPair('UNKNOWN')">Unknown</ButtonComponent>
    <ButtonComponent type="secondary" :disabled="isCmdDisabled('INTERMEDIATE')" @click="() => setDoPair('INTERMEDIATE')">Intermediate</ButtonComponent>
  </div>
</template>

<script setup lang="ts">
import ButtonComponent from "@/components/ui/ButtonComponent.vue"
import type { SwitchgearState } from "@/composables/useSwitchgear"

const props = defineProps<{
  isCmdDisabled: (s: SwitchgearState) => boolean
  setDoPair: (s: SwitchgearState) => void
}>()
</script>
