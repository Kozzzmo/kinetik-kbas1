# Kinetik KBAS-1
#   make setup      fetch + patch DPF (once)
#   make            build for the current system (macOS: VST3, AU, CLAP; Linux: VST3, CLAP, JACK)
#   make universal  macOS only: one binary for Apple Silicon + Intel
#   make WINDOWS=true CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++   cross-build for Windows

MAC_FLAGS = -arch x86_64 -arch arm64 -mmacosx-version-min=10.15

all: dpf/Makefile.base.mk
	$(MAKE) -C src

universal: dpf/Makefile.base.mk
	$(MAKE) -C src CFLAGS="$(MAC_FLAGS)" CXXFLAGS="$(MAC_FLAGS)" LDFLAGS="$(MAC_FLAGS)"

setup dpf/Makefile.base.mk:
	./scripts/setup.sh

clean:
	rm -rf bin build dpf/build

.PHONY: all universal setup clean
