<script setup lang="ts">
import { onMounted, toRef } from "vue"
import type { Switchgear } from "@/types/switchgear"
import UiButton from "@/components/ui/UiButton.vue"
import UiBadge from "@/components/ui/UiBadge.vue"
import { useChannelStore } from "@/stores/channelStore"
import { runStoreBootstrap } from "@/composables/useStoreBootstrap"
import { useSwitchgearCommandActions } from "@/composables/useSwitchgearCommandActions"

const props = defineProps<{
  switchgear: Switchgear
  compact?: boolean
}>()

const channelStore = useChannelStore()
const {
  acting,
  canClose,
  canOpen,
  canUndefined,
  canUnknown,
  hasPairTargets,
  pairSameUnit,
  positionStateLabel,
  positionVariant,
  sendSwitchgearCommand,
} = useSwitchgearCommandActions(toRef(props, "switchgear"), { source: "switchgear-toolbar" })

onMounted(() => {
  if (channelStore.channels.length > 0) {
    return
  }
  void runStoreBootstrap(
    ["switchgear-toolbar-channels"],
    [() => channelStore.ensureLoaded()],
    { mode: "settled" },
  )
})
</script>

<template>
  <section
    :class="props.compact
      ? 'flex flex-wrap items-center gap-2'
      : 'mt-4 rounded-2xl border border-neutral-200 bg-white/80 p-5 shadow-sm dark:border-neutral-800 dark:bg-neutral-900/80'"
  >
    <div class="flex flex-wrap items-center gap-3">
      <div :class="props.compact ? 'flex flex-wrap items-center gap-2' : 'grid w-full grid-cols-2 gap-2 sm:flex sm:w-auto sm:flex-wrap sm:items-center'">
        <span v-if="props.compact" class="mr-1 text-xs font-semibold uppercase tracking-[0.16em] text-neutral-500 dark:text-neutral-400">
          {{ switchgear.name }}
        </span>
        <UiButton
          size="sm"
          variant="success"
          :disabled="!canOpen"
          :class="props.compact ? 'min-w-[88px] justify-center' : 'w-full justify-center sm:w-auto sm:min-w-[120px]'"
          @click="sendSwitchgearCommand('open')"
        >
          {{ acting === "open" ? "Opening..." : "Open" }}
        </UiButton>

        <UiButton
          size="sm"
          variant="danger"
          :disabled="!canClose"
          :class="props.compact ? 'min-w-[88px] justify-center' : 'w-full justify-center sm:w-auto sm:min-w-[120px]'"
          @click="sendSwitchgearCommand('close')"
        >
          {{ acting === "close" ? "Closing..." : "Close" }}
        </UiButton>

        <UiButton
          size="sm"
          variant="secondary"
          :disabled="!canUndefined"
          :class="props.compact ? 'min-w-[88px] justify-center' : 'w-full justify-center sm:w-auto sm:min-w-[120px]'"
          @click="sendSwitchgearCommand('intermediate')"
        >
          {{ acting === "intermediate" ? "Applying..." : "Undefined" }}
        </UiButton>

        <UiButton
          size="sm"
          variant="secondary"
          :disabled="!canUnknown"
          :class="props.compact ? 'min-w-[88px] justify-center' : 'w-full justify-center sm:w-auto sm:min-w-[120px]'"
          @click="sendSwitchgearCommand('unknown')"
        >
          {{ acting === "unknown" ? "Applying..." : "Unknown" }}
        </UiButton>
      </div>

      <span
        v-if="hasPairTargets && !pairSameUnit"
        class="text-xs text-amber-600 dark:text-amber-300"
      >
        Pair commands require both DO channels on the same unit
      </span>

      <div :class="props.compact ? 'inline-flex items-center gap-2 text-sm text-neutral-600 dark:text-neutral-300' : 'inline-flex w-full items-center gap-2 text-sm text-neutral-600 dark:text-neutral-300 sm:ml-auto sm:w-auto'">
        <span class="whitespace-nowrap">{{ props.compact ? 'State:' : 'Current state:' }}</span>
        <UiBadge :variant="positionVariant" :class="props.compact ? 'inline-flex justify-center min-w-[100px]' : 'inline-flex justify-center sm:min-w-[120px]'">
          {{ positionStateLabel }}
        </UiBadge>
      </div>
    </div>
  </section>
</template>
