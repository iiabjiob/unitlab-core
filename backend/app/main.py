from fastapi import FastAPI
from app.db import Base, engine
from app.core import APP_NAME, DEBUG

app = FastAPI(title=APP_NAME, debug=DEBUG)  # Используем переменные из config.py

if DEBUG:
    print("DEBUG mode: Creating tables...")
    Base.metadata.create_all(bind=engine)

@app.get("/")
def read_root():
    return {"message": f"Welcome to {APP_NAME}"}