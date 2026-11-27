#!/usr/bin/env python3
# sheet out.png shot.png...
#	The screenshots of a test run on one page, four to a row, each
#	labelled with its name's last field (shot.30.png: 30 s).  Needs
#	Pillow (python3-pil).
import re
import sys
from PIL import Image, ImageDraw

if len(sys.argv) < 3:
	sys.exit("usage: sheet out.png shot.png...")
shots = sorted(sys.argv[2:], key=lambda f: int(re.findall(r"(\d+)\.png$", f)[0]))
w, h, label = 341, 256, 16
page = Image.new("RGB", (w*4, (h + label)*((len(shots) + 3)//4)), "white")
d = ImageDraw.Draw(page)
for i, f in enumerate(shots):
	x, y = i%4*w, i//4*(h + label)
	page.paste(Image.open(f).convert("RGB").resize((w, h)), (x, y + label))
	d.text((x + 4, y + 2), "t=%ss" % re.findall(r"(\d+)\.png$", f)[0], fill="black")
page.save(sys.argv[1])
