import argparse
import os
import platform
import shutil
import subprocess
import sys
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from common import sha256_file

NODTOOL_VERSION = "2.0.0-alpha.9"


def read_manifests(paths):
    rows = {}
    for path in paths:
        with open(path) as f:
            for line in f:
                line = line.strip()
                if line:
                    digest, rel = line.split(None, 1)
                    if rows.setdefault(rel, digest) != digest:
                        sys.exit("error: conflicting hashes for " + rel)
    return sorted((digest, rel) for rel, digest in rows.items())


NODTOOL_URL = "https://github.com/encounter/nod/releases/download/v" + NODTOOL_VERSION + "/"
NODTOOL_BUILDS = {
    ("darwin", "arm64"): (
        "nodtool-macos-arm64",
        "5b1c355a4d1a04a69149f6e5e78aba7383329a29a10e029229debe829939c7af",
    ),
    ("darwin", "x86_64"): (
        "nodtool-macos-x86_64",
        "e97ac1b80c4a105780f109628e8468c6f422351e253148f8821aeed6548bac87",
    ),
    ("linux", "x86_64"): (
        "nodtool-linux-x86_64",
        "78bbdf9f7d535b1dca584b43c973797855439613c4789dd7752a73f77865ccab",
    ),
    ("linux", "aarch64"): (
        "nodtool-linux-aarch64",
        "9a6737f49eb0bf04f2446e1f8bfa1d83366ca3ffb464347eb6dae8e5b778ee7d",
    ),
    ("linux", "i686"): (
        "nodtool-linux-i686",
        "5f7dc6621c40a77cea70cc73168d19a285771daee7a1b08c3481b4be293108c1",
    ),
    ("windows", "x86_64"): (
        "nodtool-windows-x86_64.exe",
        "0696c1c4b65fee6358c4bfdfdb8d957a39886d0d5c465ee240eb9b10c237eb73",
    ),
    ("windows", "arm64"): (
        "nodtool-windows-arm64.exe",
        "27b975875cbcf1819d8c74c29806cd00abee9d8535ccca8115fda9afe4ee38c5",
    ),
    ("windows", "x86"): (
        "nodtool-windows-x86.exe",
        "8336261f51d15824408058e272e20a1ccc121938e477644ba6ca093e4c81e194",
    ),
}
MACHINES = {"amd64": "x86_64", "x64": "x86_64", "aarch64": "aarch64", "arm64": "arm64",
            "i386": "i686", "i686": "i686", "x86": "x86"}


def nodtool_build():
    system = platform.system().lower()
    if system.startswith(("cygwin", "msys", "mingw")):
        system = "windows"
    machine = MACHINES.get(platform.machine().lower(), platform.machine().lower())
    if system == "darwin" and machine == "aarch64":
        machine = "arm64"
    if system == "linux" and machine == "arm64":
        machine = "aarch64"
    if system == "windows" and machine == "i686":
        machine = "x86"
    return NODTOOL_BUILDS.get((system, machine))


def download(url, dest):
    try:
        with urllib.request.urlopen(url, timeout=60) as r, open(dest, "wb") as f:
            shutil.copyfileobj(r, f)
        return
    except Exception:
        pass
    curl = shutil.which("curl")
    if not curl:
        raise RuntimeError("download failed: " + url)
    subprocess.run([curl, "-fsSL", "-o", dest, url], check=True)


def find_nodtool(tools_dir):
    env = os.environ.get("NODTOOL")
    if env:
        return env
    build = nodtool_build()
    if build:
        name, digest = build
        local = os.path.join(tools_dir, NODTOOL_VERSION, name)
        if os.path.isfile(local) and sha256_file(local) == digest:
            return local
        os.makedirs(os.path.dirname(local), exist_ok=True)
        print("Downloading " + name + " " + NODTOOL_VERSION)
        tmp = local + ".part"
        try:
            download(NODTOOL_URL + name, tmp)
        except Exception as e:
            sys.exit("error: could not download nodtool (" + str(e) + "). Check your internet connection, "
                     "or download " + NODTOOL_URL + name + " yourself and set NODTOOL=/path/to/it")
        if sha256_file(tmp) != digest:
            os.remove(tmp)
            sys.exit("error: downloaded nodtool has the wrong checksum")
        os.chmod(tmp, 0o755)
        os.replace(tmp, local)
        return local
    found = shutil.which("nodtool")
    if found:
        return found
    cargo = shutil.which("cargo")
    if not cargo:
        sys.exit("error: no prebuilt nodtool for this system. Install Rust and run "
                 "cargo install nodtool --version " + NODTOOL_VERSION + ", or set NODTOOL=/path/to/nodtool")
    env = dict(os.environ, CARGO_HOME=os.path.join(tools_dir, "cargo"))
    subprocess.run([cargo, "install", "nodtool", "--version", NODTOOL_VERSION, "--locked",
                    "--root", tools_dir], check=True, env=env)
    return os.path.join(tools_dir, "bin", "nodtool")


def verify(files_root, rows):
    bad = []
    for digest, rel in rows:
        path = os.path.join(files_root, rel)
        if not os.path.isfile(path) or sha256_file(path) != digest:
            bad.append(rel)
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--iso", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--manifest", nargs="+", required=True)
    ap.add_argument("--tools", required=True)
    a = ap.parse_args()
    rows = read_manifests(a.manifest)
    files_root = os.path.join(a.out, "files")
    stamp = os.path.join(a.out, ".verified")
    if os.path.exists(stamp) and not verify(files_root, rows):
        return
    if not os.path.isfile(a.iso):
        sys.exit("error: " + a.iso + " not found. Place your Super Mario Galaxy (USA) disc image there.")
    nodtool = find_nodtool(os.path.abspath(a.tools))
    tmp = a.out + ".tmp"
    shutil.rmtree(tmp, ignore_errors=True)
    shutil.rmtree(a.out, ignore_errors=True)
    print("Extracting " + a.iso)
    subprocess.run([nodtool, "extract", "-q", a.iso, tmp], check=True)
    for _, rel in rows:
        src = os.path.join(tmp, "files", rel)
        dst = os.path.join(files_root, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        if os.path.isfile(src):
            os.replace(src, dst)
    shutil.rmtree(tmp, ignore_errors=True)
    bad = verify(files_root, rows)
    if bad:
        for rel in bad[:20]:
            print("mismatch: " + rel, file=sys.stderr)
        sys.exit("error: the disc image is not Super Mario Galaxy (USA) RMGE01 rev 0 ("
                 + str(len(bad)) + " source files missing or different)")
    os.makedirs(a.out, exist_ok=True)
    open(stamp, "w").close()
    print("Verified " + str(len(rows)) + " source files")


if __name__ == "__main__":
    main()
