#!/bin/sh
# Fetch DPF at the tested revision and apply the Kinetik patches.
set -e
cd "$(dirname "$0")/.."
DPF_REV=4238e1c7f0351bbe488d79f0899c540543ac7583
if [ ! -d dpf/.git ]; then
  git clone https://github.com/DISTRHO/DPF.git dpf
fi
git -C dpf fetch --quiet origin
git -C dpf checkout --quiet "$DPF_REV"
git -C dpf submodule update --init --recursive
python3 scripts/dpfpatch.py dpf
echo "DPF ready."
