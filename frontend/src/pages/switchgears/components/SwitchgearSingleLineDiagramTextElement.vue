<script setup lang="ts">
import { computed, nextTick, ref, watch } from "vue"

const props = defineProps<{
  id: string
  text: string
  x: number
  y: number
  selected?: boolean
  editing?: boolean
}>()

const emit = defineEmits<{
  (event: "dragStart", pointer: PointerEvent): void
  (event: "select", mouseEvent: MouseEvent): void
  (event: "edit"): void
  (event: "commit", value: string): void
  (event: "cancel"): void
  (event: "contextMenu", mouseEvent: MouseEvent): void
}>()

const inputRef = ref<HTMLInputElement | null>(null)
const draftText = ref(props.text)

const rootStyle = computed(() => ({
  left: `${props.x}px`,
  top: `${props.y}px`,
}))

watch(
  () => props.text,
  (value) => {
    if (!props.editing) {
      draftText.value = value
    }
  },
)

watch(
  () => props.editing,
  async (editing) => {
    if (!editing) {
      return
    }
    draftText.value = props.text
    await nextTick()
    inputRef.value?.focus({ preventScroll: true })
    inputRef.value?.select()
  },
)

function handlePointerDown(event: PointerEvent) {
  if (props.editing) {
    return
  }
  emit("dragStart", event)
}

function commit() {
  emit("commit", draftText.value)
}

function handleKeyDown(event: KeyboardEvent) {
  if (event.key === "Enter") {
    event.preventDefault()
    commit()
    return
  }

  if (event.key === "Escape") {
    event.preventDefault()
    draftText.value = props.text
    emit("cancel")
  }
}
</script>

<template>
  <div
    class="switchgear-sld-text"
    :class="{ 'switchgear-sld-text--selected': selected, 'switchgear-sld-text--editing': editing }"
    :style="rootStyle"
    data-text-root
    @pointerdown.stop="handlePointerDown"
    @click.stop="emit('select', $event)"
    @dblclick.stop="emit('edit')"
    @contextmenu.stop.prevent="emit('contextMenu', $event)"
  >
    <input
      v-if="editing"
      ref="inputRef"
      v-model="draftText"
      class="switchgear-sld-text__input"
      maxlength="80"
      data-text-editor
      @pointerdown.stop
      @click.stop
      @blur="commit"
      @keydown="handleKeyDown"
    />
    <button
      v-else
      type="button"
      class="switchgear-sld-text__button"
      :title="text"
    >
      {{ text }}
    </button>
  </div>
</template>

<style scoped>
.switchgear-sld-text {
  position: absolute;
  transform: translate(-50%, -50%);
}

.switchgear-sld-text__button,
.switchgear-sld-text__input {
  min-width: 4rem;
  max-width: 18rem;
  padding: 0.25rem 0.5rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-300) 64%, transparent);
  border-radius: var(--radius-sm);
  background: color-mix(in srgb, var(--color-white) 86%, transparent);
  color: var(--color-neutral-700);
  font: inherit;
  font-family: var(--font-mono);
  font-size: 0.75rem;
  font-weight: 600;
  letter-spacing: 0;
  line-height: 1.2;
  outline: none;
  text-align: center;
  white-space: nowrap;
  box-shadow:
    0 8px 18px rgb(15 23 42 / 0.1),
    inset 0 1px 0 rgb(255 255 255 / 0.72);
}

.switchgear-sld-text__button {
  overflow: hidden;
  cursor: grab;
  text-overflow: ellipsis;
}

.switchgear-sld-text__button:active {
  cursor: grabbing;
}

.switchgear-sld-text__input {
  width: 12rem;
  cursor: text;
}

.switchgear-sld-text--selected .switchgear-sld-text__button,
.switchgear-sld-text--editing .switchgear-sld-text__input {
  border-color: color-mix(in srgb, var(--color-blue-400) 66%, transparent);
  background: color-mix(in srgb, var(--color-blue-50) 90%, transparent);
  color: var(--color-blue-800);
  box-shadow:
    0 0 0 2px color-mix(in srgb, var(--color-blue-400) 28%, transparent),
    0 10px 22px rgb(37 99 235 / 0.14),
    inset 0 1px 0 rgb(255 255 255 / 0.74);
}

:global(.dark .switchgear-sld-text__button),
:global(.dark .switchgear-sld-text__input) {
  border-color: color-mix(in srgb, var(--color-neutral-700) 70%, transparent);
  background: color-mix(in srgb, var(--color-neutral-900) 86%, transparent);
  color: var(--color-neutral-200);
  box-shadow:
    0 10px 20px rgb(0 0 0 / 0.28),
    inset 0 1px 0 rgb(255 255 255 / 0.05);
}

:global(.dark .switchgear-sld-text--selected .switchgear-sld-text__button),
:global(.dark .switchgear-sld-text--editing .switchgear-sld-text__input) {
  border-color: color-mix(in srgb, var(--color-blue-400) 54%, transparent);
  background: color-mix(in srgb, var(--color-blue-900) 62%, transparent);
  color: var(--color-blue-100);
  box-shadow:
    0 0 0 2px color-mix(in srgb, var(--color-blue-400) 26%, transparent),
    0 12px 24px rgb(14 165 233 / 0.14),
    inset 0 1px 0 rgb(255 255 255 / 0.08);
}
</style>
