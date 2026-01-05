# UnitLab Backend

FastAPI + AsyncPG + Redis + MQTT backend

## MQTT/Redis пайплайн

Новая архитектура разделяет транспорт на отдельные воркеры, поэтому сам FastAPI остаётся «тонким» API/WS шлюзом.

### Запуск сервисов

1. FastAPI (WS + REST):
	```bash
	uvicorn app.main:app --reload --workers 4
	```
2. MQTT ingress → Redis Stream `mqtt:inbound`:
	```bash
	python -m app.workers.mqtt_ingress
	```
3. Inbound processor (бизнес-логика + WS события → Redis Pub/Sub `ws:events`):
	```bash
	python -m app.workers.inbound_processor
	```
4. MQTT outbound publisher ← Redis Stream `mqtt:outbound`:
	```bash
	python -m app.workers.mqtt_outbound
	```
5. Device offline checker (следит за `devices:all`, публикует WS heartbeats):
	```bash
	python -m app.workers.device_offline
	```

FastAPI подписывается на `ws:events` и ретранслирует события подключённым WebSocket клиентам.

> В `docker-compose.prod.yml` эти воркеры уже описаны отдельными сервисами (`mqtt_ingress`, `inbound_processor`, `mqtt_outbound`, `device_offline`). Для запуска всего стека в продукционной конфигурации достаточно выполнить `docker compose -f docker-compose.prod.yml up -d`.
