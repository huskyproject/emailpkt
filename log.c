/* ------------------------------------------------------------------------
 * log.c
 *     Logs actions in the logfile
 * ------------------------------------------------------------------------
 *
 *  This file is part of EMAILPKT
 *
 */

#include <stdio.h>
#include <time.h>

#include "emailpkt.h"

int log(char *string)
{
    char name[MAX];
    char date[40];
    FILE *logFile;
    time_t t;

    if (cfg.logdir[0] == 0)
        return 0;

    time(&t);
    strftime(date, 40, "%a %d %b  %H:%M:%S", localtime(&t));

    sprintf(name, "%s/email.log", cfg.logdir);

        if ((logFile = fopen(name, "a")) == NULL)
            return -1;
        fprintf(logFile, " %s   %s", date, string);
        fclose(logFile);

    return 0;
}
