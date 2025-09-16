// src/config/sequencePropertySchema.ts
import type { PropertySchema } from "@/types/propertySchema"
import type { SequenceDef } from "@/types/sequences"
import { useSequenceStore } from "@/stores/sequenceStore"

export const sequencePropertySchema: PropertySchema<SequenceDef> = {
  fields: [
    { key: "name", label: "Name", editable: true, type: "string" },
    { key: "description", label: "Description", editable: true, type: "string" },
    // steps сюда обычно не добавляем, потому что это массив сложных объектов
    // их редактирование будет через отдельный редактор
  ],

  async update(item, key, value) {
    const store = useSequenceStore()
    // await store.update(item.id as any, { [key]: value })
  },
}
