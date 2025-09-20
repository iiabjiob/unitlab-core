// src/stores/validationStore.ts
import { defineStore } from "pinia"
import { ref } from "vue"
import type { ValidationError } from "@/property-schemas/validation"

export const useValidationStore = defineStore("validationStore", () => {
  const errors = ref<ValidationError[]>([])

  // UI state
  const showValidator = ref(false)
  const panelHeight = ref(160) // default size

  function setErrors(newErrors: ValidationError[]) {
    errors.value = newErrors
  }

  function getErrors(schemaName: string, itemId: number | string, fieldKey?: string) {
    return errors.value.filter(e =>
      e.schemaName === schemaName &&
      e.itemId === itemId &&
      (fieldKey ? e.fieldKey === fieldKey : true)
    )
  }

  function clear() {
    errors.value = []
  }

  function toggleValidator() {
    showValidator.value = !showValidator.value
  }

  function setPanelHeight(h: number) {
    panelHeight.value = h
  }

  return {
    errors,
    showValidator,
    panelHeight,
    setErrors,
    getErrors,
    clear,
    toggleValidator,
    setPanelHeight,
  }
}, {

  persist: true,
})
