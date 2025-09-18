import type { PropertySchema } from "@/types/propertySchema"
import type { SequenceStep } from "@/types/sequences"
import { useSequenceStore } from "@/stores/sequenceStore"
import { StepKind } from "@/types/sequences"

export const sequenceStepPropertySchema: PropertySchema<SequenceStep> = {
  fields: [
    {
      key: "order_index",
      label: "Step",
      editable: false,
      type: "string",
      display: (s) => String(s.order_index + 1),
    },

    {
      key: "kind",
      label: "Kind",
      editable: true,
      type: "select",
      options: Object.values(StepKind),
      display: (s) => s.kind ?? "n/a",
    },

    // WAIT
    {
      key: "payload.ms",
      label: "Delay (ms)",
      editable: true,
      type: "number",
      visible: (s) => s.kind === StepKind.WAIT,
    },

    // --- DO_LATCH ---
    {
      key: "unit_id",
      label: "Unit",
      editable: true,
      type: "unit",
      visible: (s) => s.kind === StepKind.DO_LATCH,
    },
    {
      key: "payload.ch",
      label: "Channel",
      editable: true,
      type: "channel",
      channelType: "do",
      visible: (s) => s.kind === StepKind.DO_LATCH },
    {
      key: "payload.value",
      label: "Value",
      editable: true,
      type: "select",
      options: ["Off", "On"],
      visible: (s) => s.kind === StepKind.DO_LATCH,
    },

    // --- DO_PULSE ---
    {
      key: "unit_id",
      label: "Unit",
      editable: true,
      type: "unit",
      visible: (s) => s.kind === StepKind.DO_PULSE,
    },
    { key: "payload.ch", label: "Channel", editable: true, type: "channel", channelType: "do", visible: (s) => s.kind === StepKind.DO_PULSE },
    {
      key: "payload.value",
      label: "Value",
      editable: true,
      type: "select",
      options: ["Off", "On"],
      visible: (s) => s.kind === StepKind.DO_PULSE,
    },
    {
      key: "payload.pulse_ms",
      label: "Pulse duration (ms)",
      editable: true,
      type: "number",
      visible: (s) => s.kind === StepKind.DO_PULSE,
    },

    // --- DO_BITMASK ---
    {
      key: "unit_id",
      label: "Unit",
      editable: true,
      type: "unit",
      visible: (s) => s.kind === StepKind.DO_BITMASK,
    },
    {
      key: "payload.bitmask",
      label: "Bitmask",
      editable: true,
      type: "bitmask",
      visible: (s) => s.kind === StepKind.DO_BITMASK,
    },

    // --- DO_PAIR ---
    {
      key: "unit_id",
      label: "Unit",
      editable: true,
      type: "unit",
      visible: (s) => s.kind === StepKind.DO_PAIR,
    },
    { key: "payload.chA", label: "Channel A", editable: true, type: "channel", channelType: "do", visible: (s) => s.kind === StepKind.DO_PAIR },
    { key: "payload.chB", label: "Channel B", editable: true, type: "channel", channelType: "do", visible: (s) => s.kind === StepKind.DO_PAIR },
    {
      key: "payload.state2b",
      label: "State (2-bit)",
      editable: true,
      type: "number",
      visible: (s) => s.kind === StepKind.DO_PAIR,
    },

    // --- AO_SET ---
    {
      key: "unit_id",
      label: "Unit",
      editable: true,
      type: "unit",
      visible: (s) => s.kind === StepKind.AO_SET,
    },
    { key: "payload.ch", label: "Channel", editable: true, type: "channel", channelType: "ao", visible: (s) => s.kind === StepKind.AO_SET },
    { key: "payload.value", label: "Value (4–20 mA)", editable: true, type: "number", visible: (s) => s.kind === StepKind.AO_SET },
  ],

  async update(item, key, value) {
    const store = useSequenceStore()
    if (key.toString().startsWith("payload.")) {
      const payloadKey = key.toString().split(".")[1]
      const newPayload = { ...(item.payload ?? {}), [payloadKey]: value }
      await store.updateStep(item.sequence_id, item.id, { payload: newPayload })
    } else {
      await store.updateStep(item.sequence_id, item.id, { [key]: value })
    }
  },
}
