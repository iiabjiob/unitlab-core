from fastapi import FastAPI
from contextlib import asynccontextmanager
from app.core.config import get_settings

@asynccontextmanager
async def lifespan(app: FastAPI):
    yield

app = FastAPI(
    title=get_settings.app_name,
    description=get_settings.description,
    version=get_settings.version,
    debug=get_settings.debug,
    lifespan=lifespan
    )

@app.get("/")
def read_root():
    return {"message": f"Welcome to {get_settings.app_name}"}