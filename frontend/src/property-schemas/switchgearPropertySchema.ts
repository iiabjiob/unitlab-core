// src/config/switchgearPropertySchema.ts
import type { PropertySchema } from "@/types/propertySchema"
import type { Switchgear } from "@/types/switchgear"
import { useEditorStore } from "@/stores/editorStore"

export const switchgearPropertySchema: PropertySchema<Switchgear> = {
  fields: [
    { key: "title", label: "Title", editable: true, type: "string" },
    { key: "doOpen", label: "DO Open", editable: true, type: "channel", channelType: "do" },
    { key: "doClosed", label: "DO Closed", editable: true, type: "channel", channelType: "do" },
    { key: "diOpen", label: "DI Open", editable: true, type: "channel", channelType: "di" },
    { key: "diClose", label: "DI Close", editable: true, type: "channel", channelType: "di" },
    { key: "feedbackDelayMs", label: "Feedback Delay, ms", editable: true, type: "number" },
  ],
  async update(item, key, value) {
    const store = useEditorStore()
    store.updateField(item.id, key as keyof Switchgear, value as any)
  },
}
