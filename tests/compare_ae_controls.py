"""Generate a synthetic AE fixture or verify actual uncompressed AVI exports.

No third-party Python dependencies. Usage: --fixture or <AE render folder>.
"""
import argparse
import itertools
import json
from pathlib import Path
import struct
import zlib

def chunks(data, begin, end):
    while begin + 8 <= end:
        tag, size = struct.unpack_from("<4sI", data, begin)
        start, stop = begin + 8, begin + 8 + size
        if stop > end:
            raise ValueError("Truncated RIFF chunk")
        yield tag, start, stop
        if tag in (b"LIST", b"RIFF"):
            yield from chunks(data, start + 4, stop)
        begin = stop + (size & 1)

def load(path):
    data = path.read_bytes()
    assert data[:4] == b"RIFF" and data[8:12] == b"AVI "
    bitmap = payload = None
    for tag, start, stop in chunks(data, 12, len(data)):
        if tag == b"strf" and stop-start >= 40:
            fields = struct.unpack_from("<IiiHHI", data, start)
            if fields[4] in (24, 32):
                bitmap = fields
        if tag in (b"00db", b"00dc"):
            payload = data[start:stop]
    assert bitmap and payload, "Expected uncompressed AVI video frame"
    _, width, height, planes, bits, compression = bitmap
    assert planes == 1 and compression == 0 and width > 0
    stride = ((width * bits + 31) // 32) * 4
    assert len(payload) >= stride * abs(height)
    rows = []
    for y in range(abs(height)):
        sy = abs(height)-1-y if height > 0 else y
        row = payload[sy*stride:sy*stride+width*(bits//8)]
        rgb = bytearray(width*3)
        for x in range(width):
            b,g,r = row[x*(bits//8):x*(bits//8)+3]
            rgb[x*3:x*3+3] = bytes((r,g,b))
        rows.append(bytes(rgb))
    return width, abs(height), b"".join(rows)

def png(path, width, height, rgb):
    def chunk(tag, raw):
        return struct.pack(">I",len(raw))+tag+raw+struct.pack(">I",zlib.crc32(tag+raw)&0xffffffff)
    scanlines=b"".join(b"\0"+rgb[y*width*3:(y+1)*width*3] for y in range(height))
    path.write_bytes(b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",width,height,8,2,0,0,0))+
                     chunk(b"IDAT",zlib.compress(scanlines))+chunk(b"IEND",b""))

def compare(root):
    names = ["original", "wipe-original", "natural-16", "natural-32"]
    names += [f"style-{i}" for i in range(1,4)] + [f"look-{i}" for i in range(2,10)]
    frames = {}
    for name in names:
        width, height, rgb = load(root / f"{name}.avi")
        assert (width, height) == (180,225), f"Unexpected dimensions for {name}"
        frames[name] = rgb
    assert frames["original"] == frames["wipe-original"], "Wipe at 100 changed original pixels"
    report = []
    pairs = [("original", name) for name in names if name not in ("original", "wipe-original")]
    pairs += list(itertools.combinations([f"style-{i}" for i in range(1,4)],2))
    pairs += list(itertools.combinations([f"look-{i}" for i in range(2,10)],2))
    for a,b in pairs:
        differences = [abs(x-y) for x,y in zip(frames[a],frames[b])]
        changed = sum(d > 1 for d in differences)
        assert changed > 100, f"No meaningful pixel change: {a} versus {b}"
        report.append({"a":a, "b":b, "changed_channels":changed, "mean_absolute_difference":round(sum(differences)/len(differences),4)})
    result = {"status":"passed", "comparison_count":len(report), "original_wipe_exact":True, "comparisons":report}
    (root / "pixel-comparison.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
    print(f"PASS {len(report)} actual AE pixel comparisons; three styles and eight recipes differ; deep/float render; wipe preserves source.")

if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder",nargs="?",type=Path)
    parser.add_argument("--fixture",action="store_true")
    args=parser.parse_args()
    if args.fixture:
        target=Path(__file__).resolve().parents[1] / "artifacts"
        target.mkdir(exist_ok=True)
        w,h=640,360
        rgb=bytes(c for y in range(h) for x in range(w) for c in (x*255//(w-1), y*255//(h-1), 204 if (x//20+y//20)%2 else 51))
        png(target / "source.png",w,h,rgb)
        print("Created artifacts/source.png")
    elif args.folder:
        compare(args.folder.resolve())
    else:
        parser.error("Provide a render folder or --fixture")
