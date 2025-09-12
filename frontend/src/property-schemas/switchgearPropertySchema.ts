// src/config/switchgearPropertySchema.ts
import type { PropertySchema } from "@/types/propertySchema"
import type { Switchgear } from "@/types/switchgear"
import { useSwitchgearStore } from "@/stores/switchgearStore"

export const switchgearPropertySchema: PropertySchema<Switchgear> = {
  fields: [
    { key: "title", label: "Title", editable: true, type: "string" },
    { key: "do_open", label: "DO Open", editable: true, type: "channel", channelType: "do" },
    { key: "do_closed", label: "DO Closed", editable: true, type: "channel", channelType: "do" },
    { key: "di_open", label: "DI Open", editable: true, type: "channel", channelType: "di" },
    { key: "di_close", label: "DI Close", editable: true, type: "channel", channelType: "di" },
    { key: "feedback_delay_ms", label: "Feedback Delay, ms", editable: true, type: "number" },
  ],

  async update(item, key, value) {
    const store = useSwitchgearStore()
    await store.updateField(item.id, { [key]: value })
  },
}
