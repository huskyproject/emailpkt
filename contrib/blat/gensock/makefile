# $Id$
# Makefile for MS Visual C: build gensock library

all: winsock pwksock

release: clean
	nmake -f gensock.mak DEBUG=0
	nmake -f genasync.mak DEBUG=0
	nmake -f pwksock.mak DEBUG=0

winsock:
	nmake -f gensock.mak
	copy gensock.dll ..\gwinsock.dll

pwksock:
	nmake -f pwksock.mak
	copy pwksock.dll ..\

gensck32:
	nmake -f gensck32.mak

# how many junks files can there be ???

clean:
	-del *.obj
	-del *.lib
	-del *.exp
	-del *.res
	-del *.dll
	-del *.bsc
	-del *.map
	-del *.sbr
	-del *.bsc
	-del *.pdb

zip:
	-del \gensock.zip
	zip -r \gensock.zip .
