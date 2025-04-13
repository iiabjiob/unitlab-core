#!/bin/bash
set -e

echo "📡 Installing Mosquitto MQTT broker..."

# Установка брокера и клиента
sudo apt install -y mosquitto mosquitto-clients

# Добавим автозапуск и стартуем
sudo systemctl enable mosquitto
sudo systemctl start mosquitto

echo "✅ Mosquitto installed and running on port 1883"
