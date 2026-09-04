# wlite — Makefile for CLI and bindings
#
# Requires libwlite to be installed or built nearby.
# Set LIBWLITE_DIR to the libwlite directory if not installed system-wide.

CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -std=c11 -pedantic -O2
LDFLAGS ?=
LIBS    ?= -lsqlite3

LIBWLITE_DIR ?= ../libwlite

# If libwlite is not installed, use the local build
LIBWLITE_CFLAGS = -I$(LIBWLITE_DIR)/include
LIBWLITE_LDFLAGS = -L$(LIBWLITE_DIR) -lwlite

# Check if libwlite is installed system-wide
ifeq ($(shell pkg-config --exists wlite 2>/dev/null && echo yes),yes)
    LIBWLITE_CFLAGS = $(shell pkg-config --cflags wlite)
    LIBWLITE_LDFLAGS = $(shell pkg-config --libs wlite)
endif

# Targets
.PHONY: all clean test

all: wlite

wlite: cli/main.c
	$(CC) $(CFLAGS) $(LIBWLITE_CFLAGS) -o $@ $< $(LIBWLITE_LDFLAGS) $(LIBS) $(LDFLAGS)

clean:
	rm -f wlite

test: wlite
	@echo "CLI built. Run: ./wlite version"

install: wlite
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 wlite $(DESTDIR)/usr/local/bin/
