# Copyright (C) 2018 Mark Hills <mark@xwax.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2, as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License version 2 for more details.
#
# You should have received a copy of the GNU General Public License
# version 2 along with this program; if not, write to the Free
# Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA 02110-1301, USA.
#

CC=/root/buildroot-2018.08/output/host/usr/bin/arm-linux-gcc

# Libraries and dependencies

INSTALL ?= install

SDL_CFLAGS ?= `sdl-config --cflags`
SDL_LIBS ?= -L/root/buildroot-2018.08/output/host/lib -L/root/buildroot-2018.08/output/host/arm-buildroot-linux-uclibcgnueabihf/sysroot/usr/lib -Wl,-rpath,/root/buildroot-2018.08/output/host/arm-buildroot-linux-uclibcgnueabihf/sysroot/usr/lib -lpthread -liconv

ALSA_LIBS ?= -lasound

# Installation paths

PREFIX ?= $(HOME)

BINDIR ?= $(PREFIX)/bin
EXECDIR ?= $(PREFIX)/libexec
MANDIR ?= $(PREFIX)/share/man
DOCDIR ?= $(PREFIX)/share/doc

# Build flags

CFLAGS ?= -O3
CFLAGS += -Wall
CPPFLAGS += -MMD
LDFLAGS ?= -O3

# Core objects and libraries

OBJS = controller.o \
	cues.o \
	deck.o \
	device.o \
	dummy.o \
	external.o \
	player.o \
	realtime.o \
	rig.o \
	status.o \
	thread.o \
	track.o \
	alsa.o \
	sc_input.o \
	sc_playlist.o \
	xwax.o
DEVICE_CPPFLAGS = -DWITH_ALSA
DEVICE_LIBS = $(ALSA_LIBS)

# Optional device types

DEPS = $(OBJS:.o=.d)

# Rules

.PHONY:		all
all:		xwax

# Dynamic versioning

.PHONY:		FORCE
.version:	FORCE
		./mkversion -r

VERSION = $(shell ./mkversion)

# Main binary

xwax:		$(OBJS)
xwax:		LDLIBS += $(SDL_LIBS) $(DEVICE_LIBS) -lm
xwax:		LDFLAGS += -pthread

#xwax.o:		CFLAGS += $(SDL_CFLAGS)
xwax.o:		CPPFLAGS += $(DEVICE_CPPFLAGS)
xwax.o:		CPPFLAGS += -DEXECDIR=\"$(EXECDIR)\" -DVERSION=\"$(VERSION)\"
xwax.o:		.version

# Install to system

.PHONY:		install
install:
		$(INSTALL) -D xwax $(DESTDIR)$(BINDIR)/xwax
		$(INSTALL) -D scan $(DESTDIR)$(EXECDIR)/xwax-scan
		$(INSTALL) -D import $(DESTDIR)$(EXECDIR)/xwax-import
		$(INSTALL) -D -m 0644 xwax.1 $(DESTDIR)$(MANDIR)/man1/xwax.1
		$(INSTALL) -D -m 0644 CHANGES $(DESTDIR)$(DOCDIR)/xwax/CHANGES
		$(INSTALL) -D -m 0644 COPYING $(DESTDIR)$(DOCDIR)/xwax/COPYING
		$(INSTALL) -D -m 0644 README $(DESTDIR)$(DOCDIR)/xwax/README

# Distribution archive from Git source code

.PHONY:		dist
dist:		.version
		./mkdist $(VERSION)

# Editor tags files

TAGS:		$(OBJS:.o=.c)
		etags $^

.PHONY:		clean
clean:
		rm -f xwax \
			$(OBJS) $(DEPS) \
			$(TESTS) $(TEST_OBJS) \
			mktimecode mktimecode.o \
			TAGS

-include $(DEPS)
