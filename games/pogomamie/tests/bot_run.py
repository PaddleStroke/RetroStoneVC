"""Run one bot attempt, ending when its first game-over event has been recorded."""
import subprocess
import sys

binary, log_path, seed = sys.argv[1:]
with open(log_path, "w") as log:
    process = subprocess.Popen(
        [binary, "--frames", "90000", "--opt", "music=0", "--opt", "dump=1",
         "--opt", "bot=1", "--opt", "ready=1", "--opt", "seed=" + seed],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    ended = False
    for line in process.stdout:
        log.write(line)
        if "run 1 over at frame" in line:
            ended = True
            process.terminate()
            break
    result = process.wait()
    if not ended and result != 0:
        sys.exit(result)
