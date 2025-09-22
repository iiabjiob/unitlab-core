import { nextTick } from "vue"
import { useRouter } from "vue-router"
import { useSelectionStore } from "@/stores/selectionStore"
import { VALIDATION_LEVELS, type ValidationError } from "@/validators/types"
import { SCHEMA_NAMES } from "@/property-schemas/types"

export function useFocusField() {
  const router = useRouter()
  const selection = useSelectionStore()

  async function focusField(err: ValidationError) {
    const typeMap: Record<string, { type: string; route: string }> = {
      device:       { type: SCHEMA_NAMES.DEVICE,       route: "/devices" },
      switchgear:   { type: SCHEMA_NAMES.SWITCHGEAR,   route: "/switchgears" },
      sequence:     { type: SCHEMA_NAMES.SEQUENCE,     route: "/sequences" },
      sequence_step:{ type: SCHEMA_NAMES.SEQUENCE_STEP,route: "/sequences" },
      channel:      { type: SCHEMA_NAMES.CHANNEL,      route: "/devices" },
    }

    const map = typeMap[err.schemaName]
    if (!map) return

    // 1. Navigate if needed
    if (router.currentRoute.value.path !== map.route) {
      await router.push(map.route)
    }

    // 2. Select item
    selection.select({ type: map.type as any, key: err.itemId })

    // 3. Wait for render
    await nextTick()

    // 4. Try to find row
    const findRow = (retries = 10, delay = 100): Promise<HTMLElement | null> =>
      new Promise(resolve => {
        const attempt = (n: number) => {
          const rows = document.querySelectorAll("[data-schema]")
          const row = [...rows].find(el => {
            const schema = el.getAttribute("data-schema")
            const item   = el.getAttribute("data-item")
            const field  = el.getAttribute("data-field")

            return schema === err.schemaName &&
                   item === String(err.itemId) &&
                   (
                     field === err.fieldKey ||
                     field?.endsWith("." + err.fieldKey)
                   )
          }) as HTMLElement | undefined

          if (row) {
            resolve(row)
          } else if (n > 0) {
            setTimeout(() => attempt(n - 1), delay)
          } else {
            resolve(null)
          }
        }
        attempt(retries)
      })

    const row = await findRow()
    if (!row) {
      console.warn("Row not found:", {
        schema: err.schemaName,
        item: err.itemId,
        field: err.fieldKey,
      })
      return
    }

    // 5. Scroll into view
    row.scrollIntoView({ behavior: "smooth", block: "center" })

    // 6. Highlight
    const target = row.querySelector(".label-cell") || row
    if (err.level === VALIDATION_LEVELS.WARNING) {
      target.classList.add("validation-focus-warning")
      setTimeout(() => target.classList.remove("validation-focus-warning"), 2000)
    } else {
      target.classList.add("validation-focus-error")
      setTimeout(() => target.classList.remove("validation-focus-error"), 2000)
    }
  }

  return { focusField }
}
