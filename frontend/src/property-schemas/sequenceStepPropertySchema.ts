// src/config/sequenceStepPropertySchema.ts
import type { PropertySchema } from "@/types/propertySchema"
import type { SequenceStep } from "@/types/sequences"
import { useSequenceStore } from "@/stores/sequenceStore"

export const sequenceStepPropertySchema: PropertySchema<SequenceStep> = {
  fields: [
    // { key: "kind", label: "Kind", editable: true, type: "select", options: ["WAIT", "DO_SET", "DO_RESET_ALL"] },
    // { key: "ms", label: "Delay (ms)", editable: true, type: "number", visible: (s) => s.kind === "WAIT" },
    // { key: "unit_id", label: "Unit", editable: true, type: "string", visible: (s) => s.kind !== "WAIT" },
    // { key: "cmd.ch", label: "Channel", editable: true, type: "number", visible: (s) => s.kind === "DO_SET" },
    // { key: "cmd.value", label: "Value", editable: true, type: "number", visible: (s) => s.kind === "DO_SET" },
    // { key: "cmd.bitmask", label: "Bitmask", editable: true, type: "number", visible: (s) => s.kind === "DO_SET" },
  ],

  async update(item, key, value) {
    const store = useSequenceStore()
    // тут нужно вызвать отдельный метод в сторе для обновления шага
    // await store.updateStep(item, key, value)
  },
}
