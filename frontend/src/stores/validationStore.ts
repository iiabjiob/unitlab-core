// src/stores/validationStore.ts
import { defineStore } from "pinia"
import { ref } from "vue"
import type { ValidationError } from "@/validators/types"
import type { SchemaName } from "@/property-schemas/types"

export const useValidationStore = defineStore("validationStore", () => {
  const errors = ref<ValidationError[]>([])
  const showValidator = ref(false)
  const showErrorsOnly = ref(false)
  const panelHeight = ref(160)

  function setErrors(newErrors: ValidationError[]) {
    errors.value = newErrors
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

  function replaceItemErrors(schemaName: SchemaName, itemId: string | number, newErrors: ValidationError[]) {
    errors.value = [
      ...errors.value.filter(e => !(e.schemaName === schemaName && e.itemId === itemId)),
      ...newErrors,
    ]
  }

  function removeItemErrors(schemaName: SchemaName, itemId: string | number) {
    errors.value = errors.value.filter(e => !(e.schemaName === schemaName && e.itemId === itemId))
  }


  return {
    errors,
    showValidator,
    showErrorsOnly,
    panelHeight,
    setErrors,
    clear,
    toggleValidator,
    setPanelHeight,
    replaceItemErrors,
    removeItemErrors,
  }
}, { persist: true })

