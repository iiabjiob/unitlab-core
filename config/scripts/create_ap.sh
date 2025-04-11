#!/bin/bash

# Интерфейс Wi-Fi (можно заменить, если у тебя другой)
INTERFACE="wlan0"

# Получаем MAC-адрес и последние два байта
mac_address=$(cat /sys/class/net/$INTERFACE/address)
last_two_bytes=$(echo $mac_address | awk -F: '{print $5$6}')
last_two_bytes=${last_two_bytes^^}

# Формируем SSID и пароль
ssid="[unitLab]_core-unit-$last_two_bytes"
password="pwd!$last_two_bytes"

# Название соединения
CON_NAME="core-unit-hotspot"

# Удаляем старое соединение, если было
nmcli connection delete "$CON_NAME" 2>/dev/null

# Создаём точку доступа
sudo nmcli dev wifi hotspot ifname "$INTERFACE" con-name "$CON_NAME" ssid "$ssid" password "$password"

# Выводим результат
echo "✅ AP started: SSID=$ssid