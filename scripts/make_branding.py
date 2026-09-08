"""Generate original installer icon/header assets using only Python's standard library."""
from pathlib import Path
import struct
import zlib

root = Path(__file__).resolve().parents[1] / "assets"

def pixel(x, y, width, height):
    # 4 x 4 mark, with subdued corner cells.
    size = 112 if width == 164 else (48 if width == 150 else 56)
    left, top = (width - size) // 2, (height - size) // 2
    step = size // 4
    if left <= x < left + size and top <= y < top + size:
        u, v = x - left, y - top
        if u % step < step - 3 and v % step < step - 3:
            fade = 0.5 if (u // step, v // step) in ((3, 0), (0, 3)) else 1
            return tuple(int(c * fade) for c in (86, 224 - (y-top) // 4, 197)) + (255,)
    return (17, 31, 44, 255)

def png(width, height):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)
    raw = b"".join(b"\0" + bytes(c for x in range(width) for c in pixel(x, y, width, height)) for y in range(height))
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")

for filename,width,height in (("installer-header.bmp",150,57),("installer-welcome.bmp",164,314)):
    stride = (width * 3 + 3) & ~3
    rows = b"".join(bytes(c for x in range(width) for c in pixel(x, y, width, height)[2::-1]) + bytes(stride - width * 3) for y in reversed(range(height)))
    header = struct.pack("<IiiHHIIiiII", 40, width, height, 1, 24, 0, len(rows), 2835, 2835, 0, 0)
    (root / filename).write_bytes(struct.pack("<2sIHHI", b"BM", 54 + len(rows), 0, 0, 54) + header + rows)
icon = png(64, 64)
(root / "app.ico").write_bytes(struct.pack("<HHH", 0, 1, 1) + struct.pack("<BBBBHHII", 64, 64, 0, 0, 1, 32, len(icon), 22) + icon)
(root / "app.png").write_bytes(icon)
print("Generated installer icon, header and welcome artwork")
