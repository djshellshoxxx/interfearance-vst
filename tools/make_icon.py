"""Generates Resources/icon.png: the Interference app icon (original artwork).
A TS plug tip leaving a jack sleeve with a contact spark, on the theme.md palette."""
from PIL import Image, ImageDraw, ImageFilter
import math, os

S = 1024
img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(img)
bg, panel, edge = (14, 17, 22, 255), (23, 27, 34, 255), (42, 48, 58, 255)
accent, teal, text = (232, 83, 42, 255), (79, 182, 196, 255), (230, 232, 236, 255)

d.rounded_rectangle([40, 40, S - 40, S - 40], radius=180, fill=bg, outline=edge, width=10)
# signature notch
d.line([(150, 260), (260, 150)], fill=accent, width=34)

cy = S // 2 + 40
# jack sleeve (right)
d.rounded_rectangle([590, cy - 170, 900, cy + 170], radius=40, fill=panel, outline=edge, width=8)
d.rectangle([590, cy - 80, 860, cy + 80], fill=bg)
d.rectangle([600, cy - 96, 650, cy - 80], fill=teal)
d.rectangle([600, cy + 80, 650, cy + 96], fill=teal)
# plug (left): handle, sleeve, insulator, tip
d.rounded_rectangle([110, cy - 120, 300, cy + 120], radius=30, fill=(35, 40, 51, 255), outline=edge, width=8)
d.rectangle([300, cy - 70, 470, cy + 70], fill=(150, 156, 166, 255))
d.rectangle([470, cy - 60, 500, cy + 60], fill=(20, 24, 31, 255))
d.rounded_rectangle([500, cy - 52, 590, cy + 52], radius=52, fill=(200, 204, 211, 255))
# spark at the gap
glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
g = ImageDraw.Draw(glow)
g.ellipse([540, cy - 150, 700, cy + 10], fill=(232, 83, 42, 150))
glow = glow.filter(ImageFilter.GaussianBlur(40))
img = Image.alpha_composite(img, glow)
d = ImageDraw.Draw(img)
cx, sy = 620, cy - 70
for i in range(8):
    a = i * math.pi / 4 + 0.3
    r = 70 if i % 2 == 0 else 42
    d.line([(cx, sy), (cx + math.cos(a) * r, sy + math.sin(a) * r)], fill=(242, 193, 78, 255), width=14)
# broken signal trace
d.line([(170, 820), (400, 820)], fill=text, width=16)
d.line([(470, 820), (520, 760), (560, 880), (600, 820)], fill=accent, width=16)
d.line([(660, 820), (860, 820)], fill=text, width=16)

img = img.resize((512, 512), Image.LANCZOS)
os.makedirs("Resources", exist_ok=True)
img.save("Resources/icon.png")
print("wrote Resources/icon.png")
