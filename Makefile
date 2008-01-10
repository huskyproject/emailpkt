# $Id$
#
# Emailpkt Makefile with Husky build enviroment support.
# You will need huskymak.cfg that comes in the huskybse package.
#

ifeq ($(DEBIAN), 1)
  # Every Debian-Source-Paket has one included.
  include debian/huskymak.cfg
else
  include ../huskymak.cfg
endif


ifndef RMDIR
  RMDIR	= rm -d
endif

SRC_DIR = src$(DIRSEP)
H_DIR   = h$(DIRSEP)
MANSRCDIR= man$(DIRSEP)
DOCSUBDIR= emailpkt

CFLAGS	= $(WARNFLAGS) -I$(INCDIR) -I../ -I$(H_DIR)
ifeq ($(DEBUG), 1)
  CFLAGS += $(DEBCFLAGS)
  LFLAGS = $(DEBLFLAGS)
else
  CFLAGS += $(OPTCFLAGS)
  LFLAGS = $(OPTLFLAGS)
endif

# local libraries
#LIBS	= -L../smapi -L../fidoconf -L$(LIBDIR) -lsmapi
# Libraries installed into /usr/local/lib (or other system library dir)
LIBS	= -L$(LIBDIR) -lsmapi

ifeq ($(SHORTNAME), 1)
  LIBS	+= -lfidoconf
else
  LIBS	+= -lfidoconfig
endif

#CDEFS=-DNOEXCEPTIONS -D$(OSTYPE) -DDIRSEP=\'$(DIRSEP)\' $(ADDCDEFS)
CDEFS=-D$(OSTYPE) -DDIRSEP=\'$(DIRSEP)\' $(ADDCDEFS)

include make/makefile.inc


%$(OBJ): $(SRC_DIR)%.c
	$(CC) -c $(CDEFS) $(CFLAGS) $(SRC_DIR)$*.c

emailpkt:  all

all:       commonall

hesend:    commonhesend

hereceive: commonhereceive

install: emailpkt
	$(INSTALL) $(IBOPT) hesend$(EXE) $(BINDIR)
	$(INSTALL) $(IBOPT) hereceive$(EXE) $(BINDIR)
	-$(MKDIR) $(MKDIROPT) $(HTMLDIR)$(DIRSEP)$(DOCSUBDIR)
	$(INSTALL) $(IMOPT) $(DOCS) $(HTMLDIR)$(DIRSEP)$(DOCSUBDIR)
	-$(MKDIR) $(MKDIROPT) $(MANDIR)/man1
	cd $(MANSRCDIR) ; $(INSTALL) $(IMOPT) $(MANPAGES) $(MANDIR)$(DIRSEP)man1

uninstall:
	-$(RM) $(RMOPT) $(BINDIR)$(DIRSEP)hesend$(EXE)
	-$(RM) $(RMOPT) $(BINDIR)$(DIRSEP)hereceive$(EXE)
	-$(RM) $(RMOPT) $(HTMLDIR)$(DIRSEP)emailpkt$(DIRSEP)*.*
	-$(RMDIR) $(RMOPT) $(HTMLDIR)$(DIRSEP)emailpkt

clean:  commonclean
	-$(RM) $(RMOPT) core
	-$(RM) $(RMOPT) *.core

distclean: commondistclean
