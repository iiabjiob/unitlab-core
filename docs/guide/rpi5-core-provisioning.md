# RPi5 Core Provisioning (Full UnitLab Core on Bare Raspberry Pi 5)

This guide describes the exact order to deploy a full **UnitLab Core** on a fresh **Raspberry Pi 5 (Bookworm)** using an **image-based frontend** (no `frontend/src` on the device):

- Docker stack (`backend`, `frontend web image`, `db`, `redis`, `mosquitto`, workers)
- Host Wi‑Fi service (`AP/STA`, NetworkManager)
- Host NTP service (`chrony`)
- Host diagnostics service (RPi core health/thermals/services)
- Host provisioning service (repair actions + smoke checks)
- Web UI access via AP (`http://10.42.0.1`)

## Result (What You Get)

After provisioning and reboot:

- UnitLab stack is running in Docker
- Host AP starts automatically:
  - `SSID`: `[unitlab]-core-ABCD`
  - `Password`: `pwd!ABCD`
- Web UI is available at:
  - `http://10.42.0.1`
- `/settings` contains (engineer mode):
  - `Core Network` (AP/STA uplink management)
  - `Core Diagnostics` (RPi health and service status)
  - `Time / NTP Sync` (chrony server management)
- `/settings/Provisioning` is available in service mode (hidden by default):
  - smoke-checks and host-agent repair actions

## Fast Path (6 Steps)

If you already know the platform, use this exact sequence:

1. Preinstall clean RPi host
  - Run host bootstrap/provisioning once:
  ```bash
  sudo ./scripts/provision-rpi.sh --timezone Europe/Berlin
  sudo reboot
  ```

2. Build and copy runtime bundle + images
  - On dev machine:
  ```bash
  ./scripts/release.sh
  ```
  - Copy bundle and image archive to RPi (`/tmp` first, then move with `sudo` to `/opt/unitlab/releases` and `/opt/unitlab`).

  ```bash
  scp dist-release/release-images-<timestamp>.tar pi@<rpi-ip>:/tmp/
  scp -r dist-release/unitlab-core-rpi-runtime-<timestamp> pi@<rpi-ip>:/tmp/
  ```
  

3. Configure env
  - Create/update shared env files:
  ```bash
  sudo mkdir -p /opt/unitlab/shared
  sudo cp -n /opt/unitlab/releases/unitlab-core-rpi-runtime-<timestamp>/shared/backend.env.example /opt/unitlab/shared/backend.env
  sudo cp -n /opt/unitlab/releases/unitlab-core-rpi-runtime-<timestamp>/shared/db.env.example /opt/unitlab/shared/db.env
  sudo nano /opt/unitlab/shared/backend.env
  sudo nano /opt/unitlab/shared/db.env
  ```

4. Deploy runtime
  - Offline mode:
  ```bash
  # Option A: bundle/image already moved into /opt
  sudo /opt/unitlab/releases/unitlab-core-rpi-runtime-<timestamp>/scripts/deploy-rpi.sh \
    --bundle-dir /opt/unitlab/releases/unitlab-core-rpi-runtime-<timestamp> \
    --images-archive /opt/unitlab/release-images-<timestamp>.tar

  # Option B: deploy directly from /tmp (no pre-move required)
  sudo /tmp/unitlab-core-rpi-runtime-<timestamp>/scripts/deploy-rpi.sh \
    --bundle-dir /tmp/unitlab-core-rpi-runtime-<timestamp> \
    --images-archive /tmp/release-images-<timestamp>.tar

  ```

5. Install host agents
  ```bash
  cd /opt/unitlab/current
  sudo ./scripts/install-host-agents.sh --skip-apt --skip-pip-upgrade
  ```
  - For strict offline updates, always keep `--skip-apt --skip-pip-upgrade`.
  - Note: during `rpi-net-agent` restart, the host can switch AP/STA mode and current SSH session may disconnect.
    Reconnect and continue with step 6.

6. Verify
  ```bash
  /opt/unitlab/current/scripts/verify-host-agents.sh
  /opt/unitlab/current/scripts/verify-rpi-runtime.sh --project-dir /opt/unitlab/current --compose-file /opt/unitlab/current/docker-compose.prod.yml
  ```

## 1. Prepare Raspberry Pi OS (Bookworm)

Recommended first setup path: **Ethernet connected**.

### 1.1 Update OS

```bash
sudo apt-get update
sudo apt-get upgrade -y
sudo reboot
```

### 1.2 Install basic tools

```bash
sudo apt-get update
sudo apt-get install -y git curl ca-certificates rsync jq
```

### 1.3 Ensure NetworkManager is installed and active

```bash
systemctl status NetworkManager
```

If not installed:

```bash
sudo apt-get install -y network-manager
sudo systemctl enable --now NetworkManager
```

Verify `wlan0` is managed:

```bash
nmcli device status
```

### 1.4 Run host pre-provision script (recommended)

Instead of running host setup steps manually, run the provisioning script from this repository:

```bash
cd /opt/unitlab/unitlab-core
sudo ./scripts/provision-rpi.sh --timezone Europe/London
```

Useful variants:

```bash
# Skip full OS upgrade (faster first pass)
sudo ./scripts/provision-rpi.sh --skip-upgrade

# Use environment variable override
sudo TIMEZONE=Etc/UTC ./scripts/provision-rpi.sh
```

What it configures:

- apt update/full-upgrade
- timezone validation + apply
- chrony install/enable
- docker + compose plugin install/enable
- `/opt/unitlab/releases` and `/opt/unitlab/shared` directories
- docker daemon log rotation (`max-size=10m`, `max-file=3`)
- kernel cgroup args for Docker memory/swap limits (`cgroup_enable=memory cgroup_memory=1`)

After completion, reboot is recommended.

## 1.5 Current Release/Deploy Flow (Recommended)

Use a strict split between **dev machine** and **RPi runtime host**:

- Dev machine: `scripts/release.sh` (build/export/bundle only)
- RPi host: `scripts/deploy-rpi.sh` (switch/start/verify/rollback/cleanup)

### One-time on RPi (device bootstrap)

```bash
scp scripts/provision-rpi.sh pi@<rpi-ip>:/tmp/
ssh pi@<rpi-ip>
sudo bash /tmp/provision-rpi.sh --timezone Europe/Berlin
sudo reboot
```

Expected host layout after bootstrap:

```text
/opt/unitlab/
  releases/
  shared/
  current -> <set during deploy>
```

### Dev machine: build release artifacts

```bash
cd /path/to/unitlab-core
./scripts/release.sh
```

Result:

```text
dist-release/
  unitlab-core-rpi-runtime-<timestamp>/
  release-images-<timestamp>.tar
```

### Copy artifacts to RPi

Offline mode:

```bash
scp -r dist-release/unitlab-core-rpi-runtime-<timestamp> pi@<rpi-ip>:/tmp/
scp dist-release/release-images-<timestamp>.tar pi@<rpi-ip>:/tmp/

ssh pi@<rpi-ip>
sudo mkdir -p /opt/unitlab/releases /opt/unitlab
sudo rm -rf /opt/unitlab/releases/unitlab-core-rpi-runtime-<timestamp>
sudo mv /tmp/unitlab-core-rpi-runtime-<timestamp> /opt/unitlab/releases/
sudo mv /tmp/release-images-<timestamp>.tar /opt/unitlab/
```

Registry mode:

```bash
scp -r dist-release/unitlab-core-rpi-runtime-<timestamp> pi@<rpi-ip>:/tmp/

ssh pi@<rpi-ip>
sudo mkdir -p /opt/unitlab/releases
sudo rm -rf /opt/unitlab/releases/unitlab-core-rpi-runtime-<timestamp>
sudo mv /tmp/unitlab-core-rpi-runtime-<timestamp> /opt/unitlab/releases/
```

### RPi: deploy release

Before first deploy, create shared env files expected by `docker-compose.prod.yml`:

```bash
sudo mkdir -p /opt/unitlab/shared
sudo cp -n /opt/unitlab/releases/unitlab-core-rpi-runtime-<timestamp>/shared/backend.env.example /opt/unitlab/shared/backend.env
sudo cp -n /opt/unitlab/releases/unitlab-core-rpi-runtime-<timestamp>/shared/db.env.example /opt/unitlab/shared/db.env

# edit values (especially secrets) before deploy
sudo nano /opt/unitlab/shared/db.env
sudo nano /opt/unitlab/shared/backend.env
```

Offline mode:

```bash
sudo /opt/unitlab/releases/unitlab-core-rpi-runtime-<timestamp>/scripts/deploy-rpi.sh \
  --bundle-dir /opt/unitlab/releases/unitlab-core-rpi-runtime-<timestamp> \
  --images-archive /opt/unitlab/release-images-<timestamp>.tar
```

Registry mode:

```bash
sudo /opt/unitlab/releases/unitlab-core-rpi-runtime-<timestamp>/scripts/deploy-rpi.sh \
  --bundle-dir /opt/unitlab/releases/unitlab-core-rpi-runtime-<timestamp>
```

Retry note: if deploy fails due transient network/pull errors, rerunning the same command for the same `<timestamp>` is safe.

`deploy-rpi.sh` behavior:

1. Validate compose in target release
2. Load images (offline mode only)
3. Switch `current` symlink atomically
4. Run `docker compose up -d --remove-orphans`
5. Run runtime verify
6. If verify fails → rollback to previous release
7. If verify succeeds → cleanup old releases (keep current + previous)

## 2. Install Docker + Compose Plugin

If Docker is not installed yet:

```bash
curl -fsSL https://get.docker.com | sh
sudo usermod -aG docker $USER
newgrp docker
sudo apt-get install -y docker-compose-plugin
```

Verify:

```bash
docker --version
docker compose version
```

## 3. Copy Runtime Bundle to RPi5 (Recommended)

Recommended: deploy a **runtime bundle** (frontend source omitted) instead of cloning the full monorepo to the field device.

### 3.1 Build runtime bundle on dev/CI machine

```bash
cd /Users/anton/Projects/unitlab-core
./scripts/create-rpi-runtime-bundle.sh
```

This creates a bundle under:

```text
/Users/anton/Projects/unitlab-core/dist-release/unitlab-core-rpi-runtime-<timestamp>
```

### 3.2 Copy bundle to RPi5

Target path on `RPi5` must be:

```text
/opt/unitlab/unitlab-core
```

Example:

```bash
rsync -av /Users/anton/Projects/unitlab-core/dist-release/unitlab-core-rpi-runtime-<timestamp>/ pi@<rpi-ip>:/opt/unitlab/unitlab-core/
```

### 3.3 Alternative (full repo clone)

If you want full source on the device for debugging, cloning the repo is still valid:

```bash
cd /opt
sudo mkdir -p /opt/unitlab
sudo chown -R $USER:$USER /opt/unitlab
cd /opt/unitlab
git clone <YOUR_REPO_URL> unitlab-core
cd unitlab-core
```

## 4. Prepare Release Images on Dev/CI (Backend + Frontend)

Production deployment on `RPi5` should use prebuilt images for:
- backend (`API + workers + migrations`)
- frontend web runtime (`nginx + dist`)

This avoids local builds on the device and makes rollout/rollback predictable.

### What to do on dev/CI machine (before deploying to RPi)

Build the backend image:

```bash
cd /Users/anton/Projects/unitlab-core
./scripts/build-backend-image.sh unitlab-backend:2026.02.23
```

Build the frontend web image:

```bash
cd /Users/anton/Projects/unitlab-core
./scripts/build-web-image.sh unitlab-web:2026.02.23
```

Then either:

- push to your registry (recommended), or
- export and transfer as tar (offline deployment)

Detailed build/release instructions:

- `/Users/anton/Projects/unitlab-core/docs/guide/frontend-web-image-release.md`

### What to do on RPi5

Create or update root `.env` (same directory as `docker-compose.prod.yml`) and set backend/frontend image tags:

```env
UNITLAB_BACKEND_IMAGE=unitlab-backend:2026.02.23
UNITLAB_WEB_IMAGE=unitlab-web:2026.02.23
```

If you use a registry:

```env
UNITLAB_BACKEND_IMAGE=ghcr.io/your-org/unitlab-backend:2026.02.23
UNITLAB_WEB_IMAGE=ghcr.io/your-org/unitlab-web:2026.02.23
```

If you deploy offline, load the image first:

```bash
docker load -i /path/to/unitlab-release-images.tar
```

## 5. Create/Update Env Files

`docker-compose.prod.yml` / services expect:

- `/opt/unitlab/shared/backend.env`
- `/opt/unitlab/shared/db.env`
- root `.env` (for `UNITLAB_BACKEND_IMAGE`, `UNITLAB_WEB_IMAGE`)

### 5.1 `/opt/unitlab/shared/db.env`

Example:

```env
POSTGRES_USER=unitlab_pg_user
POSTGRES_PASSWORD=unitlab_pg_password
POSTGRES_DB=unitlab_pg
POSTGRES_HOST_AUTH_METHOD=md5
```

### 5.2 `/opt/unitlab/shared/backend.env`

Minimal required values (example):

```env
APP_ENV=production
DEBUG=false
DEBUG_LEVEL=INFO

POSTGRES_USER=unitlab_pg_user
POSTGRES_PASSWORD=unitlab_pg_password
POSTGRES_HOST=db
POSTGRES_PORT=5432
POSTGRES_DB=unitlab_pg

REDIS_HOST=redis
REDIS_PORT=6379

MQTT_HOST=mosquitto
MQTT_PORT=1883
```

Add any other required backend variables used in your build/environment.

Create/update shared env files (persistent across releases):

```bash
sudo mkdir -p /opt/unitlab/shared
sudo nano /opt/unitlab/shared/db.env
sudo nano /opt/unitlab/shared/backend.env
```

### 5.3 Root `.env` (backend + frontend image tags)

Create `/opt/unitlab/unitlab-core/.env` (or update it) with:

```env
UNITLAB_BACKEND_IMAGE=unitlab-backend:2026.02.23
UNITLAB_WEB_IMAGE=unitlab-web:2026.02.23
```

Registry example:

```env
UNITLAB_BACKEND_IMAGE=ghcr.io/your-org/unitlab-backend:2026.02.23
UNITLAB_WEB_IMAGE=ghcr.io/your-org/unitlab-web:2026.02.23
```

## 6. Start Docker Stack (API/UI/Workers/DB/Redis/MQTT)

From project root:

```bash
cd /opt/unitlab/unitlab-core
docker compose -f docker-compose.prod.yml pull
docker compose -f docker-compose.prod.yml up -d
```

Check status:

```bash
docker compose -f docker-compose.prod.yml ps
```

Key containers:

- `unitlab-db`
- `unitlab-redis`
- `unitlab-backend`
- `unitlab-nginx`
- `unitlab-mosquitto`
- `unitlab-signal-allocation-runner`
- `unitlab-signal-test-run-runner`
- `unitlab-sequence-runner`

Check backend health:

```bash
curl -s http://127.0.0.1/api/v1/health | jq .
```

## 7. Install Host Wi‑Fi AP/STA Service (NetworkManager Agent)

This service manages:

- AP mode on boot (`[unitlab]-core-ABCD`)
- STA scan/connect/fallback
- Redis contract used by backend/frontend `/settings`

### 7.1 Install and start

```bash
cd /opt/unitlab/unitlab-core
sudo ./scripts/install-host-agents.sh --skip-apt --skip-pip-upgrade
./scripts/verify-host-agents.sh
```

### 7.2 Verify service

```bash
systemctl status unitlab-rpi-net-agent --no-pager
journalctl -u unitlab-rpi-net-agent -n 100 --no-pager
```

### 7.3 Expected behavior

On startup it should create/start AP:

- `SSID`: `[unitlab]-core-ABCD`
- `Password`: `pwd!ABCD`
- AP IP: `10.42.0.1/24`

## 8. Install Host NTP Service (Chrony Agent)

This service manages `chrony` natively and exposes state/actions to UI via Redis/backend.

### 8.1 Install and start

```bash
cd /opt/unitlab/unitlab-core
sudo ./scripts/install-host-agents.sh --skip-apt --skip-pip-upgrade
./scripts/verify-host-agents.sh
```

Unified installer behavior:

- installs `chrony` (if missing)
- installs/updates all host agents from bundled wheels
- enables and starts corresponding systemd services

Offline update behavior:

- `--skip-apt --skip-pip-upgrade` guarantees no apt/pip index network calls during agent reinstall/update.

### 8.2 Verify service

```bash
systemctl status unitlab-rpi-ntp-agent --no-pager
journalctl -u unitlab-rpi-ntp-agent -n 100 --no-pager
```

### 8.3 Verify backend sees NTP state

```bash
curl -s http://127.0.0.1/api/v1/core-ntp/state | jq .
```

## 8A. Install Host Diagnostics Service (RPi Core Health)

This service publishes central-module diagnostics (temperature/load/memory/disk/systemd services) to Redis for UI display in `/settings/diagnostics`.

### 8A.1 Install and start

```bash
cd /opt/unitlab/unitlab-core
sudo ./scripts/install-host-agents.sh --skip-apt --skip-pip-upgrade
./scripts/verify-host-agents.sh
```

### 8A.2 Verify service

```bash
systemctl status unitlab-rpi-core-diag-agent --no-pager
journalctl -u unitlab-rpi-core-diag-agent -n 100 --no-pager
```

### 8A.3 Verify backend sees diagnostics state

```bash
curl -s http://127.0.0.1/api/v1/core-diagnostics/state | jq .
```

## 9. Verify UI End-to-End

Connect a laptop/tablet to the AP:

- `SSID`: `[unitlab]-core-ABCD`
- `Password`: `pwd!ABCD`

Open:

- `http://10.42.0.1`

### 9.1 Settings → Core Network

Verify:

- AP status / SSID / password / IP shown
- `Scan Wi‑Fi` works
- `Connect STA` works
- on failed connect it returns to AP

### 9.2 Settings → Time / NTP Sync

Verify:

- chrony service status (`ACTIVE/INACTIVE`)
- sync status (`SYNCED / NOT SYNCED`)
- effective sources visible
- add/remove NTP servers
- `Apply servers`
- `Restore defaults`
- `Reload Chrony`

### 9.3 Settings → Core Diagnostics

Verify:

- mode (`OK / DEGRADED / ERROR`)
- hostname / model / OS / kernel shown
- CPU temperature and load values update
- memory and disk usage shown
- host service statuses visible (`docker`, `NetworkManager`, `chrony`, host agents)

### 9.4 Settings → Provisioning

After installing the provisioning agent (see **10A**) and enabling service mode, verify:

- provisioning checks list is populated (project files, docker, compose, host agents, frontend delivery mode)
- `Run smoke check` populates local API checks
- install/repair buttons are visible for host agents
- last action result block updates after a provisioning action

Note:
- In normal engineer mode, `Provisioning` is hidden from the Settings sidebar.
- Enable frontend service mode (`VITE_SETTINGS_SERVICE_MODE=true`) to access service-only actions.

## 10. Operational Checks (Required)

### 10.1 Redis is available to host services only on localhost

```bash
ss -ltnp | rg 6379
```

Expected:

- `127.0.0.1:6379`

### 10.2 Reboot behavior (critical)

```bash
sudo reboot
```

After reboot verify:

- AP starts again (`[unitlab]-core-ABCD`)
- UI works at `http://10.42.0.1`
- Docker stack is healthy
- `/settings` shows Core Network + Diagnostics + NTP state

### 10.3 STA failure fallback

From `/settings`, try connecting to an invalid/unknown SSID:

- the agent should report connection failure
- AP should be restored automatically
- local UI access should remain available via AP

## 10A. Install Host Provisioning Service (Repair / Smoke Checks)

This service runs host-side provisioning checks and can execute host-agent install scripts from the UI (`/settings/provisioning`).

### 10A.1 Install and start

```bash
cd /opt/unitlab/unitlab-core
sudo ./scripts/install-host-agents.sh --skip-apt --skip-pip-upgrade
./scripts/verify-host-agents.sh
```

### 10A.2 Verify service

```bash
systemctl status unitlab-rpi-provision-agent --no-pager
journalctl -u unitlab-rpi-provision-agent -n 100 --no-pager
```

### 10A.3 Verify backend sees provisioning state

```bash
curl -s http://127.0.0.1/api/v1/core-provision/state | jq .
```

## 11. Golden Flash / One-Command Strategy (Recommended)

For future central modules, build a golden image that already includes:

- Raspberry Pi OS Bookworm (64-bit)
- Docker + compose plugin
- `NetworkManager`
- runtime bundle files in `/opt/unitlab/unitlab-core` (frontend source omitted)
- root `.env` with:
  - `UNITLAB_BACKEND_IMAGE=<release-tag>`
  - `UNITLAB_WEB_IMAGE=<release-tag>`
- production env files (or first-boot templates)

Then per-device provisioning becomes approximately:

```bash
cd /opt/unitlab/unitlab-core
docker compose -f docker-compose.prod.yml up -d
sudo ./scripts/install-host-agents.sh --skip-apt --skip-pip-upgrade
./scripts/verify-host-agents.sh
```

## 12. Short Version (Checklist)

1. Update OS and install Docker + Compose + NetworkManager
2. Build release artifacts and copy runtime bundle + image archive to RPi
3. Configure `/opt/unitlab/shared/backend.env` and `/opt/unitlab/shared/db.env`
4. Run deploy script for selected release (`deploy-rpi.sh`)
5. Install host agents: `sudo ./scripts/install-host-agents.sh --skip-apt --skip-pip-upgrade`
6. Verify runtime and agents, then open UI at `http://10.42.0.1`
