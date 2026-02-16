import { ref } from "vue"

import { useSequenceStore } from "@/stores/sequenceStore"
import { useToastStore } from "@/stores/toastStore"

export function useSequenceImport() {
	const store = useSequenceStore()
	const toast = useToastStore()
	const importing = ref(false)
	const fileInput = ref<HTMLInputElement | null>(null)

	function openFileDialog() {
		if (importing.value) return
		fileInput.value?.click()
	}

	async function onFileSelected(event: Event) {
		const target = event.target as HTMLInputElement
		const file = target.files?.[0]
		if (!file) return

		importing.value = true
		try {
			const imported = await store.importSequencesFile(file)
			const count = imported.length
			toast.success(count === 1 ? "Instruction imported" : `Imported ${count} instructions`)
		} catch (error) {
			toast.error(error instanceof Error ? error.message : "Failed to import instructions")
		} finally {
			importing.value = false
			target.value = ""
		}
	}

	return {
		importing,
		fileInput,
		openFileDialog,
		onFileSelected,
	}
}
