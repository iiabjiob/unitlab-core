FROM python:3.10-slim

WORKDIR /app

# Скопируем файл зависимостей
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Скопируем код приложения
COPY ./app ./app

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]