# Frontend Web Image Release (Production)

This guide describes the recommended production workflow for delivering UnitLab container images to `RPi5`, with special focus on the frontend web image (`nginx + dist`) and **without shipping frontend source files** (`frontend/src`, `package.json`, `node_modules`).

## Why This Is Better Than Copying `frontend/src`

Use a prebuilt frontend runtime image (`nginx + dist`) because it gives you:

- deterministic frontend build artifact
- versioned rollback by image tag
- smaller footprint on `RPi5`
- no `node/pnpm` requirement on the target device
- no risk of rebuilding the wrong commit directly on the field unit

## What Gets Deployed to RPi5

On `RPi5`, keep:

- `docker-compose.prod.yml`
- env files (`backend/.env.prod`, `backend/.env.db.prod`, root `.env` for image tags)
- host services (`rpi-net-agent`, `rpi-ntp-agent`, `rpi-core-diag-agent`, `rpi-provision-agent`)
- optional backend source (only if you intentionally still build backend locally; not recommended)

Do **not** require:

- `frontend/src`
- `frontend/node_modules`
- local `pnpm build` on RPi

### Recommended packaging method (runtime bundle)

Create a minimal runtime bundle that omits `frontend/src` entirely:

```bash
cd /Users/anton/Projects/unitlab-core
./scripts/create-rpi-runtime-bundle.sh
```

This produces a deployable directory under `dist-release/` containing:
- `docker-compose.prod.yml`
- `config/`
- `backend/` (for backend local builds)
- `host-services/`
- `scripts/`
- provisioning guides
- `.env.example`

## Delivery Model

### Build machine (dev/CI)
1. Build backend image (`linux/arm64`)
2. Build frontend web runtime image (`linux/arm64`)
3. Push images to registry (or export image tar)

### RPi5 (production target)
1. Pull/load backend + frontend images
2. Set `UNITLAB_BACKEND_IMAGE` and `UNITLAB_WEB_IMAGE` in root `.env`
3. `docker compose pull && docker compose up -d`

## Files Added for This Flow

- `/Users/anton/Projects/unitlab-core/Dockerfile.web.prod`
  - runtime `nginx` image that copies:
    - `frontend/dist`
    - `config/nginx.conf`
- `/Users/anton/Projects/unitlab-core/scripts/build-web-image.sh`
  - builds `frontend/dist` and the runtime web image
- `/Users/anton/Projects/unitlab-core/scripts/build-backend-image.sh`
  - builds backend runtime image for API/workers/migrations
- `/Users/anton/Projects/unitlab-core/scripts/export-release-images.sh`
  - exports backend/web images as tar (offline transfer option)

## Build the Web Image (Dev/CI Machine)

### Option A: Build frontend + image in one command

```bash
cd /Users/anton/Projects/unitlab-core
./scripts/build-web-image.sh unitlab-web:2026.02.23
```

What it does:
- runs `pnpm build` in `frontend/`
- verifies `frontend/dist/index.html` exists
- builds runtime image using `/Users/anton/Projects/unitlab-core/Dockerfile.web.prod`
- uses `docker buildx` with default platform `linux/arm64`

### Option B: `dist` already built

```bash
cd /Users/anton/Projects/unitlab-core
SKIP_FRONTEND_BUILD=1 ./scripts/build-web-image.sh unitlab-web:2026.02.23
```

Use this in CI when `dist` was produced in a prior stage.

## Build the Backend Image (Dev/CI Machine)

```bash
cd /Users/anton/Projects/unitlab-core
./scripts/build-backend-image.sh unitlab-backend:2026.02.23
```

Notes:
- uses `docker buildx` with default platform `linux/arm64`
- produces the image used by:
  - backend API
  - workers
  - migrations job

## Push the Image to a Registry (Recommended)

Example (replace with your registry):

```bash
docker tag unitlab-backend:2026.02.23 ghcr.io/your-org/unitlab-backend:2026.02.23
docker push ghcr.io/your-org/unitlab-backend:2026.02.23
docker tag unitlab-web:2026.02.23 ghcr.io/your-org/unitlab-web:2026.02.23
docker push ghcr.io/your-org/unitlab-web:2026.02.23
```

Use immutable tags (`date`, commit SHA, release number), not only `latest`.

## Offline / Air-Gapped Transfer (If Registry Is Not Available)

Export images as tar:

```bash
cd /Users/anton/Projects/unitlab-core
BACKEND_IMAGE=unitlab-backend:latest WEB_IMAGE=unitlab-web:2026.02.23 ./scripts/export-release-images.sh /tmp/unitlab-release-images.tar
```

Transfer `/tmp/unitlab-release-images.tar` to `RPi5`, then:

```bash
docker load -i /path/to/unitlab-release-images.tar
```

## Configure `docker-compose.prod.yml` on RPi5

`docker-compose.prod.yml` now expects the frontend as an image:

- `backend.image = ${UNITLAB_BACKEND_IMAGE:-unitlab-backend:latest}`
- `nginx.image = ${UNITLAB_WEB_IMAGE:-unitlab-web:latest}`
- no bind mount of `./frontend/dist`

Set the image tag in root `.env` (same directory as `docker-compose.prod.yml`):

```env
UNITLAB_BACKEND_IMAGE=ghcr.io/your-org/unitlab-backend:2026.02.23
UNITLAB_WEB_IMAGE=ghcr.io/your-org/unitlab-web:2026.02.23
```

If omitted, compose falls back to `unitlab-web:latest`.

## Deploy on RPi5 (Image-Based Frontend)

### Registry-based deployment

```bash
cd /opt/unitlab/unitlab-core
docker compose -f docker-compose.prod.yml pull
docker compose -f docker-compose.prod.yml up -d
```

Notes:
- `pull` updates backend + frontend images (and official dependencies if newer)
- no local container builds on `RPi5`

### Offline deployment (after `docker load`)

```bash
cd /opt/unitlab/unitlab-core
docker compose -f docker-compose.prod.yml up -d
```

## Rollback Frontend

Change `UNITLAB_WEB_IMAGE` and/or `UNITLAB_BACKEND_IMAGE` in `/opt/unitlab/unitlab-core/.env` to previous tags, then:

```bash
cd /opt/unitlab/unitlab-core
docker compose -f docker-compose.prod.yml up -d
```

This is the main operational benefit of image-based frontend delivery.

## Operational Notes

### Provisioning checks (`/settings/provisioning`)
The provisioning agent now accepts **either**:
- `frontend/dist` (legacy mode), or
- image-based frontend delivery (`UNITLAB_WEB_IMAGE` referenced in compose)

So the checks remain useful after switching to web-image deployment.

### Future improvement (optional)
Pin images by digest instead of tags for fully immutable releases:

- `ghcr.io/your-org/unitlab-web@sha256:...`
- `ghcr.io/your-org/unitlab-backend@sha256:...`
