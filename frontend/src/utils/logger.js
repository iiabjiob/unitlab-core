import { config } from "@/config"; // Импортируем конфиг с бэкенда

const LOG_LEVELS = ["debug", "info", "warning", "error", "critical"];

function getCurrentLogLevel() {
  if (!config.debug) return null; // Если debug выключен, логов не будет
  const level = config.debug_level?.toLowerCase();
  return LOG_LEVELS.includes(level) ? level : "info"; // Фолбэк на "info" если уровень задан неверно
}

function shouldLog(level) {
  const currentLevel = getCurrentLogLevel();
  if (!currentLevel) return false; // Если debug выключен, ничего не логируем

  const levelIndex = LOG_LEVELS.indexOf(level);
  const currentIndex = LOG_LEVELS.indexOf(currentLevel);

  return levelIndex >= currentIndex; // Логируем только если уровень соответствует настройкам
}

// Logger object
export const logger = {
  debug: (...args) => shouldLog("debug") && console.debug("DEBUG:", ...args),
  info: (...args) => shouldLog("info") && console.info("INFO:", ...args),
  warning: (...args) => shouldLog("warning") && console.warn("WARNING:", ...args),
  error: (...args) => shouldLog("error") && console.error("ERROR:", ...args),
  critical: (...args) => shouldLog("critical") && console.error("CRITICAL:", ...args),
};

// Очистка консоли в production
if (!config.debug) {
  console.clear();
}
