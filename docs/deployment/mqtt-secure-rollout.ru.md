# Secure MQTT rollout

Secure profile is opt-in until installed device firmware has credentials.

## Preconditions

1. Provision `/opt/unitlab/shared/mosquitto.password` on the production host
   using `mosquitto_passwd`; do not commit the file.
2. Set `MQTT_USERNAME` and `MQTT_PASSWORD` in the external backend environment
   consumed by the MQTT workers.
3. Provision the same identity/ACL-compatible credentials on every device and
   verify registration, telemetry, command and RESP topics in a simulator or
   maintenance window.
4. Copy the tracked `config/mosquitto.secure.acl` with the secure profile. The
   backend identity is `unitlab-backend`; each device username must equal its
   stable `unit_id`. Do not reuse one device credential across units.

Для TLS дополнительно задайте backend `MQTT_TLS=1`, `MQTT_PORT=8883` и путь
`MQTT_TLS_CA_FILE` к CA-сертификату. Клиентский сертификат и ключ
(`MQTT_TLS_CERT_FILE` и `MQTT_TLS_KEY_FILE`) являются опциональными, но
задаются только парой; при отсутствующем CA backend намеренно не подключается.
TLS listener включается отдельным overlay с внешним каталогом
`/opt/unitlab/shared/mosquitto-tls`:

```bash
docker compose -f docker-compose.prod.yml \
  -f docker-compose.mqtt-secure.yml -f docker-compose.mqtt-tls.yml \
  up -d mosquitto mqtt_ingress mqtt_outbound
```

Каталог должен содержать `ca.crt`, `server.crt` и `server.key`; секреты и ключи
не добавляются в репозиторий.

## Activation

```bash
docker compose -f docker-compose.prod.yml -f docker-compose.mqtt-secure.yml up -d mosquitto mqtt_ingress mqtt_outbound
```

The secure profile disables anonymous access, persists broker data, and keeps
the password file outside the repository. Its ACL allows the backend to consume
device telemetry and publish commands, while a device can read only its own
command/request topics and publish only its own telemetry/response topics.
Activation is not complete until an anonymous publish and a cross-device topic
publish are rejected while an authorized command/telemetry round-trip succeeds.
