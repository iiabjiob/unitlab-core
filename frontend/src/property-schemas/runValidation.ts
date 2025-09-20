import { validateItem, type ValidationError } from "./validation"

// схемы
import { devicePropertySchema } from "./devicePropertySchema"
import { switchgearPropertySchema } from "./switchgearPropertySchema"
import { sequencePropertySchema } from "./sequencePropertySchema"

// сторы
import { useDeviceStore } from "@/stores/deviceStore"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useSequenceStore } from "@/stores/sequenceStore"

// глобальный стор ошибок
import { useValidationStore } from "@/stores/validationStore"
import { sequenceStepPropertySchema } from "./sequenceStepPropertySchema"

export async function runGlobalValidation() {
  const errors: ValidationError[] = []

  const deviceStore = useDeviceStore()
  const switchgearStore = useSwitchgearStore()
  const sequenceStore = useSequenceStore()

  // Devices
  for (const d of deviceStore.devices) {
    errors.push(...validateItem(devicePropertySchema, d, "device"))
  }

  // Switchgears
  for (const sg of switchgearStore.switchgears) {
    errors.push(...validateItem(switchgearPropertySchema, sg, "switchgear"))
  }

  // Sequences
  for (const seq of sequenceStore.sequences) {
    errors.push(...validateItem(sequencePropertySchema, seq, "sequence"))

    // Sequence steps
    for (const step of seq.steps ?? []) {
      errors.push(...validateItem(sequenceStepPropertySchema, step, "sequence_step"))
    }
  }

  const validationStore = useValidationStore()
  validationStore.setErrors(errors)
}
