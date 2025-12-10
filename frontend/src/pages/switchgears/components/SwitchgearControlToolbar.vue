<script setup lang="ts">
import { computed, ref } from "vue"
import type { Switchgear } from "@/types/switchgear"
import UiButton from "@/components/ui/UiButton.vue"
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

</script>

<template>
  <div class="py-5 flex flex-col gap-3">

    <!-- CONTROLS -->
    <div class="flex flex-wrap items-center gap-2">
      <UiButton
        size="sm"
        :disabled="!canOpen"
        @click="console.log('Open switchgear')"
      >
        Open
      </UiButton>

      <UiButton
        size="sm"
        variant="danger"
        :disabled="!canClose"
        @click="console.log('Close switchgear')"
      >
        Close
      </UiButton>

      <UiButton
        size="sm"
        variant="secondary"
        :disabled="!canClose"
        @click="console.log('Close switchgear')"
      >
        Intermediate
      </UiButton>

      <UiButton
        size="sm"
        variant="secondary"
        :disabled="!canClose"
        @click="console.log('Close switchgear')"
      >
        Unknown
      </UiButton>

    </div>

  </div>
</template>
