import os, sys

d = "/run/NetworkManager/system-connections/"
if not os.path.exists(d):
    print(f"Directory {d} does not exist.")
    sys.exit(1)

for f in os.listdir(d):
    try:
        filepath = os.path.join(d, f)
        if not os.path.isfile(filepath): continue
        with open(filepath, 'r') as file:
            content = file.readlines()
            ssid = ""
            psk = ""
            for line in content:
                line = line.strip()
                if line.startswith("ssid="):
                    ssid = line.split('=', 1)[1]
                elif line.startswith("psk="):
                    psk = line.split('=', 1)[1]
            if ssid:
                print(f"{ssid}:::{psk}")
    except Exception as e:
        pass
