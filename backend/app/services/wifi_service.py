import subprocess

def scan_wifi_linux():
    try:
        result = subprocess.run(
            ["nmcli", "-t", "-f", "SSID,SIGNAL", "dev", "wifi"],
            capture_output=True,
            text=True
        )
        
        networks = []
        for line in result.stdout.splitlines():
            parts = line.split(":")
            if len(parts) >= 2:
                ssid = parts[0].strip()
                signal = parts[1].strip()

                # Фильтруем пустые SSID
                if ssid:
                    networks.append({"ssid": ssid, "signal": signal})

        return networks if networks else {"error": "No networks found"}

    except Exception as e:
        return {"error": str(e)}