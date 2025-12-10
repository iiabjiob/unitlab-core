<script setup lang="ts">
import { computed } from "vue"
import ChannelSelect from "@/components/ui/ChannelSelect.vue"
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

function updateChannel(id: number | null) {
	emit("update", { channel_id: id ?? null })
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
	<div class="space-y-4">
		<div>
			<label class="text-xs font-semibold text-neutral-500 dark:text-neutral-400">
				Target DO channel
			</label>
			<ChannelSelect
				class="mt-1 w-64"
				:model-value="step.channel_id ?? null"
				:channel-type="CHANNEL_TYPES.DO"
				:disabled="disabled"
				@update:modelValue="updateChannel"
			/>
		</div>

		<div>
			<label class="text-xs font-semibold text-neutral-500 dark:text-neutral-400">
				Pulse state
			</label>
			<div class="mt-2 flex gap-2">
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
			<label class="text-xs font-semibold text-neutral-500 dark:text-neutral-400">
				Pulse duration, ms
			</label>
			<input
				type="number"
				min="0"
				class="mt-1 w-32 rounded border border-neutral-300 px-2 py-1 text-sm
							 dark:border-neutral-700 dark:bg-neutral-800"
				:value="pulseMs"
				:disabled="disabled"
				@change="handlePulseChange"
			/>
		</div>
	</div>
</template>
