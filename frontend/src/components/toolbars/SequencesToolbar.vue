<template>
  <UiToolbar title="Sequences">
    <template #left>
      <UiButton type="toolbar" size="sm" @click="onAdd">➕ Add</UiButton>

      <UiButton
        type="toolbar"
        size="sm"
        :disabled="importing"
        @click="openFileDialog"
      >
        <span v-if="importing">⏳ Importing…</span>
        <span v-else>📂 Import</span>
      </UiButton>

      <input
        ref="fileInput"
        type="file"
        accept="application/json"
        class="hidden"
        @change="onFileSelected"
      />
    </template>
  </UiToolbar>
</template>

<script setup lang="ts">
import { ApiBuilder } from "@/utils/api"
import UiButton from "../ui/UiButton.vue"
import UiToolbar from "../ui/UiToolbar.vue"
import { useSequenceStore } from "@/stores/sequenceStore"
import { ref } from "vue"

const store = useSequenceStore()
const fileInput = ref<HTMLInputElement | null>(null)
const importing = ref(false)

async function onAdd() {
  const seq = await store.createSequence({
    name: "New Sequence",
    description: "Draft sequence",
    steps: [],
  })
  console.log("Created:", seq)
}

function openFileDialog() {
  fileInput.value?.click()
}

async function onFileSelected(e: Event) {
  const target = e.target as HTMLInputElement
  const file = target.files?.[0]
  if (!file) return

  const form = new FormData()
  form.append("file", file)

  importing.value = true
  try {
    const res = await fetch(ApiBuilder.sequenceImport(), {
      method: "POST",
      body: form,
    })

    if (!res.ok) {
      alert("Failed to import file")
      return
    }

    const imported = await res.json()
    const list = Array.isArray(imported) ? imported : [imported]

    list.forEach(seq => {
      if (seq.steps && Array.isArray(seq.steps)) {
        store.sequences.push(seq)
      } else {
        console.warn("⚠️ Skipped invalid sequence", seq)
      }
    })

    console.log("Imported:", list)
  } finally {
    importing.value = false
    target.value = "" // reset input so same file можно выбрать снова
  }
}
</script>
