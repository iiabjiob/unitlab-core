import logging
import os

# Создаём папку для логов, если её нет
LOG_DIR = "logs"
if not os.path.exists(LOG_DIR):
    os.makedirs(LOG_DIR)

# Настройка логирования
logger = logging.getLogger("app")
logger.setLevel(logging.DEBUG)  # Уровень логирования

# Файл логов
file_handler = logging.FileHandler(os.path.join(LOG_DIR, "app.log"), encoding="utf-8")
file_handler.setLevel(logging.DEBUG)

# Вывод в консоль
console_handler = logging.StreamHandler()
console_handler.setLevel(logging.INFO)

# Формат логов
formatter = logging.Formatter("%(asctime)s - %(levelname)s - %(message)s")
file_handler.setFormatter(formatter)
console_handler.setFormatter(formatter)

# Добавляем обработчики к логгеру
logger.addHandler(file_handler)
logger.addHandler(console_handler)
