# SPDX-License-Identifier: GPL-2.0

CFLAGS += -Wall -Wl,--no-as-needed -O2 -g
CFLAGS += -I../../../../../usr/include/ $(KHDR_INCLUDES)
# Additional include paths needed by kselftest.h
CFLAGS += -I../../

TEST_FILES := ../../../../../Documentation/netlink/specs
TEST_FILES += ../../../../net/ynl

TEST_GEN_FILES += csum
TEST_GEN_FILES += $(patsubst %.c,%.o,$(wildcard *.bpf.c))
TEST_GEN_FILES += xdp_helper

TEST_INCLUDES := $(wildcard py/*.py sh/*.sh)

include ../../lib.mk

include ../bpf.mk
