// src/composables/useSequenceImport.ts
// import { ref } from "vue"
// import { useSequenceStore } from "@/stores/sequenceStore"
// import { ApiBuilder } from "@/utils/api"

// export function useSequenceImport() {
//   const store = useSequenceStore()
//   const importing = ref(false)
//   const fileInput = ref<HTMLInputElement | null>(null)

//   function openFileDialog() {
//     fileInput.value?.click()
//   }

//   async function onFileSelected(e: Event) {
//     const target = e.target as HTMLInputElement
//     const file = target.files?.[0]
//     if (!file) return

//     const form = new FormData()
//     form.append("file", file)

//     importing.value = true
//     try {
//       const res = await fetch(ApiBuilder.sequenceImport(), {
//         method: "POST",
//         body: form,
//       })
//       if (!res.ok) {
//         alert("Failed to import file")
//         return
//       }

//       await store.fetchSequences()
//     } finally {
//       importing.value = false
//       target.value = "" // reset input
//     }
//   }

//   return { importing, fileInput, openFileDialog, onFileSelected }
// }
