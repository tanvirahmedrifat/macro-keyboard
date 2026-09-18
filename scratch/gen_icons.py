from PIL import Image, ImageDraw
import math

def create_icon(name, size, draw_func):
    img = Image.new('1', (size, size), 0)
    draw = ImageDraw.Draw(img)
    draw_func(draw, size)

    temp_path = f"scratch/{name}_{size}.xbm"
    img.save(temp_path)

    with open(temp_path, "r") as f:
        data = f.read()

    start = data.find("{")
    end = data.find("}")
    arr = data[start:end+1]

    return f"const unsigned char {name}_{size}[] PROGMEM = {arr};\n"

def draw_profile(draw, size):
    r = size // 2
    draw.ellipse((r-size//4, r-size//2+2, r+size//4, r-2), fill=1)
    draw.ellipse((r-size//2.5, r, r+size//2.5, r+size//2.5 * 2), fill=1)
    draw.rectangle((0, size, size, size+10), fill=0)

def draw_wifi(draw, size):
    r = size // 2
    cx, cy = r, size - size//4
    draw.ellipse((cx-3, cy-3, cx+3, cy+3), fill=1)
    for i in range(1, 4):
        radius = i * (size//3.5)
        draw.ellipse((cx-radius, cy-radius, cx+radius, cy+radius), outline=1, width=max(1, size//12))
    draw.rectangle((0, cy, size, size+10), fill=0)
    draw.polygon([(cx, cy), (0, cy), (0, -10)], fill=0)
    draw.polygon([(cx, cy), (size, cy), (size, -10)], fill=0)
    
def draw_presenter(draw, size):
    pad = size//6
    draw.rounded_rectangle((pad, pad, size-pad, size-pad*2), radius=2, outline=1, width=max(1, size//12))
    cx = size//2
    draw.rectangle((cx-size//8, size-pad*2, cx+size//8, size-pad), fill=1)
    draw.rectangle((cx-size//3, size-pad, cx+size//3, size-pad+size//12), fill=1)

def draw_media(draw, size):
    pad = size//5
    w = max(1, size//10)
    draw.rectangle((pad*1.5, pad, pad*1.5+w, size-pad*1.5), fill=1)
    draw.rectangle((size-pad*1.5-w, pad, size-pad*1.5, size-pad*1.5), fill=1)
    draw.rectangle((pad*1.5, pad, size-pad*1.5, pad+w*2), fill=1)
    draw.ellipse((pad, size-pad*2, pad*2.5, size-pad), fill=1)
    draw.ellipse((size-pad*2.5, size-pad*2, size-pad, size-pad), fill=1)

def draw_tester(draw, size):
    r = size//2
    cx, cy = r, r
    draw.ellipse((cx-r//1.5, cy-r//1.5, cx+r//1.5, cy+r//1.5), outline=1, width=max(2, size//8))
    w = size//8
    draw.rectangle((cx-w, 2, cx+w, size-2), fill=1)
    draw.rectangle((2, cy-w, size-2, cy+w), fill=1)
    draw.ellipse((cx-r//2.5, cy-r//2.5, cx+r//2.5, cy+r//2.5), fill=0)

out = ""
for name, func in [("icon_profile", draw_profile),
                   ("icon_wifi", draw_wifi),
                   ("icon_presenter", draw_presenter),
                   ("icon_media", draw_media),
                   ("icon_tester", draw_tester)]:
    out += create_icon(name, 24, func)
    out += create_icon(name, 48, func)

with open("/home/tanvir/Projects-AI/Macro-Keyboard/menu_icons.h", "w") as f:
    f.write("#pragma once\n\n#include <pgmspace.h>\n\n" + out)

print("Icons rebuilt.")
