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
    class="switchgear-control-toolbar"
    :class="{ 'switchgear-control-toolbar--compact': props.compact }"
  >
    <div class="switchgear-control-toolbar__row">
      <div class="switchgear-control-toolbar__commands">
        <span v-if="props.compact" class="switchgear-control-toolbar__compact-title">
          {{ switchgear.name }}
        </span>
        <UiButton
          size="sm"
          variant="success"
          :disabled="!canOpen"
          class="switchgear-control-toolbar__command-button"
          :class="{ 'switchgear-control-toolbar__command-button--compact': props.compact }"
          @click="sendSwitchgearCommand('open')"
        >
          {{ acting === "open" ? "Opening..." : "Open" }}
        </UiButton>

        <UiButton
          size="sm"
          variant="danger"
          :disabled="!canClose"
          class="switchgear-control-toolbar__command-button"
          :class="{ 'switchgear-control-toolbar__command-button--compact': props.compact }"
          @click="sendSwitchgearCommand('close')"
        >
          {{ acting === "close" ? "Closing..." : "Close" }}
        </UiButton>

        <UiButton
          size="sm"
          variant="secondary"
          :disabled="!canUndefined"
          class="switchgear-control-toolbar__command-button"
          :class="{ 'switchgear-control-toolbar__command-button--compact': props.compact }"
          @click="sendSwitchgearCommand('intermediate')"
        >
          {{ acting === "intermediate" ? "Applying..." : "Undefined" }}
        </UiButton>

        <UiButton
          size="sm"
          variant="secondary"
          :disabled="!canUnknown"
          class="switchgear-control-toolbar__command-button"
          :class="{ 'switchgear-control-toolbar__command-button--compact': props.compact }"
          @click="sendSwitchgearCommand('unknown')"
        >
          {{ acting === "unknown" ? "Applying..." : "Unknown" }}
        </UiButton>
      </div>

      <span
        v-if="hasPairTargets && !pairSameUnit"
        class="switchgear-control-toolbar__warning"
      >
        Pair commands require both DO channels on the same unit
      </span>

      <div
        class="switchgear-control-toolbar__state"
        :class="{ 'switchgear-control-toolbar__state--compact': props.compact }"
      >
        <span class="switchgear-control-toolbar__state-label">{{ props.compact ? 'State:' : 'Current state:' }}</span>
        <UiBadge
          :variant="positionVariant"
          class="switchgear-control-toolbar__state-badge"
          :class="{ 'switchgear-control-toolbar__state-badge--compact': props.compact }"
        >
          {{ positionStateLabel }}
        </UiBadge>
      </div>
    </div>
  </section>
</template>

<style scoped>
.switchgear-control-toolbar {
  margin-top: 1rem;
  padding: 1.25rem;
  border: 1px solid var(--color-neutral-200);
  border-radius: 1rem;
  background: color-mix(in srgb, var(--color-white) 80%, transparent);
  box-shadow: var(--shadow-sm);
}

.switchgear-control-toolbar--compact {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.5rem;
  margin-top: 0;
  padding: 0;
  border: 0;
  border-radius: 0;
  background: transparent;
  box-shadow: none;
}

.switchgear-control-toolbar__row {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.75rem;
}

.switchgear-control-toolbar__commands {
  display: grid;
  width: 100%;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 0.5rem;
}

.switchgear-control-toolbar--compact .switchgear-control-toolbar__commands {
  display: flex;
  width: auto;
  flex-wrap: wrap;
  align-items: center;
}

.switchgear-control-toolbar__compact-title {
  margin-right: 0.25rem;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  font-weight: 600;
  letter-spacing: 0;
  text-transform: uppercase;
}

.switchgear-control-toolbar__command-button {
  width: 100%;
  justify-content: center;
}

.switchgear-control-toolbar__command-button--compact {
  min-width: 88px;
  width: auto;
}

.switchgear-control-toolbar__warning {
  color: var(--color-amber-700);
  font-size: var(--text-xs);
}

.switchgear-control-toolbar__state {
  display: inline-flex;
  width: 100%;
  align-items: center;
  gap: 0.5rem;
  color: var(--color-neutral-600);
  font-size: var(--text-sm);
}

.switchgear-control-toolbar__state--compact {
  width: auto;
}

.switchgear-control-toolbar__state-label {
  white-space: nowrap;
}

.switchgear-control-toolbar__state-badge {
  justify-content: center;
}

.switchgear-control-toolbar__state-badge--compact {
  min-width: 100px;
}

@media (min-width: 640px) {
  .switchgear-control-toolbar__commands {
    width: auto;
    display: flex;
    flex-wrap: wrap;
    align-items: center;
  }

  .switchgear-control-toolbar__command-button {
    min-width: 120px;
    width: auto;
  }

  .switchgear-control-toolbar__state {
    width: auto;
    margin-left: auto;
  }

  .switchgear-control-toolbar__state-badge {
    min-width: 120px;
  }
}

:global(.dark .switchgear-control-toolbar) {
  border-color: var(--color-neutral-800);
  background: color-mix(in srgb, var(--color-neutral-900) 80%, transparent);
}

:global(.dark .switchgear-control-toolbar--compact) {
  background: transparent;
}

:global(.dark .switchgear-control-toolbar__compact-title) {
  color: var(--color-neutral-400);
}

:global(.dark .switchgear-control-toolbar__warning) {
  color: var(--color-amber-300);
}

:global(.dark .switchgear-control-toolbar__state) {
  color: var(--color-neutral-300);
}
</style>
