
import { useThemeStore } from "@/stores/themeStore"
import { logger } from "@/utils/logger"

export function bootTheme() {
  const theme = useThemeStore()

  theme.init()

  logger.info("🎨 Boot: Theme initialized:", theme.currentTheme)
}
