#mapmytr.fcgi: parseMmtReq.c parseMmtReq.h
#	gcc -Wall -lfcgi -o mapmytr.fcgi parseMmtReq.c parseMmtReq.h

release:
	gcc -Wall -lfcgi -O2 -s -o atrack.fcgi atrack.c

debug:
	gcc -DDBGLOG -Wall -lfcgi -o atrack.fcgi atrack.c
