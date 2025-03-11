import subprocess

def get_wifi_mode():
    """Определяет, работает ли Raspberry Pi в режиме Wi-Fi клиента или точки доступа."""
    try:
        result = subprocess.run(["nmcli", "-t", "-f", "DEVICE,TYPE,STATE", "dev"], capture_output=True, text=True)
        if "wifi" in result.stdout and "connected" in result.stdout:
            return "client"
        return "ap"
    except Exception as e:
        raise RuntimeError(f"Ошибка при определении режима Wi-Fi: {str(e)}")

def set_wifi_client():
    """Переключает Raspberry Pi в режим Wi-Fi клиента."""
    try:
        subprocess.run(["systemctl", "stop", "hostapd"], check=True)
        subprocess.run(["systemctl", "stop", "dnsmasq"], check=True)
        subprocess.run(["nmcli", "radio", "wifi", "on"], check=True)
        return "switched to client mode"
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Ошибка переключения в клиентский режим: {str(e)}")

def set_wifi_ap():
    """Переключает Raspberry Pi в режим точки доступа (AP Mode)."""
    try:
        subprocess.run(["nmcli", "radio", "wifi", "off"], check=True)
        subprocess.run(["systemctl", "start", "hostapd"], check=True)
        subprocess.run(["systemctl", "start", "dnsmasq"], check=True)
        return "switched to access point mode"
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Ошибка переключения в AP-режим: {str(e)}")
