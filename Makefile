# zeromerge Makefile

# PREFIX determines where files will be installed. Common examples
# include "/usr" or "/usr/local".
PREFIX = /usr/local

# Certain platforms do not support long options (command line options).
# To disable long options, uncomment the following line.
#CFLAGS += -DOMIT_GETOPT_LONG

# Uncomment this to build in hardened mode.
# This can be enabled at build time: 'make HARDEN=1'
#HARDEN=1

# PROGRAM_NAME determines the installation name and manual page name
PROGRAM_NAME = zeromerge

# BIN_DIR indicates directory where program is to be installed.
# Suggested value is "$(PREFIX)/bin"
BIN_DIR = $(PREFIX)/bin

# MAN_DIR indicates directory where the zeromerge man page is to be
# installed. Suggested value is "$(PREFIX)/man/man1"
MAN_BASE_DIR = $(PREFIX)/share/man
MAN_DIR = $(MAN_BASE_DIR)/man1
MAN_EXT = 1

# Required External Tools
CC	?= gcc
INSTALL = install
RM      = rm -f
RMDIR	= rmdir -p
MKDIR   = mkdir -p
INSTALL_PROGRAM = $(INSTALL) -m 0755
INSTALL_DATA    = $(INSTALL) -m 0644

# Make Configuration
COMPILER_OPTIONS = -Wall -Wextra -Wwrite-strings -Wcast-align -Wstrict-aliasing -Wstrict-overflow -Wstrict-prototypes -Wpointer-arith -Wundef
COMPILER_OPTIONS += -Wshadow -Wfloat-equal -Wstrict-overflow=5 -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code -Wformat=2 -Winit-self
COMPILER_OPTIONS += -std=gnu11 -O2 -g -D_FILE_OFFSET_BITS=64 -fstrict-aliasing -pipe

# Remove unused code if requested
ifdef GC_SECTIONS
COMPILER_OPTIONS += -fdata-sections -ffunction-sections
LINK_OPTIONS += -Wl,--gc-sections
endif

# Debugging code inclusion
ifdef LOUD
DEBUG=1
COMPILER_OPTIONS += -DLOUD_DEBUG
endif
ifdef DEBUG
COMPILER_OPTIONS += -DDEBUG
else
COMPILER_OPTIONS += -DNDEBUG
endif
ifdef HARDEN
COMPILER_OPTIONS += -Wformat -Wformat-security -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -fpie -Wl,-z,relro -Wl,-z,now
else
#COMPILER_OPTIONS += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
endif

# MinGW needs this for printf() conversions to work
ifeq ($(OS), Windows_NT)
ifndef NO_UNICODE
	UNICODE=1
	COMPILER_OPTIONS += -municode
	SUFFIX=.exe
endif
	COMPILER_OPTIONS += -D__USE_MINGW_ANSI_STDIO=1 -DON_WINDOWS=1
	UNAME_S=$(shell uname -s)
	ifeq ($(UNAME_S), MINGW32_NT-5.1)
		OBJS += winres_xp.o
	else
		OBJS += winres.o
	endif
endif


### Find and use nearby libjodycode by default
ifndef IGNORE_NEARBY_JC
 ifneq ("$(wildcard ../libjodycode/libjodycode.h)","")
  $(info Found and using nearby libjodycode at ../libjodycode)
  COMPILER_OPTIONS += -I../libjodycode -L../libjodycode
  ifeq ("$(wildcard ../libjodycode/version.o)","")
   $(error You must build libjodycode before building $(PROGRAM_NAME))
  endif
 endif
 ifdef FORCE_JC_DLL
  LINK_OPTIONS += -l:../libjodycode/libjodycode.dll
 else
  LINK_OPTIONS += -ljodycode
 endif
endif


CFLAGS += $(COMPILER_OPTIONS) $(CFLAGS_EXTRA)
LDFLAGS += $(LINK_OPTIONS) $(LDFLAGS_EXTRA)

OBJS += libjodycode_check.o zeromerge.o


all: libjodycode_hint $(PROGRAM_NAME) dynamic_jc

dynamic_jc: $(PROGRAM_NAME)
	$(CC) $(CFLAGS) $(OBJS) -Wl,-Bdynamic $(LDFLAGS) -o $(PROGRAM_NAME)$(SUFFIX)

static_jc: $(PROGRAM_NAME)
	$(CC) $(CFLAGS) $(OBJS) -Wl,-Bstatic $(LDFLAGS) -Wl,-Bdynamic -o $(PROGRAM_NAME)$(SUFFIX)

static: $(PROGRAM_NAME)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(PROGRAM_NAME)$(SUFFIX) -static

static_stripped: $(PROGRAM_NAME) static
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(PROGRAM_NAME)$(SUFFIX) -static
	strip $(PROGRAM_NAME)

$(PROGRAM_NAME): $(OBJS)
	:

winres.o: winres.rc winres.manifest.xml
	./tune_winres.sh
	windres winres.rc winres.o

winres_xp.o: winres_xp.rc
	./tune_winres.sh
	windres winres_xp.rc winres_xp.o

installdirs:
	test -e $(DESTDIR)$(BIN_DIR) || $(MKDIR) $(DESTDIR)$(BIN_DIR)
	test -e $(DESTDIR)$(MAN_DIR) || $(MKDIR) $(DESTDIR)$(MAN_DIR)

install: $(PROGRAM_NAME) installdirs
	$(INSTALL_PROGRAM)	$(PROGRAM_NAME)   $(DESTDIR)$(BIN_DIR)/$(PROGRAM_NAME)$(SUFFIX)
	$(INSTALL_DATA)		$(PROGRAM_NAME).1 $(DESTDIR)$(MAN_DIR)/$(PROGRAM_NAME).$(MAN_EXT)

uninstalldirs:
	-test -e $(DESTDIR)$(BIN_DIR) && $(RMDIR) $(DESTDIR)$(BIN_DIR)
	-test -e $(DESTDIR)$(MAN_DIR) && $(RMDIR) $(DESTDIR)$(MAN_DIR)

uninstall: uninstalldirs
	$(RM)	$(DESTDIR)$(BIN_DIR)/$(PROGRAM_NAME)$(SUFFIX)
	$(RM)	$(DESTDIR)$(MAN_DIR)/$(PROGRAM_NAME).$(MAN_EXT)

test: zeromerge
	./test.sh

stripped: $(PROGRAM_NAME)
	strip $(PROGRAM_NAME)$(SUFFIX)

clean:
	$(RM) $(OBJS) $(OBJS_CLEAN) $(PROGRAM_NAME)$(SUFFIX) *~ *.gcno *.gcda *.gcov

distclean: clean
	$(RM) *.pkg.tar.*
	$(RM) -r zeromerge-*-*/ zeromerge-*-*.zip

chrootpackage:
	+./chroot_build.sh
package:
	+./generate_packages.sh

libjodycode_hint:
	$(info hint: if ../libjodycode is built but jdupes won't run, try doing 'make static_jc')
