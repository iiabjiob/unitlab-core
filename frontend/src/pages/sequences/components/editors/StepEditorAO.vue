<script setup lang="ts">
import { computed } from "vue"
import SignalBackedChannelField from "@/components/signals/SignalBackedChannelField.vue"
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

const value = computed(() => Number(props.step.payload?.value ?? 0))
const signalId = computed(() => {
	const raw = props.step.payload?.signal_id
	return Number.isFinite(Number(raw)) ? Number(raw) : null
})
const signalKey = computed(() => {
	const raw = props.step.payload?.signal_key
	if (raw === null || raw === undefined) return null
	const normalized = String(raw).trim()
	return normalized.length > 0 ? normalized : null
})

function updateChannel(id: number | null) {
	emit("update", { channel_id: id ?? null })
}

function updateSignal(payload: { signalId: number | null; signalKey: string | null }) {
	emit("update", {
		payload: {
			signal_id: payload.signalId,
			signal_key: payload.signalKey,
		},
	})
}

function handleValueChange(event: Event) {
	const target = event.target as HTMLInputElement | null
	const raw = target?.valueAsNumber ?? Number(target?.value ?? 0)
	const safe = Number.isFinite(raw) ? raw : 0
	emit("update", { payload: { value: safe } })
}
</script>

<template>
	<div class="sequence-step-form">
		<div>
			<div class="sequence-step-form__label">
				AO channel
			</div>
			<SignalBackedChannelField
				class="sequence-step-form__control"
				:channel-id="step.channel_id ?? null"
				:channel-type="CHANNEL_TYPES.AO"
				:signal-id="signalId"
				:signal-key="signalKey"
				:signal-picker-title="'Select analog signal'"
				:disabled="disabled"
				@update:channelId="updateChannel"
				@update:signal="updateSignal"
			/>
		</div>

		<div>
			<label for="sequence-step-ao-value" class="sequence-step-form__label">
				Output value (mA)
			</label>
			<input
				type="number"
				autocomplete="off"
				id="sequence-step-ao-value"
				name="sequence-step-ao-value"
				step="0.01"
				class="sequence-step-form__input sequence-step-form__control--sm"
				:value="value"
				:disabled="disabled"
				@change="handleValueChange"
			/>
		</div>
		<p class="sequence-step-form__hint">
			Specify the analog output setpoint in milliamps.
		</p>
	</div>
</template>
