from fastapi import FastAPI
from app.database import Base, engine

app = FastAPI()

# Создаём таблицы, если их нет
Base.metadata.create_all(bind=engine)

@app.get("/")
def read_root():
    return {"Hello": "World"}