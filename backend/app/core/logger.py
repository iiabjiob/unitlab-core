import logging
import os
from app.core.config import get_settings  # Импортируем конфиг

# Получаем настройки из .env
settings = get_settings()
LOG_DIR = "logs"

# Создаём папку для логов, если её нет
os.makedirs(LOG_DIR, exist_ok=True)

# Создаём логгер
logger = logging.getLogger("app")

# Устанавливаем уровень логирования в зависимости от DEBUG
log_level = logging.DEBUG if settings.debug else logging.INFO
logger.setLevel(log_level)

# Создаем обработчик для записи логов в файл
file_handler = logging.FileHandler(os.path.join(LOG_DIR, "app.log"), encoding="utf-8")
file_handler.setLevel(log_level)  # Логируем в файл все от DEBUG и выше

# Создаем обработчик для вывода логов в консоль
console_handler = logging.StreamHandler()
console_handler.setLevel(logging.INFO)  # В консоль только INFO и выше

# Формат логов
formatter = logging.Formatter("%(asctime)s - %(levelname)s - %(message)s")
file_handler.setFormatter(formatter)
console_handler.setFormatter(formatter)

# Добавляем обработчики к логгеру
logger.addHandler(file_handler)
logger.addHandler(console_handler)

# Отключаем дублирование логов, если FastAPI использует root-логгер
logger.propagate = False
