# Backend Development

## Running infrastructure

The dev compose file now only provisions stateful services. Start them via:

```bash
docker compose -f ../docker-compose.dev.yml up -d db redis mosquitto
```

This keeps Postgres, Redis and Mosquitto inside containers while FastAPI runs locally.

## Running the backend locally

```bash
cd backend
uv sync
uv run uvicorn app.main:app --reload
```

Frontend continues to run with `pnpm dev` under `frontend/`.
