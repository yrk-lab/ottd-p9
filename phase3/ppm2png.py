#!/usr/bin/env python3
# ppm2png in.ppm out.png
#	Convert the draw shim's screenshot (binary PPM, P6, 8 bits) to PNG,
#	with the standard library only.
import struct
import sys
import zlib

if len(sys.argv) != 3:
	sys.exit("usage: ppm2png in.ppm out.png")
d = open(sys.argv[1], "rb").read()
fields = []
i = 0
while len(fields) < 4:
	while d[i:i+1].isspace():
		i += 1
	j = i
	while not d[j:j+1].isspace():
		j += 1
	fields.append(d[i:j])
	i = j
i += 1
if fields[0] != b"P6" or fields[3] != b"255":
	sys.exit("ppm2png: not an 8-bit P6 PPM")
w, h = int(fields[1]), int(fields[2])
pix = d[i:i + w*h*3]
raw = b"".join(b"\0" + pix[y*w*3:(y+1)*w*3] for y in range(h))


def chunk(t, b):
	c = struct.pack(">I", len(b)) + t + b
	return c + struct.pack(">I", zlib.crc32(t + b) & 0xffffffff)


png = b"\x89PNG\r\n\x1a\n"
png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(raw, 6))
png += chunk(b"IEND", b"")
open(sys.argv[2], "wb").write(png)
