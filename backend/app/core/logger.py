import logging
import os
from app.core.config import get_settings  # Импорт конфигурации

# Получаем настройки из .env
settings = get_settings()
LOG_DIR = "logs"

# Создаём папку для логов, если её нет
os.makedirs(LOG_DIR, exist_ok=True)

# Создаём логгер
logger = logging.getLogger("app")

# Устанавливаем уровень логирования
LOG_LEVELS = {
    "debug": logging.DEBUG,
    "info": logging.INFO,
    "warning": logging.WARNING,
    "error": logging.ERROR,
    "critical": logging.CRITICAL
}

log_level = LOG_LEVELS.get(settings.debug_level.lower(), logging.INFO)
logger.setLevel(log_level)

# Создаем обработчик для записи логов в файл
log_filename = os.path.join(LOG_DIR, f"{settings.app_env}.log")  # Файл зависит от окружения
file_handler = logging.FileHandler(log_filename, encoding="utf-8")
file_handler.setLevel(log_level)  # Логируем в файл всё от DEBUG и выше

# Создаем обработчик для вывода логов в консоль
console_handler = logging.StreamHandler()
console_handler.setLevel(logging.INFO if settings.app_env == "production" else log_level)  

# Формат логов
formatter = logging.Formatter("%(asctime)s - %(levelname)s - %(message)s")
file_handler.setFormatter(formatter)
console_handler.setFormatter(formatter)

# Добавляем обработчики к логгеру
logger.addHandler(file_handler)
logger.addHandler(console_handler)

# Отключаем дублирование логов, если FastAPI использует root-логгер
logger.propagate = False

# Пример тестового лога
logger.info(f"Logger initialized with level: {settings.debug_level} (Environment: {settings.app_env})")
