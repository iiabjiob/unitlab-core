FROM python:3.11-slim

# Обновить pip, wheel и setuptools
RUN pip install --upgrade pip setuptools wheel

WORKDIR /app

# Копируем только зависимости на этом шаге — это ускоряет пересборку!
COPY requirements.txt .

# Ставим зависимости
RUN pip install --no-cache-dir -r requirements.txt

# Копируем остальной код
COPY . .

# Запуск Uvicorn (можно изменить под свой main)
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]
