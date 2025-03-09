from fastapi import FastAPI
from contextlib import asynccontextmanager
from app.core.config import settings

@asynccontextmanager
async def lifespan(app: FastAPI):
    yield

app = FastAPI(
    title=settings.APP_NAME,         # Название API
    description=settings.DESCRIPTION, # Описание API
    version=settings.VERSION,         # Версия API
    contact={
        "name": "Anton Pavlov",
        "email": "pavlov@myyahoo.com",
    },  # Контактная информация
    license_info={
        "name": "MIT License",
        "url": "https://opensource.org/licenses/MIT",
    },  # Лицензия
    debug=settings.DEBUG,             # Включает режим отладки
    lifespan=lifespan                 # Управление жизненным циклом
)

@app.get("/")
def read_root():
    return {"message": f"Welcome to {settings.APP_NAME}"}