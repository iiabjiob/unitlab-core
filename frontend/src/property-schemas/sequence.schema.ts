// src/config/sequencePropertySchema.ts
import { PROPERTY_FIELD_TYPES, type PropertySchema } from "@/property-schemas/types"
import type { SequenceDef } from "@/types/sequences"
import { useSequenceStore } from "@/stores/sequenceStore"
import SequenceStepsProperties from "@/components/sequences/SequenceStepsProperties.vue"

export const sequencePropertySchema: PropertySchema<SequenceDef> = {
  fields: [
    {
      key: "name",
      label: "Name",
      editable: true,
      type: PROPERTY_FIELD_TYPES.STRING
    },
    {
      key: "description",
      label: "Description",
      editable: true,
      type: PROPERTY_FIELD_TYPES.STRING
    },
    {
      key: "steps",
      label: "Steps",
      type: PROPERTY_FIELD_TYPES.CUSTOM,
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
