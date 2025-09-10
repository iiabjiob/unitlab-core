// src/stores/editorStore.ts
import { defineStore } from "pinia"
import { ref } from "vue"
import type { Switchgear } from "@/types/switchgear"

type EditorItem = Switchgear // future: union of all element kinds

let seq = 1
function nextTitle() {
  return `Switchgear #${seq++}`
}

function newSwitchgear(): Switchgear {
  return {
    id: crypto.randomUUID(),
    kind: "switchgear",
    title: nextTitle(),

    doOpen: null,
    doClosed: null,
    diOpen: null,
    diClose: null,

    feedbackDelayMs: 0,
  }
}

export const useEditorStore = defineStore("editorStore", () => {
  const items = ref<EditorItem[]>([])

  function addSwitchgear() {
    const sg = newSwitchgear()
    console.log("➕ creating", sg.id, "count before", items.value.length)
    items.value.push(sg)
    console.log("count after", items.value.length, items.value.map(x => x.id))
  }

  function removeById(id: string) {
    items.value = items.value.filter((x) => x.id !== id)
  }

  function updateField<T extends keyof EditorItem>(
    id: string,
    key: T,
    value: EditorItem[T]
  ) {
    const i = items.value.findIndex((x) => x.id === id)
    if (i !== -1) {
      // мутируем существующий объект
      items.value[i][key] = value
    }
  }

  return { items, addSwitchgear, removeById, updateField }
})
