OBJFILES = \
 config$(OBJ) \
 emailpkt$(OBJ) \
 log$(OBJ) \
 mime$(OBJ) \
 receive$(OBJ) \
 send$(OBJ) \
 uue$(OBJ)

commonall: commonprog

commonprog: $(OBJFILES)
	$(CC) -o emailpkt$(EXE) $(OBJFILES)

commonclean:
	-$(RM) *$(OBJ)

commondistclean: clean
	-$(RM) emailpkt$(EXE)
