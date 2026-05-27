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
				:signal-picker-title="'Select command signal'"
				:disabled="disabled"
				@update:channelId="updateChannel"
				@update:signal="updateSignal"
			/>
		</div>

		<div>
			<div class="sequence-step-form__label">
				Latch state
			</div>
			<div class="sequence-step-form__button-row">
				<UiButton
					v-for="option in [0, 1]"
					:key="option"
					size="xs"
					:variant="currentValue === option ? 'primary' : 'secondary'"
					:disabled="disabled"
					@click="setValue(option)"
				>
					{{ option === 0 ? "Off" : "On" }}
				</UiButton>
			</div>
		</div>
	</div>
</template>
