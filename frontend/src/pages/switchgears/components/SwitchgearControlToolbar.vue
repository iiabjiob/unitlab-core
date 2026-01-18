<script setup lang="ts">
import { computed, ref } from "vue"
import type { Switchgear } from "@/types/switchgear"
import UiButton from "@/components/ui/UiButton.vue"
import UiBadge from "@/components/ui/UiBadge.vue"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useChannelStore } from "@/stores/channelStore"

const props = defineProps<{
  switchgear: Switchgear
}>()

const switchgearStore = useSwitchgearStore()
const channelStore = useChannelStore()

const acting = ref<null | "open" | "close">(null)

function channelForRole(role: "do_open" | "do_closed" | "di_open" | "di_close") {
  const binding = switchgearStore.bindingByRole(props.switchgear, role)
  if (!binding?.channel_id) return null
  return channelStore.channels.find(ch => ch.id === binding.channel_id) ?? null
}

const doOpenChannel = computed(() => channelForRole("do_open"))
const doCloseChannel = computed(() => channelForRole("do_closed"))


const canOpen = computed(() => !!doOpenChannel.value && acting.value === null)
const canClose = computed(() => !!doCloseChannel.value && acting.value === null)

const positionState = computed(() => switchgearStore.resolveSwitchgearState(props.switchgear))
const positionVariant = computed(() => {
  switch (positionState.value) {
    case "OPEN":
      return "success"
    case "CLOSED":
      return "danger"
    case "INTERMEDIATE":
      return "warning"
    default:
      return "neutral"
  }
})

const unitOnline = computed(() => switchgearStore.isUnitOnline(props.switchgear))
const unitStatusText = computed(() => (unitOnline.value ? "Unit online" : "Unit offline"))

</script>

<template>
  <section class="mt-4 rounded-2xl border border-neutral-200 bg-white/80 p-5 shadow-sm dark:border-neutral-800 dark:bg-neutral-900/80">
    <div class="flex flex-wrap items-center gap-3">
      <div class="flex flex-wrap items-center gap-2">
        <UiButton
          size="sm"
          :disabled="!canOpen"
          class="min-w-[120px] justify-center"
          @click="console.log('Open switchgear')"
        >
          Open
        </UiButton>

        <UiButton
          size="sm"
          variant="danger"
          :disabled="!canClose"
          class="min-w-[120px] justify-center"
          @click="console.log('Close switchgear')"
        >
          Close
        </UiButton>

        <UiButton
          size="sm"
          variant="secondary"
          :disabled="!canClose"
          class="min-w-[120px] justify-center"
          @click="console.log('Close switchgear')"
        >
          Intermediate
        </UiButton>

        <UiButton
          size="sm"
          variant="secondary"
          :disabled="!canClose"
          class="min-w-[120px] justify-center"
          @click="console.log('Close switchgear')"
        >
          Unknown
        </UiButton>
      </div>

      <UiBadge :variant="positionVariant" class="inline-flex min-w-[120px] justify-center">
        {{ positionState }}
      </UiBadge>

      <span class="text-xs text-neutral-500 dark:text-neutral-400">
        {{ unitStatusText }}
      </span>
    </div>
  </section>
</template>
