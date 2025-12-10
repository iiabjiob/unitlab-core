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

function updateChannel(id: number | null) {
	emit("update", { channel_id: id ?? null })
}

function setValue(next: number) {
	if (props.disabled) return
	emit("update", { payload: { value: next } })
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
				Latch state
			</label>
			<div class="mt-2 flex gap-2">
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
