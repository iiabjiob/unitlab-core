import { useUiStore } from "@/stores/uiStorage"
import { logger } from "@/utils/logger"

export function bootUi() {
  const ui = useUiStore()

  ui.restore()

  logger.info("🖥️ Boot: UI restored")
}
