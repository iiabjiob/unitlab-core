import { nextTick, ref, type Ref } from "vue"

interface UseTablePanelsOptions {
  focusContainer: () => void
  emitGroupToggle: (open: boolean) => void
}

interface UseTablePanelsResult {
  showVisibilityPanel: Ref<boolean>
  showGroupFilterPanel: Ref<boolean>
  toggleVisibilityPanel: () => void
  openVisibilityPanel: () => void
  closeVisibilityPanel: () => void
  toggleGroupFilterPanel: () => void
  closeGroupFilterPanel: () => void
}

export function useTablePanels(options: UseTablePanelsOptions): UseTablePanelsResult {
  const showVisibilityPanel = ref(false)
  const showGroupFilterPanel = ref(false)

  const focusAfterClose = () => {
    nextTick(() => options.focusContainer())
  }

  const emitGroupToggle = (open: boolean) => {
    options.emitGroupToggle(open)
  }

  const toggleVisibilityPanel = () => {
    const next = !showVisibilityPanel.value
    showVisibilityPanel.value = next
    if (next) {
      showGroupFilterPanel.value = false
      emitGroupToggle(false)
      return
    }
    focusAfterClose()
  }

  const openVisibilityPanel = () => {
    if (showVisibilityPanel.value) return
    showVisibilityPanel.value = true
    showGroupFilterPanel.value = false
    emitGroupToggle(false)
  }

  const closeVisibilityPanel = () => {
    if (!showVisibilityPanel.value) return
    showVisibilityPanel.value = false
    focusAfterClose()
  }

  const toggleGroupFilterPanel = () => {
    const next = !showGroupFilterPanel.value
    showGroupFilterPanel.value = next
    emitGroupToggle(next)
    if (next) {
      showVisibilityPanel.value = false
      return
    }
    focusAfterClose()
  }

  const closeGroupFilterPanel = () => {
    if (!showGroupFilterPanel.value) return
    showGroupFilterPanel.value = false
    emitGroupToggle(false)
    focusAfterClose()
  }

  return {
    showVisibilityPanel,
    showGroupFilterPanel,
    toggleVisibilityPanel,
    openVisibilityPanel,
    closeVisibilityPanel,
    toggleGroupFilterPanel,
    closeGroupFilterPanel,
  }
}
