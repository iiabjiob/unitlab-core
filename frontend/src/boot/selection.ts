import { useSelectionStore } from "@/stores/selectionStore"
import { logger } from "@/utils/logger"

export function bootSelection() {
  const sel = useSelectionStore()

  sel.restore()

  logger.info("🎯 Boot: Selection restored:", {
    seq: sel.lastSequenceId,
    dev: sel.lastDeviceId,
    swg: sel.lastSwitchgearId,
    test: sel.lastTestRunId,
  })
}
