# Makefile with Husky support
# you will need huskymak.cfg that comes in the huskybse package.

include ../huskymak.cfg

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

%$(OBJ): %.c
	$(CC) -c $(CDEFS) $(CFLAGS) -DHUSKY $*.c

OBJFILES = \
 config$(OBJ) \
 emailpkt$(OBJ) \
 log$(OBJ) \
 mime$(OBJ) \
 receive$(OBJ) \
 send$(OBJ)

all: emailpkt$(EXE)

emailpkt: $(OBJFILES)
	$(CC) $(LFLAGS) -o emailpkt$(EXE) $(OBJFILES) $(LIBS)

install:
	$(INSTALL) $(IBOPT) emailpkt$(EXE) $(BINDIR)

uninstall:
	-$(RM) $(RMOPT) $(BINDIR)$(DIRSEP)emailpkt$(EXE)

clean:
	-$(RM) $(RMOPT) *$(OBJ)
	-$(RM) $(RMOPT) core
	-$(RM) $(RMOPT) *~

distclean: clean
	-$(RM) $(RMOPT) emailpkt$(EXE)

