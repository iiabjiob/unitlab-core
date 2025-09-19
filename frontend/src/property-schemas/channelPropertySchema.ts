import type { Channel } from "@/types/channel"
import { useChannelStore } from "@/stores/channelStore"
import type { PropertySchema } from "@/property-schemas/types"

export const channelPropertySchema: PropertySchema<Channel> = {
  fields: [
    {
      key: "name",
      label: (ch: Channel, idx?: number) => `CH${(ch.index ?? idx ?? 0) + 1}`,
      editable: true,
      type: "string",
    },
  ],

  async update(item, key, value) {
    const store = useChannelStore()
    await store.updateChannelField(item.id, { [key]: value })
  },
}
