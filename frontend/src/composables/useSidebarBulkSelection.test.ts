import { computed, nextTick, ref } from "vue"
import { describe, expect, it } from "vitest"

import { useSidebarBulkSelection } from "./useSidebarBulkSelection"

type TestItem = {
  id: number
  label: string
}

function createSelection(items: TestItem[]) {
  const source = ref(items)
  const selection = useSidebarBulkSelection(computed(() => source.value))
  return { source, selection }
}

describe("useSidebarBulkSelection", () => {
  it("navigates on plain selection and replaces the selected item", () => {
    const { selection } = createSelection([
      { id: 1, label: "A" },
      { id: 2, label: "B" },
    ])

    expect(selection.handleSelection(1)).toBe("navigate")
    expect(selection.selectedIds.value).toEqual([1])

    expect(selection.handleSelection(2)).toBe("navigate")
    expect(selection.selectedIds.value).toEqual([2])
  })

  it("toggles items without navigation on ctrl selection", () => {
    const { selection } = createSelection([
      { id: 1, label: "A" },
      { id: 2, label: "B" },
    ])

    expect(selection.handleSelection(1, { ctrlKey: true } as MouseEvent)).toBe("select-only")
    expect(selection.selectedIds.value).toEqual([1])

    expect(selection.handleSelection(2, { ctrlKey: true } as MouseEvent)).toBe("select-only")
    expect(selection.selectedIds.value).toEqual([1, 2])

    selection.handleSelection(1, { ctrlKey: true } as MouseEvent)
    expect(selection.selectedIds.value).toEqual([2])
  })

  it("selects a range from the anchor without navigation", () => {
    const { selection } = createSelection([
      { id: 1, label: "A" },
      { id: 2, label: "B" },
      { id: 3, label: "C" },
      { id: 4, label: "D" },
    ])

    selection.handleSelection(2)
    expect(selection.handleSelection(4, { shiftKey: true } as MouseEvent)).toBe("select-only")

    expect(selection.selectedIds.value).toEqual([2, 3, 4])
  })

  it("prunes deleted items from the selection", async () => {
    const { source, selection } = createSelection([
      { id: 1, label: "A" },
      { id: 2, label: "B" },
      { id: 3, label: "C" },
    ])

    selection.handleSelection(1, { ctrlKey: true } as MouseEvent)
    selection.handleSelection(2, { ctrlKey: true } as MouseEvent)

    source.value = [{ id: 3, label: "C" }]
    await nextTick()

    expect(selection.selectedIds.value).toEqual([])
  })
})
