<template>
  <div class="p-3 border-r border-neutral-200 dark:border-neutral-800 w-56 shrink-0">
    <h3 class="text-sm font-semibold mb-2">Sequences</h3>

    <ButtonComponent class="w-full mb-2" type="secondary" size="sm" @click="addSequence">
      Add Sequence
    </ButtonComponent>

    <ButtonComponent class="w-full" type="secondary" size="sm" @click="triggerImport">
      Import Sequence
    </ButtonComponent>

    <input type="file" ref="fileInput" class="hidden" @change="onImport" />
  </div>
</template>

<script setup lang="ts">
import { ref } from "vue"
import ButtonComponent from "../ui/ButtonComponent.vue"
import { useSequenceStore } from "@/stores/sequenceStore"

const store = useSequenceStore()
const fileInput = ref<HTMLInputElement | null>(null)

async function addSequence() {
  const seq = await store.createSequence({ name: "New Sequence", description: "", steps: [] })
  // можно сразу выбрать новый
}

function triggerImport() {
  fileInput.value?.click()
}

async function onImport(e: Event) {
  const input = e.target as HTMLInputElement
  if (!input.files?.length) return
  const file = input.files[0]

  try {
    const text = await file.text()
    const parsed = JSON.parse(text)
    console.log("Parsed sequence:", parsed)
    const seq = await store.createSequence(parsed)
    console.log("Created sequence:", seq)
  } catch (err) {
    console.error("Import failed:", err)
    alert("Import failed: " + (err as Error).message)
  } finally {
    input.value = "" // reset
  }
}
</script>
