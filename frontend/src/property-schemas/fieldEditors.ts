// fieldEditors.ts
import type { Component } from "vue"

import UiSelect from "@/components/ui/UiSelect.vue"
import UnitSelect from "@/components/ui/UnitSelect.vue"
import BitmaskEditor from "@/components/ui/BitmaskEditor.vue"
import ChannelSelect from "@/components/ui/ChannelSelect.vue"

// Registry for field editors
export const fieldEditors: Record<string, Component | string> = {
  string: "input",
  number: "input",
  boolean: "input",
  select: UiSelect,
  unit: UnitSelect,
  bitmask: BitmaskEditor,
  channel: ChannelSelect,
}
