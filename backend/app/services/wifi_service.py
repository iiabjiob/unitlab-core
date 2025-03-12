import subprocess

def scan_wifi():
    try:
        result = subprocess.run(["nmcli", "-t", "-f", "SSID,SIGNAL", "dev", "wifi"], capture_output=True, text=True)
        networks = []
        for line in result.stdout.splitlines():
            parts = line.split(":")
            if len(parts) >= 2:
                networks.append({"ssid": parts[0], "signal": parts[1]})
        return networks
    except Exception as e:
        return {"error": str(e)}