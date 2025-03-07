from fastapi import FastAPI
from app.database import Base, engine
from app.config import APP_NAME, DEBUG  # Импортируем настройки

app = FastAPI(title=APP_NAME, debug=DEBUG)  # Используем переменные из config.py

# Создаём таблицы, если их нет
Base.metadata.create_all(bind=engine)

@app.get("/")
def read_root():
    return {"message": f"Welcome to {APP_NAME}"}