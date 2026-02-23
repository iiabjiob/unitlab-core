# UnitLab RPi NTP Agent (Chrony)

Host-side Chrony configuration and status agent for UnitLab core on Raspberry Pi.

Purpose:
- manage Chrony sources (`/etc/chrony/sources.d/unitlab-ntp.sources`)
- expose status + tracking + sources to backend/frontend via Redis
- allow UI to add/remove arbitrary number of NTP servers

This service runs on the host OS (Bookworm / Raspberry Pi 5), not in Docker.

## Redis Contract

- command stream: `core_ntp:commands`
- event stream: `core_ntp:events`
- state key: `core_ntp:state`

Commands (`json` field in stream):
- `status`
- `apply_servers` with `servers: string[]`
- `restore_defaults`
- `reload`

## Host requirements

- `chrony` installed and running
- `chronyc`
- Python 3.11+
- Redis reachable from host (`127.0.0.1:6379` by default)

## Install

```bash
sudo /opt/unitlab/install/install_rpi_ntp_agent.sh
```

## Notes

- Agent writes only its managed file:
  - `/etc/chrony/sources.d/unitlab-ntp.sources`
- It uses `chronyc reload sources` (no full `chronyd` restart by default).

