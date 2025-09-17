// config/devicePropertySchema.ts
import type { Device } from "@/types/device"
import { useDeviceStore } from "@/stores/deviceStore"
import type { PropertySchema } from "@/types/propertySchema"
import DeviceChannelsProperties from "@/components/devices/DeviceChannelsProperties.vue"

export const devicePropertySchema: PropertySchema<Device> = {
  fields: [
    { key: "unit_id", label: "ID", editable: false, type: "string" },
    { key: "status", label: "Status", editable: false, type: "string" },
    { key: "firmware_version", label: "Firmware", editable: false, type: "string" },
    { key: "type", label: "Type", editable: false, type: "string" },
    { key: "num_channels", label: "Channels count", editable: false, type: "number" },
    { key: "name", label: "Name", editable: true, type: "string" },
    { key: "location", label: "Location", editable: true, type: "string" },

    // Вставляем список каналов через кастомный компонент
    {
      key: "channels",
      label: "Channels",
      type: "custom",
      component: DeviceChannelsProperties,
      props: (device: Device) => ({ device }),
      editable: false,
    },
  ],

  async update(item, key, value) {
    const store = useDeviceStore()
    await store.updateDeviceField(item.id, { [key]: value })
  },
}
