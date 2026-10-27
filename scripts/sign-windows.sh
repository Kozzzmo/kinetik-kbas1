#!/bin/sh
# Sign Windows binaries in place with osslsigncode when a certificate is configured.
# Secrets (GitHub repository settings): WINDOWS_CERTIFICATE (base64 .pfx), WINDOWS_CERTIFICATE_PASSWORD.
# Without them this script does nothing (unsigned build), so the pipeline still works.
set -e
[ -n "$WINDOWS_CERTIFICATE" ] || { echo "no Windows certificate: $* left unsigned"; exit 0; }
CERT=$(mktemp); echo "$WINDOWS_CERTIFICATE" | base64 -d > "$CERT"
for f in "$@"; do
  osslsigncode sign -pkcs12 "$CERT" -pass "$WINDOWS_CERTIFICATE_PASSWORD" \
    -n "${SIGN_DESCRIPTION:-Kinetik}" -h sha256 -ts http://timestamp.digicert.com \
    -in "$f" -out "$f.signed" && mv "$f.signed" "$f"
  echo "signed $f"
done
rm -f "$CERT"
