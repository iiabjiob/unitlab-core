<script setup lang="ts">
import { computed, onMounted } from "vue"
import UiButton from "@/components/ui/UiButton.vue"
import UiAffinoListbox from "@/components/ui/UiAffinoListbox.vue"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useChannelStore } from "@/stores/channelStore"
import type { SequenceStep } from "@/types/sequences"
import type { StepEditorChange } from "./editorTypes"

const props = defineProps<{
	step: SequenceStep
	disabled?: boolean
}>()

const emit = defineEmits<{
	(e: "update", payload: StepEditorChange): void
}>()

const switchgearStore = useSwitchgearStore()
const channelStore = useChannelStore()

const pairChannelLabels = computed(() => {
	const ids = props.step.payload?.channel_ids ?? []
	return ids.slice(0, 2).map((id) => {
		const channel = channelStore.channels.find((item) => item.id === id)
		return channel ? channelStore.resolveChannelFullLabel(channel) : `CH#${id}`
	})
})

const switchgearId = computed(() => {
	const raw = props.step.payload?.switchgear_id
	return Number.isFinite(Number(raw)) ? Number(raw) : null
})

const configuredSwitchgears = computed(() => switchgearStore.switchgears.filter((switchgear) => {
	const open = switchgear.bindings.find((binding) => binding.role === "do_open")?.channel_id
	const closed = switchgear.bindings.find((binding) => binding.role === "do_closed")?.channel_id
	return open !== null && open !== undefined && closed !== null && closed !== undefined && open !== closed
}))

const switchgearOptions = computed(() => configuredSwitchgears.value.map((switchgear) => ({
	value: switchgear.id,
	label: `${switchgear.name} · ${switchgear.switchgear_type}`,
})))

const state2b = computed(() => Number(props.step.payload?.state2b ?? 0))
onMounted(() => {
	void switchgearStore.ensureLoaded().catch(() => undefined)
})

function updateSwitchgear(value: string | number | null) {
	const id = Number(value)
	const selected = configuredSwitchgears.value.find((switchgear) => switchgear.id === id)
	if (!selected) {
		return
	}

	const openChannelId = selected.bindings.find((binding) => binding.role === "do_open")?.channel_id ?? null
	const closedChannelId = selected.bindings.find((binding) => binding.role === "do_closed")?.channel_id ?? null
	if (openChannelId === null || closedChannelId === null || openChannelId === closedChannelId) {
		return
	}

	emit("update", {
		payload: {
			switchgear_id: selected.id,
			channel_ids: [openChannelId, closedChannelId],
			signal_ids: [null, null],
			signal_keys: [null, null],
		},
	})
}

function setState(next: number) {
  if (props.disabled) return
  emit("update", { payload: { state2b: next } })
}

const stateOptions = [
	{ value: 1, label: "Open" },
	{ value: 2, label: "Closed" },
	{ value: 0, label: "Unknown" },
	{ value: 3, label: "Undefined" },
]
</script>

<template>
	<div class="sequence-step-form">
		<div>
				<div class="sequence-step-form__label">
					Switchgear
				</div>
				<UiAffinoListbox
					class="sequence-step-form__control"
					:model-value="switchgearId"
					:options="switchgearOptions"
					:disabled="disabled || switchgearStore.loading"
					placeholder="Select configured switchgear"
					aria-label="Configured switchgear"
					@update:modelValue="updateSwitchgear"
				/>
				<div v-if="pairChannelLabels.length" class="sequence-step-form__configured-pair">
					{{ pairChannelLabels.join(" + ") }}
				</div>
			</div>

		<div>
			<div class="sequence-step-form__label">
				Switch position
			</div>
			<div class="sequence-step-form__button-row">
				<UiButton
					v-for="option in stateOptions"
					:key="option.value"
					size="sm"
					:variant="state2b === option.value ? 'primary' : 'secondary'"
					:disabled="disabled"
					@click="setState(option.value)"
				>
					{{ option.label }}
				</UiButton>
			</div>
		</div>
	</div>
</template>
