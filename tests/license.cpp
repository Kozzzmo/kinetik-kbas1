// Licence verification test. Fixtures are signed with a fixed TEST key that the plugins never trust.
// build: gcc -O2 -c src/third_party/monocypher/monocypher.c src/third_party/monocypher/monocypher-ed25519.c
//        g++ -O2 -std=c++17 -Isrc tests/license.cpp monocypher.o monocypher-ed25519.o -o /tmp/t && /tmp/t
#include "License.hpp"
using namespace kn::lic;
static const uint8_t kTestKey[32] = {0x03, 0xa1, 0x07, 0xbf, 0xf3, 0xce, 0x10, 0xbe, 0x1d, 0x70, 0xdd, 0x18, 0xe7, 0x4b, 0xc0, 0x99, 0x67, 0xe4, 0xd6, 0x30, 0x9b, 0xa5, 0x0d, 0x5f, 0x1d, 0xdc, 0x86, 0x64, 0x12, 0x55, 0x31, 0xb8};
static std::string rd(const char* f) { std::string s; readFile(std::string("tests/license-fixtures/") + f, s); return s; }
int main()
{
    int fails = 0;
    auto chk = [&](const char* what, bool c) { std::printf("%s %s\n", c ? "ok  " : "FAIL", what); fails += !c; };
    const std::string mine = rd("kdsp.lic"), other = rd("kmsp.lic");
    chk("own licence accepted", verify(mine, "KDSP-16", kTestKey).valid);
    chk("other product refused", !verify(other, "KDSP-16", kTestKey).valid);
    chk("bundle accepted", verify(rd("bundle.lic"), "KDSP-16", kTestKey).valid);
    chk("UTF-8 name kept", verify(rd("bundle.lic"), "KDSP-16", kTestKey).name == "Zoë Test");
    chk("tampered refused", !verify(rd("tampered.lic"), "KDSP-16", kTestKey).valid);
    chk("test key not trusted by the product", !verify(mine, "KDSP-16").valid);
    std::string pasted = "Hello,\r\n" + mine + "\r\nBye";
    for (size_t i = 0; (i = pasted.find('\n', i)) != std::string::npos; i += 2) pasted.insert(i, "\r");
    chk("pasted text with CRLF accepted", verify(pasted, "KDSP-16", kTestKey).valid);
    chk("garbage refused", !verify("hello", "KDSP-16", kTestKey).valid);
    std::printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails;
}
