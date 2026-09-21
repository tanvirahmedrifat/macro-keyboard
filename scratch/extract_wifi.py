import os, configparser, sys

d = "/etc/NetworkManager/system-connections/"
if not os.path.exists(d):
    sys.exit(1)

for f in os.listdir(d):
    c = configparser.ConfigParser()
    try:
        c.read(os.path.join(d, f))
        if "wifi" in c and "ssid" in c["wifi"]:
            ssid = c["wifi"]["ssid"]
            psk = c["wifi-security"]["psk"] if "wifi-security" in c and "psk" in c["wifi-security"] else ""
            print(f"{ssid}:::{psk}")
    except Exception:
        pass
