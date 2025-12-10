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

const pairChannels = computed<[number | null, number | null]>(() => {
	const ids = props.step.payload?.channel_ids ?? []
	return [ids[0] ?? null, ids[1] ?? null]
})

const state2b = computed(() => Number(props.step.payload?.state2b ?? 0))

function updateChannel(index: 0 | 1, value: number | null) {
	const next = [...pairChannels.value] as [number | null, number | null]
	next[index] = value ?? null
	emit("update", { payload: { channel_ids: next } })
}

function setState(next: number) {
	if (props.disabled) return
	emit("update", { payload: { state2b: next } })
}
</script>

<template>
	<div class="space-y-4">
		<div class="grid gap-4 md:grid-cols-2">
			<div>
				<label class="text-xs font-semibold text-neutral-500 dark:text-neutral-400">
					Channel A
				</label>
				<ChannelSelect
					class="mt-1 w-full"
					:model-value="pairChannels[0]"
					:channel-type="CHANNEL_TYPES.DO"
					:exclude-ids="pairChannels[1] ? [pairChannels[1]] : []"
					:disabled="disabled"
					@update:modelValue="value => updateChannel(0, value)"
				/>
			</div>
			<div>
				<label class="text-xs font-semibold text-neutral-500 dark:text-neutral-400">
					Channel B
				</label>
				<ChannelSelect
					class="mt-1 w-full"
					:model-value="pairChannels[1]"
					:channel-type="CHANNEL_TYPES.DO"
					:exclude-ids="pairChannels[0] ? [pairChannels[0]] : []"
					:disabled="disabled"
					@update:modelValue="value => updateChannel(1, value)"
				/>
			</div>
		</div>

		<div>
			<label class="text-xs font-semibold text-neutral-500 dark:text-neutral-400">
				Combined state (2-bit)
			</label>
			<div class="mt-2 flex flex-wrap gap-2">
				<UiButton
					v-for="option in [0, 1, 2, 3]"
					:key="option"
					size="sm"
					:variant="state2b === option ? 'primary' : 'secondary'"
					:disabled="disabled"
					@click="setState(option)"
				>
					{{ option }}
				</UiButton>
			</div>
			<p class="mt-2 text-xs text-neutral-500 dark:text-neutral-400">
				Channels must belong to the same DO device. The state value encodes both outputs.
			</p>
		</div>
	</div>
</template>
