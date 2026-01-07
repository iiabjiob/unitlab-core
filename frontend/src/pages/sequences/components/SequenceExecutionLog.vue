<script setup lang="ts">
import { computed } from "vue"
import ExecutionLogPanel from "@/components/ui/ExecutionLogPanel.vue"
import { useSequenceLogStore } from "@/stores/sequenceLogStore"
import type { SequenceDef, SequenceState } from "@/types/sequences"

const props = defineProps<{
  sequence: SequenceDef
  state: SequenceState
}>()

const logStore = useSequenceLogStore()
const logs = computed(() => logStore.logs[props.sequence.id] ?? [])
</script>

<template>
  <ExecutionLogPanel :logs="logs" />
</template>
