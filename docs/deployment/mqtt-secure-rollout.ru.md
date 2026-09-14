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

## Activation

```bash
docker compose -f docker-compose.prod.yml -f docker-compose.mqtt-secure.yml up -d mosquitto mqtt_ingress mqtt_outbound
```

The secure profile disables anonymous access, persists broker data, and keeps
the password file outside the repository. Activation is not complete until an
anonymous publish and a cross-device topic publish are rejected while an
authorized command/telemetry round-trip succeeds.
