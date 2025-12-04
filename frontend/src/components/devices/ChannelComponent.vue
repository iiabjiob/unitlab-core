<template>
  <div
    class="flex items-center w-full border-b py-0.5 border-neutral-300 dark:border-neutral-700 last:border-0 text-xs"
  >
    <!-- Имя канала -->
    <span>{{ label }}</span>

    <!-- Управление DO -->
    <div v-if="channel.type === 'do'" class="flex flex-1 items-center justify-end gap-2">
      <UiButton
        size="xs"
        type="secondary"
        class="min-w-[40px]"
        :disabled="Boolean(!channel.state)"
        @click.stop="$emit('toggle', false)"
      >
        Off
      </UiButton>
      <UiButton
        size="xs"
        type="secondary"
        class="min-w-[40px]"
        :disabled="Boolean(channel.state)"
        @click.stop="$emit('toggle', true)"
      >
        On
      </UiButton>
    </div>

    <!-- AO -->
    <div v-else-if="channel.type === 'ao'" class="flex flex-1 items-center justify-end gap-2">
      <div class="flex items-center gap-1">
        <input
          type="number"
          min="0"
          max="24"
          step="0.01"
          v-model="inputValue"
          @input="onInput"
          @blur="onBlur"
          @keyup.enter="onConfirm"
          class="w-20 text-center px-1 border border-neutral-300 dark:border-neutral-700 rounded text-xs bg-neutral-100 dark:bg-neutral-900 p-0.5"
        />
        <span class="text-neutral-400 text-xs">mA</span>
      </div>
      <UiButton size="xs" type="secondary" @click.stop="onConfirm">
        Set
      </UiButton>
    </div>

    <!-- DI -->
    <div v-else-if="channel.type === 'di'" class="flex flex-1 items-center justify-end gap-2">
      <!-- Только статус -->
    </div>

    <!-- Статус -->
    <span class="ml-4 pl-4 border-l border-neutral-300 dark:border-neutral-700 text-right">
      <template v-if="channel.type === 'do' || channel.type === 'di'">
        {{ channel.state ? "🟢" : "⚪️" }}
      </template>
      <template v-else-if="channel.type === 'ao'">
        <div class="min-w-[80px] text-nowrap">
          {{ formatAoValue(channel.state as number) }}
          <span class="text-xs text-neutral-400 ml-1">mA</span>
        </div>
      </template>
      <template v-else>n/a</template>
    </span>
  </div>
</template>

<script setup lang="ts">
import type { Channel } from "@/types/channel"
import UiButton from "../ui/UiButton.vue"
import { computed, ref } from "vue"
import { formatAoValue, parseAoInput } from "@/utils/channel";
import { useChannelStore } from "@/stores/channelStore";

const channelStore = useChannelStore()

const props = defineProps<{
  channel: Channel
  disabled?: boolean
}>()

const emit = defineEmits(["toggle", "ao-change"])

const label = computed(() => channelStore.resolveChannelLabel(props.channel))

const inputValue = ref(
  typeof props.channel.state === "number"
    ? formatAoValue(props.channel.state)
    : "4.00"
)

function onInput(e: Event) {
  const target = e.target as HTMLInputElement
  inputValue.value = target.value // показываем, что юзер набрал
}

function onBlur() {
  const num = parseAoInput(inputValue.value)
  inputValue.value = formatAoValue(num)
}

function onConfirm() {
  const num = parseAoInput(inputValue.value)
  emit("ao-change", num)
  inputValue.value = formatAoValue(num)
}
</script>
