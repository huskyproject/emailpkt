# Makefile with Husky support
# you will need huskymak.cfg that comes in the huskybse package.

include ../huskymak.cfg

ifeq ($(DEBUG), 1)
  CFLAGS = $(WARNFLAGS) $(DEBCFLAGS)
  LFLAGS = $(DEBLFLAGS)
else
  CFLAGS = $(WARNFLAGS) $(OPTCFLAGS)
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

clean:
	-$(RM) *$(OBJ)
	-$(RM) core
	-$(RM) *~

distclean: clean
	-$(RM) emailpkt$(EXE)

