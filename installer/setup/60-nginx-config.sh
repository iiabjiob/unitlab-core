#!/bin/bash
set -e

echo "🌐 Configuring Nginx for FastAPI..."

# Путь к конфигу в инсталлере
NGINX_CONF_SOURCE="$(dirname "$0")/../config/nginx.conf"
NGINX_CONF_TARGET="/etc/unitlab/nginx.conf"

# Копируем nginx.conf
sudo mkdir -p /etc/unitlab
sudo cp "$NGINX_CONF_SOURCE" "$NGINX_CONF_TARGET"

# Подключаем его
sudo rm -f /etc/nginx/sites-enabled/default
sudo ln -sf "$NGINX_CONF_TARGET" /etc/nginx/sites-enabled/unitlab.conf

# Проверка и перезапуск
sudo chmod o+x /home/pi /home/pi/unitlab /home/pi/unitlab/frontend || true
sudo nginx -t
sudo systemctl restart nginx

echo "✅ Nginx configured and restarted."
