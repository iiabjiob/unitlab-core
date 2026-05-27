<script setup lang="ts">
import { computed, ref, watch } from "vue"
import SignalBackedChannelField from "@/components/signals/SignalBackedChannelField.vue"
import UiButton from "@/components/ui/UiButton.vue"
import type { SequenceStep } from "@/types/sequences"
import { CHANNEL_TYPES } from "@/types/channel"
import type { StepEditorChange } from "./editorTypes"

const props = defineProps<{
	step: SequenceStep
	disabled?: boolean
}>()

const emit = defineEmits<{
	(e: "update", payload: StepEditorChange): void
}>()

const pairChannels = computed<[number | null, number | null]>(() => {
	const ids = props.step.payload?.channel_ids ?? []
	return [ids[0] ?? null, ids[1] ?? null]
})

const state2b = computed(() => Number(props.step.payload?.state2b ?? 0))
const pairSignalIds = computed<[number | null, number | null]>(() => {
	const ids = props.step.payload?.signal_ids ?? []
	const normalize = (value: unknown) => Number.isFinite(Number(value)) ? Number(value) : null
	return [normalize(ids[0]), normalize(ids[1])]
})
const pairSignalKeys = computed<[string | null, string | null]>(() => {
	const keys = props.step.payload?.signal_keys ?? []
	const normalize = (value: unknown) => {
		if (value === null || value === undefined) return null
		const text = String(value).trim()
		return text.length > 0 ? text : null
	}
	return [normalize(keys[0]), normalize(keys[1])]
})

const draftChannels = ref<[number | null, number | null]>([null, null])
const draftSignalIds = ref<[number | null, number | null]>([null, null])
const draftSignalKeys = ref<[string | null, string | null]>([null, null])

watch(
	[pairChannels, pairSignalIds, pairSignalKeys],
	([nextChannels, nextSignalIds, nextSignalKeys]) => {
		draftChannels.value = [...nextChannels] as [number | null, number | null]
		draftSignalIds.value = [...nextSignalIds] as [number | null, number | null]
		draftSignalKeys.value = [...nextSignalKeys] as [string | null, string | null]
	},
	{ immediate: true },
)

function emitPairPayloadPatch() {
	emit("update", {
		payload: {
			channel_ids: [...draftChannels.value],
			signal_ids: [...draftSignalIds.value],
			signal_keys: [...draftSignalKeys.value],
		},
	})
}

function updateChannel(index: 0 | 1, value: number | null) {
	draftChannels.value[index] = value ?? null
	draftSignalIds.value[index] = null
	draftSignalKeys.value[index] = null
	emitPairPayloadPatch()
}

function updateSignal(index: 0 | 1, payload: { signalId: number | null; signalKey: string | null }) {
	draftSignalIds.value[index] = payload.signalId ?? null
	draftSignalKeys.value[index] = payload.signalKey ?? null
	emitPairPayloadPatch()
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
		<div class="sequence-step-form__grid sequence-step-form__grid--two">
			<div>
				<div class="sequence-step-form__label">
					Open output
				</div>
				<SignalBackedChannelField
					class="sequence-step-form__control"
					:channel-id="pairChannels[0]"
					:channel-type="CHANNEL_TYPES.DO"
					:signal-id="pairSignalIds[0]"
					:signal-key="pairSignalKeys[0]"
					:exclude-ids="pairChannels[1] ? [pairChannels[1]] : []"
					:signal-picker-title="'Select pair signal A'"
					:disabled="disabled"
					@update:channelId="value => updateChannel(0, value)"
					@update:signal="value => updateSignal(0, value)"
				/>
			</div>
			<div>
				<div class="sequence-step-form__label">
					Close output
				</div>
				<SignalBackedChannelField
					class="sequence-step-form__control"
					:channel-id="pairChannels[1]"
					:channel-type="CHANNEL_TYPES.DO"
					:signal-id="pairSignalIds[1]"
					:signal-key="pairSignalKeys[1]"
					:exclude-ids="pairChannels[0] ? [pairChannels[0]] : []"
					:signal-picker-title="'Select pair signal B'"
					:disabled="disabled"
					@update:channelId="value => updateChannel(1, value)"
					@update:signal="value => updateSignal(1, value)"
				/>
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
			<p class="sequence-step-form__hint sequence-step-form__hint--spaced">
				Both outputs must be allocated on the same DO unit.
			</p>
		</div>
	</div>
</template>
