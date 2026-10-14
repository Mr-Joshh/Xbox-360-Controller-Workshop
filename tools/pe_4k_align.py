"""Mark an Xbox 360 PE as 4 KB-section-aligned so imagexex builds a 4 KB-page XEX.

The XDK linker always emits 64 KB section alignment for PPC (only /DRIVER makes it
honour /ALIGN:4096, and that adds discardable INIT code and non-paged sections that
break plugins). System plugins at 0x9xxxxxxx need 4 KB pages, and imagexex picks the
page size from SectionAlignment. Every section here already starts on a 64 KB
boundary, which is also a 4 KB boundary, so lowering the field keeps the file valid.

    python pe_4k_align.py <file.dll>
"""

import struct
import sys

path = sys.argv[1]
d = bytearray(open(path, "rb").read())
if d[:2] != b"MZ":
    sys.exit("not a PE file")
pe = struct.unpack_from("<I", d, 0x3C)[0]
opt = pe + 24
salign = struct.unpack_from("<I", d, opt + 32)[0]
if salign == 0x1000:
    print("already 4 KB aligned")
    sys.exit(0)

nsec = struct.unpack_from("<H", d, pe + 6)[0]
optsize = struct.unpack_from("<H", d, pe + 20)[0]
for i in range(nsec):
    va = struct.unpack_from("<I", d, opt + optsize + i * 40 + 12)[0]
    if va % 0x1000:
        sys.exit(f"section {i} at {va:#x} is not 4 KB aligned")

struct.pack_into("<I", d, opt + 32, 0x1000)
open(path, "wb").write(d)
print(f"section alignment {salign:#x} -> 0x1000")
