import os
from dotenv import load_dotenv

# Загружаем .env
load_dotenv()

# Читаем переменные окружения
DATABASE_URL = os.getenv("DATABASE_URL")
APP_NAME = os.getenv("APP_NAME", "Default Name")
DEBUG = os.getenv("DEBUG", "False").lower() == "true"
