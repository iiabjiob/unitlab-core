<script setup lang="ts">
import { computed } from "vue"
import BitmaskEditor from "@/components/ui/BitmaskEditor.vue"
import DevicePickerCombobox from "@/components/ui/DevicePickerCombobox.vue"
import type { SequenceStep } from "@/types/sequences"
import type { StepEditorChange } from "./editorTypes"
import { useDeviceStore } from "@/stores/deviceStore"
import { runStoreBootstrap } from "@/composables/useStoreBootstrap"
import { CHANNEL_TYPES } from "@/types/channel"

const props = defineProps<{
	step: SequenceStep
	disabled?: boolean
}>()

const emit = defineEmits<{
	(e: "update", payload: StepEditorChange): void
}>()

const deviceStore = useDeviceStore()
void runStoreBootstrap(
	["sequence-step-mask-devices"],
	[() => deviceStore.ensureLoaded()],
	{ mode: "settled" },
)

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

function handleDeviceChange(value: number | null) {
	if (props.disabled) return
	const parsed = value === null ? null : Number(value)
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
	<div class="sequence-step-form">
		<div>
			<label for="sequence-step-mask-device" class="sequence-step-form__label">
				Target DO device
			</label>
			<DevicePickerCombobox
				class="sequence-step-form__control sequence-step-form__control--lg"
				id="sequence-step-mask-device"
				:model-value="deviceId"
				:devices="doDevices"
				:allowed-types="[CHANNEL_TYPES.DO]"
				:disabled="disabled"
				@update:modelValue="handleDeviceChange"
			/>
		</div>

		<div>
			<div class="sequence-step-form__label">
				Output bitmask
			</div>
			<BitmaskEditor
				class="sequence-step-form__control"
				:model-value="bitmask"
				:channel-count="channelCount"
				:disabled="disabled"
				@update:modelValue="handleMaskChange"
			/>
			<p class="sequence-step-form__hint sequence-step-form__hint--spaced">
				Each bit represents a DO channel state. Bits beyond the device capacity are ignored.
			</p>
		</div>
	</div>
</template>
