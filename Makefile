# Emailpkt make file
#
# Makefile with Husky support
# you will need huskymak.cfg that comes in the huskybse package.
#
# $Id$
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

ifeq ($(DEBUG), 1)
  CFLAGS = $(WARNFLAGS) $(DEBCFLAGS) -I$(INCDIR)
  LFLAGS = $(DEBLFLAGS)
else
  CFLAGS = $(WARNFLAGS) $(OPTCFLAGS) -I$(INCDIR)
  LFLGAS = $(OPTLFLAGS)
endif

ifeq ($(SHORTNAME), 1)
  LIBS   = -L$(LIBDIR) -lsmapi -lfidoconf
else
  LIBS   = -L$(LIBDIR) -lsmapi -lfidoconfig
endif


CDEFS=-DNOEXCEPTIONS -D$(OSTYPE) $(ADDCDEFS)

HFILES = emailpkt.h

%$(OBJ): %.c $(HFILES)
	$(CC) -c $(CDEFS) $(CFLAGS) -DHUSKY $*.c

OBJFILES = \
 config$(OBJ) \
 emailpkt$(OBJ) \
 log$(OBJ) \
 mime$(OBJ) \
 receive$(OBJ) \
 send$(OBJ) \
 uue$(OBJ)


all: emailpkt$(EXE)

emailpkt: $(OBJFILES)
	$(CC) $(LFLAGS) -o emailpkt$(EXE) $(OBJFILES) $(LIBS)

install:
	$(INSTALL) $(IBOPT) emailpkt$(EXE) $(BINDIR)
	$(MKDIR) $(MKDIROPT) $(HTMLDIR)$(DIRSEP)emailpkt
	$(INSTALL) $(IMOPT) README $(HTMLDIR)$(DIRSEP)emailpkt

uninstall:
	-$(RM) $(RMOPT) $(BINDIR)$(DIRSEP)emailpkt$(EXE)
	-$(RM) $(RMOPT) $(HTMLDIR)$(DIRSEP)emailpkt
	-$(RMDIR) $(RMOPT) $(HTMLDIR)

clean:
	-$(RM) $(RMOPT) *$(OBJ)
	-$(RM) $(RMOPT) core
	-$(RM) $(RMOPT) *~

distclean: clean
	-$(RM) $(RMOPT) emailpkt$(EXE)

