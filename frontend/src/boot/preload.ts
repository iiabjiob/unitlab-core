import { logger } from "@/utils/logger"
import { useSystemHealthStore } from "@/stores/systemHealthStore"
import { pinia } from "@/stores/pinia"
import { bootSelection } from "./selection"
import { bootTheme } from "./theme"
import { bootUi } from "./ui"
import { bootWebSocket } from "./webSocket"

export function bootPreload() {
  const systemHealthStore = useSystemHealthStore(pinia)

  logger.info("🔌 Boot: Preload…")

  bootTheme()
  bootUi()
  bootSelection()
  bootWebSocket()
  systemHealthStore.startMonitoring()

  logger.info("🚀 Boot: UI + Theme + Selection + WebSocket + Health monitoring are ready")
}
