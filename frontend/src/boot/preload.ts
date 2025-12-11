import { logger } from "@/utils/logger"
import { bootSelection } from "./selection"
import { bootTheme } from "./theme"
import { bootUi } from "./ui"
import { bootWebSocket } from "./webSocket"

export function bootPreload() {
  
  logger.info("🔌 Boot: Preload…")

  bootTheme()
  bootUi()
  bootSelection()
  bootWebSocket()

  logger.info("🚀 Boot: UI + Theme + Selection + WebSocket are ready")
}
