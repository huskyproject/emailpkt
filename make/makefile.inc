# $Id$
# include-file for other makefiles
#
# This file is part of the Emailpkt package,
# part of the husky fidosoft project
#

OBJS	= encdec$(OBJ) misc$(OBJ)
DOCS	+= README README.TXT
HFILES	= cvsdate.h ecommon.h version.h

commonall: commonhesend commonhereceive

commonhesend: $(OBJS) hesend$(OBJ)
	$(CC) $(LFLAGS) -o hesend$(EXE) $(OBJS) hesend$(OBJ) $(LIBS)

commonhereceive:  $(OBJS) hereceive$(OBJ)
	$(CC) $(LFLAGS) -o hereceive$(EXE) $(OBJS) hereceive$(OBJ) $(LIBS)

commonclean:
	-$(RM) $(RMOPT) *$(OBJ)

commondistclean: commonclean
	-$(RM) $(RMOPT) hesend$(EXE)
	-$(RM) $(RMOPT) hereceive$(EXE)
