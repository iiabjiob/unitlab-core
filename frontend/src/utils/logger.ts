// src/utils/logger.ts

export enum LogLevel {
  DEBUG = 10,
  INFO = 20,
  WARN = 30,
  ERROR = 40,
  NONE = 100,
}

const LEVEL_NAMES: Record<LogLevel, string> = {
  [LogLevel.DEBUG]: "DEBUG",
  [LogLevel.INFO]:  "INFO ",
  [LogLevel.WARN]:  "WARN ",
  [LogLevel.ERROR]: "ERROR",
  [LogLevel.NONE]:  "NONE ",
}

// читаем уровень из окружения (vite: VITE_LOG_LEVEL)
const ENV_LOG_LEVEL = (import.meta.env.VITE_LOG_LEVEL || "info").toLowerCase()
const LOG_LEVELS: Record<string, LogLevel> = {
  debug: LogLevel.DEBUG,
  info: LogLevel.INFO,
  warn: LogLevel.WARN,
  error: LogLevel.ERROR,
  none: LogLevel.NONE,
}
let currentLevel = LOG_LEVELS[ENV_LOG_LEVEL] ?? LogLevel.INFO

function shouldLog(level: LogLevel): boolean {
  return level >= currentLevel
}

function padSource(source: string): string {
  // всегда 4 символа: WS, APP, API → [WS  ], [APP], [API]
  if (source.length > 4) {
    return source.slice(0, 4).toUpperCase()
  }
  return source.toUpperCase().padEnd(4, " ")
}

function toStr(msg: any): string {
  if (typeof msg === "string") return msg
  try {
    return JSON.stringify(msg)
  } catch {
    return String(msg)
  }
}

function log(level: LogLevel, source: string, message: any, ...optionalParams: any[]) {
  if (!shouldLog(level)) return

  const levelName = LEVEL_NAMES[level]
  const sourceTag = `[${padSource(source)}]`
  const formatted = `${levelName} ${sourceTag} ${toStr(message)}`

  switch (level) {
    case LogLevel.DEBUG: console.debug(formatted, ...optionalParams); break
    case LogLevel.INFO:  console.info(formatted, ...optionalParams); break
    case LogLevel.WARN:  console.warn(formatted, ...optionalParams); break
    case LogLevel.ERROR: console.error(formatted, ...optionalParams); break
  }
}

export function getLogger(source: string) {
  return {
    debug: (msg: any, ...args: any[]) => log(LogLevel.DEBUG, source, msg, ...args),
    info:  (msg: any, ...args: any[]) => log(LogLevel.INFO,  source, msg, ...args),
    warn:  (msg: any, ...args: any[]) => log(LogLevel.WARN,  source, msg, ...args),
    error: (msg: any, ...args: any[]) => log(LogLevel.ERROR, source, msg, ...args),
  }
}

export const logger = getLogger("APP")

logger.info(
  `🗒️ Logger initialized with level=${ENV_LOG_LEVEL} (mode=${import.meta.env.MODE})`
)
