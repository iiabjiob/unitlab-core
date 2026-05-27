<script setup lang="ts">
import { computed } from "vue"
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

const currentValue = computed(() => Number(props.step.payload?.value ?? 0))
const pulseMs = computed(() => Number(props.step.payload?.pulse_ms ?? 0))
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

function setValue(next: number) {
	if (props.disabled) return
	emit("update", { payload: { value: next } })
}

function handlePulseChange(event: Event) {
	const target = event.target as HTMLInputElement | null
	const raw = target?.valueAsNumber ?? Number(target?.value ?? 0)
	const safe = Number.isFinite(raw) ? Math.max(0, raw) : 0
	emit("update", { payload: { pulse_ms: safe } })
}
</script>

<template>
	<div class="sequence-step-form">
		<div>
			<div class="sequence-step-form__label">
				Target DO channel
			</div>
			<SignalBackedChannelField
				class="sequence-step-form__control"
				:channel-id="step.channel_id ?? null"
				:channel-type="CHANNEL_TYPES.DO"
				:signal-id="signalId"
				:signal-key="signalKey"
				:signal-picker-title="'Select pulse signal'"
				:disabled="disabled"
				@update:channelId="updateChannel"
				@update:signal="updateSignal"
			/>
		</div>

		<div>
			<div class="sequence-step-form__label">
				Pulse state
			</div>
			<div class="sequence-step-form__button-row">
				<UiButton
					v-for="option in [0, 1]"
					:key="option"
					size="sm"
					:variant="currentValue === option ? 'primary' : 'secondary'"
					:disabled="disabled"
					@click="setValue(option)"
				>
					{{ option === 0 ? "Low" : "High" }}
				</UiButton>
			</div>
		</div>

		<div>
			<label for="sequence-step-pulse-ms" class="sequence-step-form__label">
				Pulse duration, ms
			</label>
			<input
				type="number"
				autocomplete="off"
				id="sequence-step-pulse-ms"
				name="sequence-step-pulse-ms"
				min="0"
				class="sequence-step-form__input sequence-step-form__control--sm"
				:value="pulseMs"
				:disabled="disabled"
				@change="handlePulseChange"
			/>
		</div>
	</div>
</template>
