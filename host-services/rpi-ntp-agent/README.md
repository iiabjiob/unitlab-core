# UnitLab RPi NTP Agent (Chrony)

Host-side Chrony configuration and status agent for UnitLab core on Raspberry Pi.

Purpose:
- manage Chrony sources (`/etc/chrony/sources.d/unitlab-ntp.sources`)
- expose status + tracking + sources to backend/frontend via Redis
- allow UI to add/remove arbitrary number of NTP servers
- keep the RPi serving NTP to peripheral devices on `10.42.0.0/24` even if upstream sync is temporarily unavailable

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
- Installer also places a host chrony drop-in:
  - `/etc/chrony/conf.d/unitlab-local-master.conf`
- It uses `chronyc reload sources` (no full `chronyd` restart by default).
- If upstream servers are unavailable, UI/backend can still show `NOT SYNCED`, but the host keeps serving its local clock to AP-side peripherals so all modules share the RPi time base.

