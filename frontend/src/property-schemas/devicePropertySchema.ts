// config/devicePropertySchema.ts
import type { Device } from "@/types/device"
import { useDeviceStore } from "@/stores/deviceStore"
import { PROPERTY_FIELD_TYPES, type PropertySchema } from "@/property-schemas/types"
import DeviceChannelsProperties from "@/components/devices/DeviceChannelsProperties.vue"

export const devicePropertySchema: PropertySchema<Device> = {
  fields: [
    {
      key: "unit_id",
      label: "ID",
      editable: false,
      type: PROPERTY_FIELD_TYPES.STRING
    },
    {
      key: "status",
      label: "Status",
      editable: false,
      type: PROPERTY_FIELD_TYPES.STRING
    },
    {
      key: "firmware_version",
      label: "Firmware",
      editable: false,
      type: PROPERTY_FIELD_TYPES.STRING
    },
    {
      key: "type",
      label: "Type",
      editable: false,
      type: PROPERTY_FIELD_TYPES.STRING
    },
    {
      key: "num_channels",
      label: "Channels count",
      editable: false,
      type: PROPERTY_FIELD_TYPES.NUMBER
    },
    {
      key: "name",
      label: "Name",
      editable: true,
      type: PROPERTY_FIELD_TYPES.STRING
    },
    {
      key: "location",
      label: "Location",
      editable: true,
      type: PROPERTY_FIELD_TYPES.STRING
    },

    // Вставляем список каналов через кастомный компонент
    {
      key: "channels",
      label: "Channels",
      type: PROPERTY_FIELD_TYPES.CUSTOM,
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
