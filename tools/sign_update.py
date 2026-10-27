#!/usr/bin/env python3
"""Write the signed update manifest (update.txt) that installed Kinetik plugins read to offer an update.

usage (CI release job):
  KINETIK_UPDATE_KEY=<64 hex> sign_update.py --product KDSP-16 --version 0.9 --repo kinetik-kdsp16 \
      --mac dist/Kinetik-KDSP16-macOS.pkg --win dist/Kinetik-KDSP16-Windows-Setup.exe \
      --changelog CHANGELOG.md --out dist/update.txt
The key (Ed25519 seed, hex) can also come from --key FILE ("KINETIK-UPDATE-PRIVATE-KEY 1" + hex line).
Requires: pip install cryptography
"""
import argparse, hashlib, os, re, sys
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

def one_line(s):
    return " ".join(s.replace(":", " -").split())

def notes_from_changelog(path, version):
    if not path or not os.path.exists(path):
        return ""
    text = open(path, encoding="utf-8").read()
    m = re.search(r"^## " + re.escape(version) + r"\s*$(.*?)(?=^## |\Z)", text, re.S | re.M)
    if not m:
        return ""
    items = []
    for line in m.group(1).splitlines():
        line = line.strip()
        if line.startswith("- "):
            items.append(re.sub(r"\*\*(.*?)\*\*", r"\1", line[2:]).split(":")[0].strip().rstrip("."))
    return one_line("; ".join(items))[:300]

def main():
    a = argparse.ArgumentParser()
    a.add_argument("--product", required=True)
    a.add_argument("--version", required=True)
    a.add_argument("--repo", required=True)
    a.add_argument("--mac"); a.add_argument("--win")
    a.add_argument("--changelog"); a.add_argument("--notes")
    a.add_argument("--tag", help="release tag holding the files (default v<version>; J-Slice: latest)")
    a.add_argument("--key"); a.add_argument("--out", required=True)
    o = a.parse_args()
    hexkey = os.environ.get("KINETIK_UPDATE_KEY", "").strip()
    if o.key:
        lines = [l.strip() for l in open(o.key) if l.strip()]
        if not lines or lines[0] != "KINETIK-UPDATE-PRIVATE-KEY 1":
            sys.exit("not a Kinetik update key file")
        hexkey = lines[1]
    if len(hexkey) != 64:
        sys.exit("no update key (KINETIK_UPDATE_KEY)")
    key = Ed25519PrivateKey.from_private_bytes(bytes.fromhex(hexkey))
    tag = o.tag or f"v{o.version}"
    base = f"https://github.com/Kozzzmo/{o.repo}/releases/download/{tag}/"
    page = f"https://github.com/Kozzzmo/{o.repo}/releases/" + ("latest" if not o.tag else f"tag/{tag}")
    fields = [("product", o.product), ("version", o.version),
              ("notes", one_line(o.notes) if o.notes else notes_from_changelog(o.changelog, o.version)),
              ("page", page)]
    for tag, f in (("mac", o.mac), ("win", o.win)):
        if f:
            fields.append((tag, base + os.path.basename(f)))
            fields.append((tag + "-sha512", hashlib.sha512(open(f, "rb").read()).hexdigest()))
    body = "KINETIK-UPDATE 1\n" + "".join(f"{k}: {v}\n" for k, v in fields)
    sig = key.sign(body.encode("utf-8")).hex()
    open(o.out, "w").write(body + f"signature: {sig}\n")
    print(body + f"signature: {sig[:16]}...")

if __name__ == "__main__":
    main()
