<script setup lang="ts">
import { computed } from "vue"
import UiButton from "@/components/ui/UiButton.vue"
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
  UiMenuLabel,
  UiMenuSeparator,
} from "@/components/ui/menu"
import { useSequenceStore } from "@/stores/sequenceStore"
import { SequenceStepType } from "@/types/sequences"
import type { SequenceStepCreate } from "@/types/sequences"

const props = defineProps<{
  sequenceId: number
}>()

const emit = defineEmits<{
  (e: "add", payload: SequenceStepCreate): void
}>()

interface MenuDef {
  type: SequenceStepType
  label: string
  section: "timing" | "digital" | "analog" | "flow"
}

const sequenceStore = useSequenceStore()

const items: MenuDef[] = [
  { type: SequenceStepType.WAIT, label: "Wait", section: "timing" },
  { type: SequenceStepType.DO_LATCH, label: "Latch", section: "digital" },
  { type: SequenceStepType.DO_PULSE, label: "Pulse", section: "digital" },
  { type: SequenceStepType.DO_PAIR, label: "Switch pos", section: "digital" },
  { type: SequenceStepType.DO_BITMASK, label: "Group ctrl", section: "digital" },
  { type: SequenceStepType.AO_SET, label: "AO", section: "analog" },
  { type: SequenceStepType.CALL_SEQUENCE, label: "Call", section: "flow" },
  { type: SequenceStepType.REPEAT_SEQUENCE, label: "Repeat", section: "flow" },
]

function add(type: SequenceStepType) {
  const fallbackTargetSequenceId = sequenceStore.sequences.find(
    (sequence) => sequence.id !== props.sequenceId,
  )?.id ?? null

  if (type === SequenceStepType.CALL_SEQUENCE) {
    emit("add", {
      sequence_step_type: type,
      payload: {
        target_sequence_id: fallbackTargetSequenceId,
      },
    })
    return
  }

  if (type === SequenceStepType.REPEAT_SEQUENCE) {
    emit("add", {
      sequence_step_type: type,
      payload: {
        target_sequence_id: fallbackTargetSequenceId,
        repeat_mode: "times",
        iterations: 1,
      },
    })
    return
  }

  emit("add", { sequence_step_type: type })
}

const timingItems = computed(() => items.filter((item) => item.section === "timing"))
const digitalItems = computed(() => items.filter((item) => item.section === "digital"))
const analogItems = computed(() => items.filter((item) => item.section === "analog"))
const flowItems = computed(() => items.filter((item) => item.section === "flow"))
</script>

<template>
  <div class="flex items-center px-1 py-2">
    <UiMenu>
      <UiMenuTrigger asChild>
        <UiButton variant="secondary">
          Add step
        </UiButton>
      </UiMenuTrigger>
      <UiMenuContent align="start" class="min-w-[220px]">
        <UiMenuLabel class="text-[11px] uppercase tracking-[0.18em] text-neutral-500">
          Timing
        </UiMenuLabel>
        <UiMenuItem
          v-for="item in timingItems"
          :key="item.type"
          class="text-neutral-900 dark:text-neutral-200"
          @select="add(item.type)"
        >
          {{ item.label }}
        </UiMenuItem>

        <UiMenuSeparator />
        <UiMenuLabel class="text-[11px] uppercase tracking-[0.18em] text-neutral-500">
          Digital Outputs
        </UiMenuLabel>
        <UiMenuItem
          v-for="item in digitalItems"
          :key="item.type"
          class="text-neutral-900 dark:text-neutral-200"
          @select="add(item.type)"
        >
          {{ item.label }}
        </UiMenuItem>

        <UiMenuSeparator />
        <UiMenuLabel class="text-[11px] uppercase tracking-[0.18em] text-neutral-500">
          Analog
        </UiMenuLabel>
        <UiMenuItem
          v-for="item in analogItems"
          :key="item.type"
          class="text-neutral-900 dark:text-neutral-200"
          @select="add(item.type)"
        >
          {{ item.label }}
        </UiMenuItem>

        <UiMenuSeparator />
        <UiMenuLabel class="text-[11px] uppercase tracking-[0.18em] text-neutral-500">
          Control Flow
        </UiMenuLabel>
        <UiMenuItem
          v-for="item in flowItems"
          :key="item.type"
          class="text-neutral-900 dark:text-neutral-200"
          @select="add(item.type)"
        >
          {{ item.label }}
        </UiMenuItem>
      </UiMenuContent>
    </UiMenu>
  </div>
</template>
