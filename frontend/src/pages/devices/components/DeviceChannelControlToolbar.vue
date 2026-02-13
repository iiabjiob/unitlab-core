<script setup lang="ts">
import { computed } from "vue"
import UiButton from "@/components/ui/UiButton.vue"
import { useChannelStore } from "@/stores/channelStore"
import { buildBitmask, buildToggleBitmask } from "@/utils/channel"

const props = withDefaults(defineProps<{ deviceId: number; unitId: string; disabled?: boolean }>(), {
  disabled: false,
})

const channelStore = useChannelStore()

const doChannels = computed(() =>
  channelStore.channelsByDevice(props.deviceId).filter(ch  => ch.type === "do")
)

const hasDo = computed(() => doChannels.value.length > 0)
const allOn = computed(() => hasDo.value && doChannels.value.every(ch => !!ch.state))
const allOff = computed(() => hasDo.value && doChannels.value.every(ch => !ch.state))

function setAll(state: boolean) {
  if (!hasDo.value || props.disabled) return
  const mask = buildBitmask(doChannels.value, state)
  channelStore.sendDoAllCommand(props.deviceId, props.unitId, mask)
}

function toggleAll() {
  if (!hasDo.value || props.disabled) return
  const mask = buildToggleBitmask(doChannels.value)
  channelStore.sendDoAllCommand(props.deviceId, props.unitId, mask)
}
</script>

<template>
  <div class="flex flex-wrap gap-2 text-xs">
    <UiButton
      size="xs"
      variant="secondary"
      :disabled="props.disabled || !hasDo || allOn"
      @click="setAll(true)"
    >
      All [ON]
    </UiButton>
    <UiButton
      size="xs"
      variant="secondary"
      :disabled="props.disabled || !hasDo || allOff"
      @click="setAll(false)"
    >
      All [OFF]
    </UiButton>
    <UiButton
      size="xs"
      variant="secondary"
      :disabled="props.disabled || !hasDo"
      @click="toggleAll"
    >
      All [TOGGLE]
    </UiButton>
  </div>
</template>
