#!/bin/sh
# Build the macOS installer package from the plugins in bin/.
# usage: packaging/macos/make-pkg.sh <BinName> "<Product title>" <bundle id> <version> <output.pkg> [installer identity]
# The plugins go to /Library/Audio/Plug-Ins/{VST3,Components,CLAP}; the EULA is shown before installing.
set -e
BIN="$1"; TITLE="$2"; ID="$3"; VER="$4"; OUT="$5"; SIGN="$6"
WORK=$(mktemp -d)
ROOT="$WORK/root/Library/Audio/Plug-Ins"
mkdir -p "$ROOT/VST3" "$ROOT/Components" "$ROOT/CLAP" "$WORK/res"
cp -R "bin/$BIN.vst3" "$ROOT/VST3/"
cp -R "bin/$BIN.component" "$ROOT/Components/"
cp "bin/$BIN.clap" "$ROOT/CLAP/" 2>/dev/null || cp -R "bin/$BIN.clap" "$ROOT/CLAP/"
cp EULA.txt "$WORK/res/EULA.txt"
# optional built-in sample library: SAMPLES_DIR (a folder of category folders) installed to SAMPLES_DEST
if [ -n "$SAMPLES_DIR" ] && [ -n "$SAMPLES_DEST" ] && [ -d "$SAMPLES_DIR" ]; then
  mkdir -p "$WORK/root$SAMPLES_DEST"
  (cd "$SAMPLES_DIR" && find . -type f \( -iname '*.wav' -o -iname '*.aif' -o -iname '*.aiff' \) -exec sh -c 'mkdir -p "$1/$(dirname "$2")" && cp "$2" "$1/$2"' _ "$WORK/root$SAMPLES_DEST" {} \;)
fi
# never let the installer "relocate" a bundle to another copy found on the disk, and always replace the installed
# bundles (version check off: an update must overwrite even if the bundle version looks the same)
pkgbuild --analyze --root "$WORK/root" "$WORK/components.plist"
n=$(/usr/libexec/PlistBuddy -c "Print" "$WORK/components.plist" | grep -c "BundleIsRelocatable" || true)
i=0
while [ "$i" -lt "$n" ]; do
  /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$WORK/components.plist"
  /usr/libexec/PlistBuddy -c "Set :$i:BundleIsVersionChecked false" "$WORK/components.plist"
  /usr/libexec/PlistBuddy -c "Set :$i:BundleOverwriteAction upgrade" "$WORK/components.plist" 2>/dev/null || \
    /usr/libexec/PlistBuddy -c "Add :$i:BundleOverwriteAction string upgrade" "$WORK/components.plist"
  i=$((i + 1))
done
pkgbuild --root "$WORK/root" --component-plist "$WORK/components.plist" --identifier "$ID" --version "$VER" --install-location / "$WORK/plugins.pkg"
cat > "$WORK/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>$TITLE</title>
  <license file="EULA.txt"/>
  <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <domains enable_localSystem="true"/>
  <choices-outline><line choice="default"><line choice="$ID"/></line></choices-outline>
  <choice id="default"/>
  <choice id="$ID" visible="false"><pkg-ref id="$ID"/></choice>
  <pkg-ref id="$ID" version="$VER" onConclusion="none">plugins.pkg</pkg-ref>
</installer-gui-script>
XML
if [ -n "$SIGN" ]; then
  productbuild --distribution "$WORK/distribution.xml" --resources "$WORK/res" --package-path "$WORK" --sign "$SIGN" "$OUT"
else
  productbuild --distribution "$WORK/distribution.xml" --resources "$WORK/res" --package-path "$WORK" "$OUT"
fi
rm -rf "$WORK"
echo "built $OUT"
