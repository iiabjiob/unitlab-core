<script setup lang="ts">
import { computed } from "vue"
import BitmaskEditor from "@/components/ui/BitmaskEditor.vue"
import UiSelect from "@/components/ui/UiSelect.vue"
import type { SequenceStep } from "@/types/sequences"
import type { StepEditorChange } from "./editorTypes"
import { useDeviceStore } from "@/stores/deviceStore"
import { CHANNEL_TYPES } from "@/types/channel"

const props = defineProps<{
	step: SequenceStep
	disabled?: boolean
}>()

const emit = defineEmits<{
	(e: "update", payload: StepEditorChange): void
}>()

const deviceStore = useDeviceStore()
void deviceStore.ensureLoaded()

const doDevices = computed(() =>
	deviceStore.devices.filter(device => device.device_type?.toLowerCase() === CHANNEL_TYPES.DO)
)

const deviceId = computed(() => props.step.payload?.device_id ?? null)
const bitmask = computed(() => Number(props.step.payload?.bitmask ?? 0))

function channelCountForDevice(id: number | null): number {
	const device = doDevices.value.find(dev => dev.id === id)
	if (!device) return 0
	const doChannels = device.channels?.filter(ch => ch.type === CHANNEL_TYPES.DO)?.length ?? 0
	if (doChannels > 0) return doChannels
	return device.num_channels ?? 0
}

const channelCount = computed(() => channelCountForDevice(deviceId.value))

const FULL_32_BIT_MASK = 0xffffffff >>> 0

function maskForCount(count: number): number {
	if (count <= 0) return 0
	if (count >= 32) return FULL_32_BIT_MASK
	return (1 << count) - 1
}

function clampMask(value: number, count = channelCount.value): number {
	const mask = maskForCount(count)
	return (value & mask) >>> 0
}

function handleDeviceChange(value: string | number | null) {
	if (props.disabled) return
	const parsed = value === null || value === "" ? null : Number(value)
	const count = channelCountForDevice(parsed)
	const safeMask = clampMask(bitmask.value, count)
	emit("update", { payload: { device_id: parsed, bitmask: parsed ? safeMask : 0 } })
}

function handleMaskChange(value: number) {
	if (props.disabled) return
	emit("update", { payload: { bitmask: clampMask(value) } })
}
</script>

<template>
	<div class="space-y-4">
		<div>
			<label class="text-xs font-semibold text-neutral-500 dark:text-neutral-400">
				Target DO device
			</label>
			<UiSelect
				class="mt-1 w-64"
				:model-value="deviceId ?? ''"
				:disabled="disabled"
				@update:modelValue="handleDeviceChange"
			>
				<option value="">
					— select device —
				</option>
				<option
					v-for="device in doDevices"
					:key="device.id"
					:value="device.id"
				>
					{{ device.display_name }}
				</option>
			</UiSelect>
		</div>

		<div>
			<label class="text-xs font-semibold text-neutral-500 dark:text-neutral-400">
				Output bitmask
			</label>
			<BitmaskEditor
				class="mt-2"
				:model-value="bitmask"
				:channel-count="channelCount"
				:disabled="disabled"
				@update:modelValue="handleMaskChange"
			/>
			<p class="mt-2 text-xs text-neutral-500 dark:text-neutral-400">
				Each bit represents a DO channel state. Bits beyond the device capacity are ignored.
			</p>
		</div>
	</div>
</template>
