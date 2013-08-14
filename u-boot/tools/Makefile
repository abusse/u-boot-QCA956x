#
# (C) Copyright 2000-2003
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
#
# See file CREDITS for list of people who contributed to this
# project.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA 02111-1307 USA
#

BINS	= mkimage$(SFX) envcrc$(SFX)
OBJS	= environment.o mkimage.o crc32.o envcrc.o

#-------------------------------------------------------------------------

HOSTARCH := $(shell uname -m | \
	sed -e s/i.86/i386/ \
	    -e s/sun4u/sparc64/ \
	    -e s/arm.*/arm/ \
	    -e s/sa110/arm/ \
	    -e s/powerpc/ppc/ \
	    -e s/Power\ Macintosh/ppc/ \
	    -e s/macppc/ppc/)

HOSTOS := $(shell uname -s | tr A-Z a-z | \
	sed -e 's/\(cygwin\).*/cygwin/')

TOOLSUBDIRS =

#
# Mac OS X / Darwin's C preprocessor is Apple specific.  It
# generates numerous errors and warnings.  We want to bypass it
# and use GNU C's cpp.  To do this we pass the -traditional-cpp
# option to the compiler.  Note that the -traditional-cpp flag
# DOES NOT have the same semantics as GNU C's flag, all it does
# is invoke the GNU preprocessor in stock ANSI/ISO C fashion.
#
# Apple's linker is similar, thanks to the new 2 stage linking
# multiple symbol definitions are treated as errors, hence the
# -multiply_defined suppress option to turn off this error.
#
ifeq ($(HOSTOS)-$(HOSTARCH),darwin-ppc)
HOST_CFLAGS = -traditional-cpp -Wall
HOST_LDFLAGS =-multiply_defined suppress
HOST_ENVIRO_CFLAGS = -traditional-cpp

else
ifeq ($(HOSTOS)-$(HOSTARCH),netbsd-ppc)
HOST_CFLAGS = -Wall -pedantic
HOST_LDFLAGS =
HOST_ENVIRO_CFLAGS =

#
# Everyone else
#
else
HOST_CFLAGS = -Wall -pedantic
HOST_LDFLAGS =
HOST_ENVIRO_CFLAGS =
endif
endif

#
# Cygwin needs .exe files :-(
#
ifeq ($(HOSTOS),cygwin)
SFX = .exe
else
SFX =
endif

#
# Include this after HOSTOS HOSTARCH check
# so that we can act intelligently.
#
include $(TOPDIR)/config.mk

#
# Use native tools and options
#
CPPFLAGS	= -idirafter ../include -DTEXT_BASE=$(TEXT_BASE) -DUSE_HOSTCC
CFLAGS		= $(HOST_CFLAGS) $(CPPFLAGS) -O2
AFLAGS		= -D__ASSEMBLY__ $(CPPFLAGS)
CC			= $(HOSTCC)
STRIP		= $(HOSTSTRIP)
MAKEDEPEND	= makedepend

all:	.depend $(BINS) subdirs

envcrc$(SFX):	envcrc.o crc32.o environment.o
		$(CC) $(CFLAGS) -o $@ $^

mkimage$(SFX):	mkimage.o crc32.o
		$(CC) $(CFLAGS) $(HOST_LDFLAGS) -o $@ $^
		$(STRIP) $@

envcrc.o:	envcrc.c
		$(CC) -g $(CFLAGS) -c $<

crc32.o:	crc32.c
		$(CC) -g $(CFLAGS) -c $<

mkimage.o:	mkimage.c
		$(CC) -g $(CFLAGS) -c $<

subdirs:
ifeq ($(TOOLSUBDIRS),)
		@:
else
		@for dir in $(TOOLSUBDIRS) ; do \
		    $(MAKE) \
			HOSTOS=$(HOSTOS) \
			HOSTARCH=$(HOSTARCH) \
			HOST_CFLAGS="$(HOST_CFLAGS)" \
		   	HOST_LDFLAGS="$(HOST_LDFLAGS)" \
			-C $$dir || exit 1 ; \
		done
endif

environment.c:
		@rm -f environment.c
		ln -s ../common/environment.c environment.c

environment.o: environment.c
		$(CC) -g $(HOST_ENVIRO_CFLAGS) $(CPPFLAGS) -c $<

crc32.c:
		@rm -f crc32.c
		ln -s ../lib_generic/crc32.c crc32.c

#########################################################################

.depend:	Makefile $(OBJS:.o=.c)
		$(CC) -M $(HOST_CFLAGS) $(CPPFLAGS) $(OBJS:.o=.c) > $@

sinclude .depend

#########################################################################