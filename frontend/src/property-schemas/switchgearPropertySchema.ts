// src/config/switchgearPropertySchema.ts
import { PROPERTY_FIELD_TYPES, type PropertySchema } from "@/property-schemas/types"
import type { Switchgear } from "@/types/switchgear"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { CHANNEL_TYPES } from "@/types/channel"

export const switchgearPropertySchema: PropertySchema<Switchgear> = {
  fields: [
    {
      key: "title",
      label: "Title",
      editable: true,
      type: PROPERTY_FIELD_TYPES.STRING
    },
    {
      key: "do_open",
      label: "DO Open",
      editable: true,
      type: PROPERTY_FIELD_TYPES.CHANNEL,
      channelType: CHANNEL_TYPES.DO,
      required: true,
    },
    {
      key: "do_closed",
      label: "DO Closed",
      editable: true,
      type: PROPERTY_FIELD_TYPES.CHANNEL,
      channelType: CHANNEL_TYPES.DO,
      required: true,
    },
    {
      key: "di_open",
      label: "DI Open",
      editable: true,
      type: PROPERTY_FIELD_TYPES.CHANNEL,
      channelType: CHANNEL_TYPES.DI
    },
    {
      key: "di_close",
      label: "DI Close",
      editable: true,
      type: PROPERTY_FIELD_TYPES.CHANNEL,
      channelType: CHANNEL_TYPES.DI
    },
    {
      key: "feedback_delay_ms",
      label: "Feedback Delay, ms",
      editable: true,
      type: PROPERTY_FIELD_TYPES.NUMBER
    },
  ],

  async update(item, key, value) {
    const store = useSwitchgearStore()
    await store.updateField(item.id, { [key]: value })
  },
}
