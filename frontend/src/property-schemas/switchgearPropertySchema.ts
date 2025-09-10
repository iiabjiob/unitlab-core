// src/config/switchgearPropertySchema.ts
import type { PropertySchema } from "@/types/propertySchema"
import type { Switchgear } from "@/types/switchgear"
import { useEditorStore } from "@/stores/editorStore"

export const switchgearPropertySchema: PropertySchema<Switchgear> = {
  fields: [
    { key: "title",           label: "Title",            editable: true,  type: "string" },
    { key: "doUnitId",        label: "DO Unit ID",       editable: true,  type: "string" },   // tip: replace with select if your Properties supports it
    { key: "doOpenCh",        label: "DO Open Ch",       editable: true,  type: "number" },
    { key: "doCloseCh",       label: "DO Close Ch",      editable: true,  type: "number" },
    { key: "diUnitId",        label: "DI Unit ID",       editable: true,  type: "string" },
    { key: "diOpenPulseCh",   label: "DI Open Pulse Ch", editable: true,  type: "number" },
    { key: "diClosePulseCh",  label: "DI Close Pulse Ch",editable: true,  type: "number" },
    { key: "feedbackDelayMs", label: "Feedback Delay, ms",editable: true, type: "number" },
  ],
  async update(item, key, value) {
    const store = useEditorStore()
    store.updateField(item.id, key as keyof Switchgear, value as any)
  },
}
