/* ------------------------------------------------------------------------
 * log.c
 *     Logs actions in the logfile
 * ------------------------------------------------------------------------
 *
 *  This file is part of EMAILPKT
 *
 * $Id$
 */

#include <stdio.h>
#include <time.h>
#include <stdarg.h>

#include "emailpkt.h"

int log(char *string, ...)
{
    char name[MAX];
    char date[40];
    FILE *logFile;
    time_t t;
    va_list ar;

    if (cfg.logdir[0] == 0)
        return 0;

    time(&t);
    strftime(date, 40, "%a %d %b  %H:%M:%S", localtime(&t));

    sprintf(name, "%s/" LOGFILENAME, cfg.logdir);

    if ((logFile = fopen(name, "a")) == NULL){
      fprintf( stderr, "!!! Cannot open log file %s\n", name);
      return -1;
    }
    fprintf(logFile, " %s   ", date );
    va_start(ar, string);
    vfprintf(logFile, string, ar);
    va_end(ar);
    fclose(logFile);

    return 0;
}

