// Kinetik — licence files (shared by every Kinetik product; keep identical in all repositories)
//
// A licence is a small text file signed with Ed25519 by the seller:
//
//   KINETIK-LICENSE 1
//   product: KDSP-16            (or KMSP-6, or BUNDLE for every Kinetik product)
//   name: Jane Doe
//   email: jane@example.com
//   order: 12345
//   issued: 2026-10-11
//   signature: <128 hex characters>
//
// The signature covers every line before "signature:", each line trimmed and ended by '\n'.
// Installed licences live in <user data>/Kinetik/Licenses/*.kinetik-license.
#pragma once
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "FsUtf8.hpp"

extern "C" {
#include "third_party/monocypher/monocypher-ed25519.h"
}

namespace kn {
namespace lic {

// Demo mode (3 s of silence every 45 s without a licence). OFF during the beta test: set to true for the
// commercial release.
static constexpr bool kDemoModeEnabled = false;

// public key of the Kinetik licence signer (the private key never goes into a repository)
static const uint8_t kPublicKey[32] = {
    0xc5, 0x07, 0x6d, 0x03, 0xb1, 0x52, 0x4d, 0x2a, 0x32, 0xd8, 0x2c, 0x01, 0x70, 0x9d, 0xd3, 0x33,
    0x26, 0x45, 0xa3, 0x2f, 0xb7, 0x5c, 0x65, 0x28, 0x61, 0xa4, 0x70, 0x81, 0x38, 0xef, 0x23, 0x7e};

struct Info {
    bool valid = false;
    std::string product, name, email, order, issued;
    std::string error;             // why a licence was refused (for the UI)
};

inline std::string trim(const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n' || (unsigned char)s[a] == 0xEF || (unsigned char)s[a] == 0xBB || (unsigned char)s[a] == 0xBF)) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}

inline bool fromHex(const std::string& h, uint8_t* out, size_t n)
{
    if (h.size() != n * 2) return false;
    for (size_t i = 0; i < n; ++i) {
        unsigned v = 0;
        for (int k = 0; k < 2; ++k) {
            const char c = h[i * 2 + k];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
            else return false;
        }
        out[i] = (uint8_t)v;
    }
    return true;
}

// Check a licence text for a product ("KDSP-16", "KMSP-6", ...).
inline Info verify(const std::string& text, const char* product, const uint8_t* publicKey = kPublicKey)
{
    Info info;
    std::string signedPart, sigHex;
    bool header = false;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t e = text.find('\n', pos);
        if (e == std::string::npos) e = text.size();
        const std::string line = trim(text.substr(pos, e - pos));
        pos = e + 1;
        if (line.empty()) continue;
        if (!header) {
            if (line != "KINETIK-LICENSE 1") continue;   // tolerate text pasted around the licence
            header = true;
            signedPart = line + "\n";
            continue;
        }
        const size_t c = line.find(':');
        if (c == std::string::npos) continue;
        const std::string key = trim(line.substr(0, c)), val = trim(line.substr(c + 1));
        if (key == "signature") { sigHex = val; break; }
        signedPart += key + ": " + val + "\n";
        if (key == "product") info.product = val;
        else if (key == "name") info.name = val;
        else if (key == "email") info.email = val;
        else if (key == "order") info.order = val;
        else if (key == "issued") info.issued = val;
    }
    if (!header) { info.error = "This is not a Kinetik licence."; return info; }
    uint8_t sig[64];
    if (!fromHex(sigHex, sig, 64)) { info.error = "The licence is incomplete (signature missing)."; return info; }
    if (crypto_ed25519_check(sig, publicKey, (const uint8_t*)signedPart.data(), signedPart.size()) != 0) {
        info.error = "The licence signature is not valid.";
        return info;
    }
    if (info.product != product && info.product != "BUNDLE") {
        info.error = "This licence is for " + info.product + ", not " + product + ".";
        return info;
    }
    info.valid = true;
    return info;
}

inline std::string licenseDir(bool create)
{
#ifdef _WIN32
    const std::string base = fs::env("APPDATA");
    if (base.empty()) return std::string();
    std::string d = base + "\\Kinetik";
    if (create) fs::makeDir(d);
    d += "\\Licenses";
    if (create) fs::makeDir(d);
    return d + "\\";
#else
    const std::string base = fs::env("HOME");
    if (base.empty()) return std::string();
   #ifdef __APPLE__
    std::string d = base + "/Library/Application Support";
   #else
    std::string d = base + "/.config";
   #endif
    if (create) fs::makeDir(d);
    d += "/Kinetik";
    if (create) fs::makeDir(d);
    d += "/Licenses";
    if (create) fs::makeDir(d);
    return d + "/";
#endif
}

inline bool readFile(const std::string& path, std::string& out, size_t maxBytes = 64 * 1024)
{
    FILE* fp = fs::open(path, "rb");
    if (!fp) return false;
    out.clear();
    char buf[4096]; size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0 && out.size() < maxBytes) out.append(buf, n);
    std::fclose(fp);
    return true;
}

// First valid installed licence for this product (valid == false when none).
inline Info findInstalled(const char* product)
{
    Info none;
    const std::string dir = licenseDir(false);
    if (dir.empty()) return none;
    Info found;
    for (const std::string& f : fs::list(dir, ".kinetik-license")) {
        std::string text;
        if (!readFile(dir + f, text)) continue;
        Info i = verify(text, product);
        if (i.valid) { found = i; break; }
    }
    return found;
}

// Verify a licence (file contents or pasted text) and install it. Returns the licence info.
inline Info install(const std::string& text, const char* product)
{
    Info i = verify(text, product);
    if (!i.valid) return i;
    const std::string dir = licenseDir(true);
    if (dir.empty()) { i.valid = false; i.error = "Could not find the user folder to store the licence."; return i; }
    std::string safe;
    for (char c : (i.product + "-" + (i.order.empty() ? std::string("licence") : i.order)))
        safe.push_back((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ? c : '_');
    // store the canonical text, not whatever surrounded it
    const size_t a = text.find("KINETIK-LICENSE 1");
    const std::string body = a == std::string::npos ? text : text.substr(a);
    FILE* fp = fs::open(dir + safe + ".kinetik-license", "wb");
    if (!fp) { i.valid = false; i.error = "Could not write the licence file."; return i; }
    std::fwrite(body.data(), 1, body.size(), fp);
    std::fclose(fp);
    return i;
}

} // namespace lic
} // namespace kn
