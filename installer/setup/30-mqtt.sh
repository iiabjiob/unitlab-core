# -------------------------------------------------------
# 🧠 Mosquitto MQTT Broker Setup
# -------------------------------------------------------

echo "📡 Installing Mosquitto MQTT broker..."

# Установка брокера и клиента
sudo apt install -y mosquitto mosquitto-clients

# Настройка порта и разрешения анонимного подключения
MOSQUITTO_CONF="/etc/mosquitto/conf.d/unitlab.conf"

if [ ! -f "$MOSQUITTO_CONF" ]; then
  echo "🛠️ Writing Mosquitto config..."
  sudo tee "$MOSQUITTO_CONF" >/dev/null <<EOF
listener 1883
allow_anonymous true
EOF
else
  echo "ℹ️ Mosquitto config already exists, skipping."
fi

# Добавим автозапуск и стартуем
sudo systemctl enable mosquitto
sudo systemctl restart mosquitto

echo "✅ Mosquitto installed and running on port 1883"
