<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from "vue"
import UiButton from "@/components/ui/UiButton.vue"
import { useChannelStore } from "@/stores/channelStore"
import { buildBitmask, buildToggleBitmask } from "@/utils/channel"

const props = withDefaults(defineProps<{ deviceId: number; unitId: string; disabled?: boolean }>(), {
  disabled: false,
})

const channelStore = useChannelStore()
const PENDING_VISUAL_DEBOUNCE_MS = 120

const doChannels = computed(() =>
  channelStore.channelsByDevice(props.deviceId).filter(ch  => ch.type === "do")
)

const hasDo = computed(() => doChannels.value.length > 0)
const allOn = computed(() => hasDo.value && doChannels.value.every(ch => !!ch.state))
const allOff = computed(() => hasDo.value && doChannels.value.every(ch => !ch.state))
const commandPending = computed(() => channelStore.hasPendingCommandForUnit(props.unitId))
const commandPendingVisible = ref(false)
let pendingVisualTimer: ReturnType<typeof setTimeout> | null = null

watch(commandPending, (pending) => {
  if (pendingVisualTimer) {
    clearTimeout(pendingVisualTimer)
    pendingVisualTimer = null
  }
  if (pending === commandPendingVisible.value) return
  pendingVisualTimer = setTimeout(() => {
    pendingVisualTimer = null
    commandPendingVisible.value = pending
  }, PENDING_VISUAL_DEBOUNCE_MS)
}, { flush: "sync", immediate: true })

onBeforeUnmount(() => {
  if (pendingVisualTimer) clearTimeout(pendingVisualTimer)
})

function setAll(state: boolean) {
  if (!hasDo.value || props.disabled || commandPending.value) return
  const mask = buildBitmask(doChannels.value, state)
  channelStore.sendDoAllCommand(props.deviceId, props.unitId, mask)
}

function toggleAll() {
  if (!hasDo.value || props.disabled || commandPending.value) return
  const mask = buildToggleBitmask(doChannels.value)
  channelStore.sendDoAllCommand(props.deviceId, props.unitId, mask)
}
</script>

<template>
  <div class="device-channel-control-toolbar" :aria-busy="commandPending">
    <UiButton
      size="xs"
      variant="secondary"
      :disabled="props.disabled || commandPendingVisible || !hasDo || allOn"
      :aria-disabled="commandPending || undefined"
      @click="setAll(true)"
    >
      All [ON]
    </UiButton>
    <UiButton
      size="xs"
      variant="secondary"
      :disabled="props.disabled || commandPendingVisible || !hasDo || allOff"
      :aria-disabled="commandPending || undefined"
      @click="setAll(false)"
    >
      All [OFF]
    </UiButton>
    <UiButton
      size="xs"
      variant="secondary"
      :disabled="props.disabled || commandPendingVisible || !hasDo"
      :aria-disabled="commandPending || undefined"
      @click="toggleAll"
    >
      All [TOGGLE]
    </UiButton>
  </div>
</template>

<style scoped>
.device-channel-control-toolbar {
  display: flex;
  flex: 0 0 auto;
  flex-wrap: wrap;
  gap: 0.5rem;
  font-size: var(--text-xs);
}
</style>
