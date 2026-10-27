import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BIN = ROOT / "build" / "bin"
SUFFIX = ".exe" if sys.platform == "win32" else ""

SEQUENCE = [
    ("nrinit", ["--no-dialogs"]),
    ("nrscan", ["10.0.0.14"]),
    ("nrtrace", ["10.0.0.14"]),
    ("nrcrack", ["creds.enc"]),
    ("nrdump", ["eth0", "--count", "8"]),
    ("nrfind", ["nrhash", "--root", ".", "--max", "4"]),
    ("nrstrings", ["README.md", "--min", "8", "--max", "6"]),
    ("nrhash", ["README.md"]),
    ("nrhex", ["README.md", "--length", "64"]),
    ("nrpe", ["build/bin/nrinit.exe"]),
]


def resolve(tool):
    local = BIN / f"{tool}{SUFFIX}"
    if local.exists():
        return str(local)
    found = shutil.which(tool)
    if found:
        return found
    sys.exit(f"{tool} not found - build it first")


def main():
    fast = "--fast" in sys.argv[1:]
    for tool, extra in SEQUENCE:
        argv = [resolve(tool), *extra]
        if fast:
            argv.append("--fast")
        print(f"\n$ {tool} {' '.join(extra)}", flush=True)
        subprocess.run(argv, check=False)
        time.sleep(0.0 if fast else 0.7)
    print()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
