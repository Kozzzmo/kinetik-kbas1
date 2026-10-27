#!/usr/bin/env python3
"""Create a signed Kinetik licence file.

usage:
  make_license.py --key /path/to/kinetik-license-private.key --product KDSP-16 \
                  --name "Jane Doe" --email jane@example.com --order 12345 [--out file]

product: KDSP-16, KMSP-6, KFX-8, KBAS-1, KGRN-4 or BUNDLE (every Kinetik product).
Requires: pip install cryptography
The private key must NEVER be committed to a repository or sent to anyone.
"""
import argparse, datetime, sys
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

def load_key(path):
    lines = [l.strip() for l in open(path) if l.strip()]
    if not lines or lines[0] != "KINETIK-LICENSE-PRIVATE-KEY 1":
        sys.exit("not a Kinetik private key file")
    return Ed25519PrivateKey.from_private_bytes(bytes.fromhex(lines[1]))

def clean(v):
    return " ".join(str(v).replace(":", " ").split())   # one line, no colon confusion

def make(key, product, name, email, order, issued=None):
    issued = issued or datetime.date.today().isoformat()
    body = "KINETIK-LICENSE 1\n"
    for k, v in (("product", product), ("name", name), ("email", email), ("order", order), ("issued", issued)):
        body += f"{k}: {clean(v)}\n"
    sig = key.sign(body.encode("utf-8")).hex()
    return body + f"signature: {sig}\n"

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--key", required=True)
    p.add_argument("--product", required=True, choices=["KDSP-16", "KMSP-6", "KFX-8", "KBAS-1", "KGRN-4", "BUNDLE"])
    p.add_argument("--name", required=True)
    p.add_argument("--email", required=True)
    p.add_argument("--order", required=True)
    p.add_argument("--issued")
    p.add_argument("--out")
    a = p.parse_args()
    text = make(load_key(a.key), a.product, a.name, a.email, a.order, a.issued)
    out = a.out or f"{a.product}-{clean(a.order).replace(' ', '_')}.kinetik-license"
    open(out, "w", newline="\n").write(text)
    print(f"written {out}")
