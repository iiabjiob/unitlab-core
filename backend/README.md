# UnitLab Backend

FastAPI + AsyncPG + Redis + MQTT backend

## MQTT/Redis Pipeline

The transport layer is now split into standalone workers, so FastAPI stays a thin REST/WS gateway.

### Startup order

Before running any of the commands below in a devcontainer, activate the backend Python environment (e.g. `cd backend && source .venv/bin/activate`) or prefix commands with `uv run` so they reuse the managed virtualenv.

1. FastAPI (REST + WebSockets):
	```bash
	uv run uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload --workers 4
	```
2. MQTT ingress → Redis Stream `mqtt:inbound`:
	```bash
	uv run python -m app.workers.mqtt_ingress
	```
3. Inbound processor (business logic + WS events → Redis Pub/Sub `ws:events`):
	```bash
	uv run python -m app.workers.inbound_processor
	```
4. MQTT outbound publisher ← Redis Stream `mqtt:outbound`:
	```bash
	uv run python -m app.workers.mqtt_outbound
	```
5. Device offline checker (watches `devices:all`, emits WS heartbeats):
	```bash
	uv run python -m app.workers.device_offline
	```

FastAPI subscribes to `ws:events` and forwards every payload to connected WebSocket clients.

> In `docker-compose.prod.yml` these workers are defined as separate services (`mqtt_ingress`, `inbound_processor`, `mqtt_outbound`, `device_offline`). To launch the full production stack run `docker compose -f docker-compose.prod.yml up -d`.

## Local docker-compose

For development, `docker-compose.dev.yml` brings up only the shared infrastructure (Postgres, Redis, Mosquitto, devcontainer). FastAPI and all workers should be started manually from the devcontainer using the commands above, which lets you see code changes immediately without rebuilding images.
