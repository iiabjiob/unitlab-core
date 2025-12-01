FROM python:3.11-slim AS base

# Install curl
RUN apt-get update \
    && apt-get install -y curl \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install uv
RUN pip install --no-cache-dir uv

WORKDIR /app

# Copy project metadata first (for caching)
COPY pyproject.toml uv.lock ./

# Install deps
RUN uv sync --frozen --no-dev

# Ensure venv binaries are used
ENV PATH="/app/.venv/bin:$PATH"

# Copy project code
COPY . .

CMD ["uv", "run", "gunicorn", \
     "-k", "uvicorn.workers.UvicornWorker", \
     "app.main:app", \
     "--workers", "1", \
     "--bind", "0.0.0.0:8000"]
