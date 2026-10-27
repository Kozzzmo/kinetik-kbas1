#!/usr/bin/make -f
# Kinetik KBAS-1 — build with DPF (https://github.com/DISTRHO/DPF)
NAME = KinetikKBAS1
FILES_DSP = KbasPlugin.cpp third_party/monocypher/monocypher.c third_party/monocypher/monocypher-ed25519.c
FILES_UI  = KbasUI.cpp Updater.cpp
DPF_PATH ?= ../dpf
DPF_TARGET_DIR = ../bin
DPF_BUILD_DIR = ../build
include $(DPF_PATH)/Makefile.plugins.mk
BUILD_CXX_FLAGS += -std=gnu++17 -DKN_VERSION='"$(shell cat ../VERSION)"'
KN_VPARTS := $(subst ., ,$(shell cat ../VERSION))
BUILD_CXX_FLAGS += -DKN_VER_MAJOR=$(or $(word 1,$(KN_VPARTS)),0) -DKN_VER_MINOR=$(or $(word 2,$(KN_VPARTS)),0) -DKN_VER_MICRO=$(or $(word 3,$(KN_VPARTS)),0)
ifeq ($(WINDOWS),true)
LINK_FLAGS += -lole32 -lshell32 -luuid
endif
TARGETS = vst3 clap
ifeq ($(MACOS),true)
TARGETS += au
endif
ifeq ($(LINUX),true)
TARGETS += jack
endif
all: $(TARGETS)
