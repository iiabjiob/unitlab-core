// src/services/validationRunner.ts
import { useValidationStore } from "@/stores/validationStore"
import { validateSwitchgear } from "./switchgear"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import type { SchemaName } from "@/property-schemas/types"

type ValidatorFn<T> = (item: T) => any[]

const validators: Record<SchemaName, () => { items: any[]; fn: ValidatorFn<any> }> = {
  switchgear: () => {
    const store = useSwitchgearStore()
    return { items: store.switchgears, fn: validateSwitchgear }
  },
  device: () => ({ items: [], fn: () => [] }),
  channel: () => ({ items: [], fn: () => [] }),
  sequence: () => ({ items: [], fn: () => [] }),
  sequence_step: () => ({ items: [], fn: () => [] }),
}


export async function validate(schemaName?: SchemaName) {
  const vStore = useValidationStore()
  if (!schemaName) vStore.clear() // если проверяем всё, то очищаем перед началом

  const run = async (name: SchemaName) => {
    const config = validators[name]?.()
    if (!config) return

    for (const item of config.items) {
      const errs = config.fn(item)
      vStore.replaceItemErrors(name, item.id, errs)
    }
  }

  if (schemaName) {
    await run(schemaName)
  } else {
    // если schemaName не указан → валидируем все сущности
    for (const key of Object.keys(validators) as SchemaName[]) {
      await run(key)
    }
  }
}
