import { PROPERTY_FIELD_TYPES, type PropertySchema } from "@/property-schemas/types"
import type { SequenceStep } from "@/types/sequences"
import { useSequenceStore } from "@/stores/sequenceStore"
import { StepKind } from "@/types/sequences"
import { ON_OFF_OPTIONS } from "@/constants/option"
import { splitKey } from "./utils"
import { CHANNEL_TYPES } from "@/types/channel"
import { useDeviceStore } from "@/stores/deviceStore"
import { SWITCHGEAR_CODE, SWITCHGEAR_OPTIONS } from "@/constants/switchgear"

export const sequenceStepPropertySchema: PropertySchema<SequenceStep> = {
  fields: [
    {
      key: "order_index",
      label: "Step",
      editable: false,
      type: PROPERTY_FIELD_TYPES.STRING,
      display: (s) => String(s.order_index + 1),
    },

    {
      key: "kind",
      label: "Kind",
      editable: true,
      type: PROPERTY_FIELD_TYPES.SELECT,
      options: Object.values(StepKind) as StepKind[],
      display: (s) => s.kind ?? "n/a",
      required: true,
    },

    // WAIT
    {
      key: "payload.ms",
      label: "Delay (ms)",
      editable: true,
      type: PROPERTY_FIELD_TYPES.NUMBER,
      visible: (s) => s.kind === StepKind.WAIT,
    },

    // --- DO_LATCH ---
    // {
    //   key: "unit_id",
    //   label: "Unit",
    //   editable: true,
    //   type: PROPERTY_FIELD_TYPES.UNIT,
    //   visible: (s) => s.kind === StepKind.DO_LATCH,
    // },
    {
      key: "payload.ch",
      label: "Channel",
      editable: true,
      type: PROPERTY_FIELD_TYPES.CHANNEL,
      channelType: CHANNEL_TYPES.DO,
      visible: (s) => s.kind === StepKind.DO_LATCH },
    {
      key: "payload.value",
      label: "Value",
      editable: true,
      type: PROPERTY_FIELD_TYPES.SELECT,
      options: ON_OFF_OPTIONS,
      visible: (s) => s.kind === StepKind.DO_LATCH,
      required: true,
    },

    // --- DO_PULSE ---
    // {
    //   key: "unit_id",
    //   label: "Unit",
    //   editable: true,
    //   type: PROPERTY_FIELD_TYPES.UNIT,
    //   visible: (s) => s.kind === StepKind.DO_PULSE,
    // },
    {
      key: "payload.ch",
      label: "Channel",
      editable: true,
      type: PROPERTY_FIELD_TYPES.CHANNEL,
      channelType: CHANNEL_TYPES.DO,
      visible: (s) => s.kind === StepKind.DO_PULSE,
      required: true,
    },
    {
      key: "payload.value",
      label: "Value",
      editable: true,
      type: PROPERTY_FIELD_TYPES.SELECT,
      options: ON_OFF_OPTIONS,
      visible: (s) => s.kind === StepKind.DO_PULSE,
      required: true,
    },
    {
      key: "payload.pulse_ms",
      label: "Pulse duration (ms)",
      editable: true,
      type: PROPERTY_FIELD_TYPES.NUMBER,
      visible: (s) => s.kind === StepKind.DO_PULSE,
      required: true,
    },

    // --- DO_BITMASK ---
    {
      key: "unit_id",
      label: "Unit",
      editable: true,
      type: PROPERTY_FIELD_TYPES.UNIT,
      visible: (s) => s.kind === StepKind.DO_BITMASK,
      required: true,
    },
    {
      key: "payload.bitmask",
      label: "Bitmask",
      editable: true,
      type: PROPERTY_FIELD_TYPES.BITMASK,
      visible: (s) => s.kind === StepKind.DO_BITMASK,
      resolveChannelCount: (s) => {
        const deviceStore = useDeviceStore()
        const device = deviceStore.devices.find(d => d.unit_id === s.unit_id)
        return device?.channels?.filter(ch => ch.type === CHANNEL_TYPES.DO).length ?? 0
      }
    },

    // --- DO_PAIR ---
    // {
    //   key: "unit_id",
    //   label: "Unit",
    //   editable: true,
    //   type: PROPERTY_FIELD_TYPES.UNIT,
    //   visible: (s) => s.kind === StepKind.DO_PAIR,
    // },
    {
      key: "payload.chA",
      label: "Channel A",
      editable: true,
      type: PROPERTY_FIELD_TYPES.CHANNEL,
      channelType: CHANNEL_TYPES.DO,
      visible: (s) => s.kind === StepKind.DO_PAIR,
      required: true,
    },
    { key: "payload.chB",
      label: "Channel B",
      editable: true,
      type: PROPERTY_FIELD_TYPES.CHANNEL,
      channelType: CHANNEL_TYPES.DO,
      visible: (s) => s.kind === StepKind.DO_PAIR,
      required: true,
    },
    {
      key: "payload.state2b",
      label: "State (2-bit)",
      editable: true,
      type: PROPERTY_FIELD_TYPES.SELECT,
      options: SWITCHGEAR_OPTIONS,
      visible: (s) => s.kind === StepKind.DO_PAIR,
      display: (s) => {
        const entry = Object.entries(SWITCHGEAR_CODE).find(([_, v]) => v === s.payload?.state2b)
        return entry?.[0] ?? "UNKNOWN"
      },
      required: true,
    },

    // --- AO_SET ---
    // {
    //   key: "unit_id",
    //   label: "Unit",
    //   editable: true,
    //   type: PROPERTY_FIELD_TYPES.UNIT,
    //   visible: (s) => s.kind === StepKind.AO_SET,
    // },
    {
      key: "payload.ch",
      label: "Channel",
      editable: true,
      type: PROPERTY_FIELD_TYPES.CHANNEL,
      channelType: CHANNEL_TYPES.AO,
      visible: (s) => s.kind === StepKind.AO_SET,
      required: true,
    },
    {
      key: "payload.value",
      label: "Value (4–20 mA)",
      editable: true,
      type: PROPERTY_FIELD_TYPES.NUMBER,
      visible: (s) => s.kind === StepKind.AO_SET,
      required: true,
    },
  ],

  async update(item, key, value) {
    const store = useSequenceStore()

    const { root, sub } = splitKey(key.toString())

    if (root === "payload") {
      const newPayload = { ...(item.payload ?? {}), [sub]: value }
      await store.updateStep(item.sequence_id, item.id, { payload: newPayload })
    } else {
      await store.updateStep(item.sequence_id, item.id, { [sub]: value })
    }
  },

}
