"""A genuine version-1 battery file retains its records when a four-player result writes version 2."""
import pathlib
import struct
import subprocess
import sys
import tempfile

def checksum(data):
    total = 0x5153
    for byte in data:
        total = (total * 31 + byte) & 0xffff
    return total

with tempfile.TemporaryDirectory(prefix="beaver-save-") as folder:
    path = pathlib.Path(folder) / "beaverrush.srm"
    old = struct.pack("<4sB3xHHI4H2H", b"BVRU", 1, 1234, 567, 9, 1, 2, 3, 4, 5, 6)
    path.write_bytes(old + struct.pack("<H", checksum(old)) + bytes(32768 - len(old) - 2))
    run = subprocess.run([sys.argv[1], "--frames", "1400", "--sram", str(path),
                          "--opt", "music=0", "--opt", "bot=4", "--opt", "players=4",
                          "--opt", "ready=1", "--opt", "botstop=3", "--opt", "seed=5"],
                         capture_output=True, text=True, check=True)
    data = path.read_bytes()
    fields = struct.unpack("<4sB3xHHI4H4HH", data[:34])
    assert fields[:5] == (b"BVRU", 2, 1234, 567, 10), (fields, run.stderr)
    assert fields[5:9] == (1, 2, 3, 4), fields
    wins = fields[9:13]
    assert wins[0] >= 5 and wins[1] >= 6 and sum(wins) == 12, fields
    assert fields[13] == checksum(data[:32]), fields
    print("  ok   version-1 records, medals and wins migrate to four-player version 2")
