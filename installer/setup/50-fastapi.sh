#!/bin/bash
set -e

echo "🐍 Setting up FastAPI..."

cd ~/unitlab/backend

if [[ ! -f .env ]]; then
  touch .env
fi

echo "📦 Creating virtualenv..."
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip

if [[ -f requirements.txt ]]; then
  pip install -r requirements.txt
else
  echo "⚠️ No requirements.txt found in ~/unitlab/backend"
fi

echo "🛠️ Creating systemd service..."
sudo tee /etc/systemd/system/fastapi.service > /dev/null <<EOF
[Unit]
Description=FastAPI Service for UnitLab
After=network.target

[Service]
User=pi
WorkingDirectory=/home/pi/unitlab/backend
ExecStart=/home/pi/unitlab/backend/.venv/bin/uvicorn app.main:app --host 0.0.0.0 --port 8000 --workers 4

Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target

EOF

sudo systemctl daemon-reload
sudo systemctl enable fastapi
sudo systemctl start fastapi

echo "✅ Service created"