// src/config/sequencePropertySchema.ts
import type { PropertySchema } from "@/types/propertySchema"
import type { SequenceDef } from "@/types/sequences"
import { useSequenceStore } from "@/stores/sequenceStore"
import SequenceStepsProperties from "@/components/sequences/SequenceStepsProperties.vue"

export const sequencePropertySchema: PropertySchema<SequenceDef> = {
  fields: [
    { key: "name", label: "Name", editable: true, type: "string" },
    { key: "description", label: "Description", editable: true, type: "string" },
    {
      key: "steps",
      label: "Steps",
      type: "custom",
      component: SequenceStepsProperties,
      props: (seq: SequenceDef) => ({ sequence: seq }),
      editable: false,
    },
  ],

  async update(item, key, value) {
    const store = useSequenceStore()
    await store.updateSequenceField(item.id, { [key]: value })
  },
}
