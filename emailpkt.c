/* --------------------------------------------------------- August 2000 --
 * emailpkt.c
 * ------------------------------------------------------------------------
 *
 *  EmailPKT is a program to send & receive a FTN bundles (echomail, netmail,
 *  tics and regular files) via e-mail. It is part of the HUSKY project.
 *
 *    Copyright (C) 2000 German Theler
 *     Fidonet: 4:905/210
 *       Email: german@linuxfreak.com
 *
 *  Get the latest version from http://husky.physcip.uni-stuttgart.de
 *
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation version 2.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "emailpkt.h"


void printUsage(void)
{
    printf("Usage: emailpkt [action]\n");
    printf("    send       send outgoing messages\n");
    printf("    receive    receive and process an email from the standard input\n");
}

int main(int argc, char *argv[])
{
    int c;

#ifdef HUSKY
    printf("EmailPKT v%s with Husky support\n", VERSION);
#else
    printf("EmailPKT v%s standalone version\n", VERSION);
#endif

    c = config();

    if (c == -1) {
        fprintf(stderr, "Config file not found!\n");
        return -1;
    } else if (c == 1) {
        fprintf(stderr, "Error parsing config file!\n");
        return 1;
    }

    if (argc != 2) {
        printUsage();
        return 0;
    }

    if (strcasecmp(argv[1], "send") == 0)
        out();
    else if (strcasecmp(argv[1], "receive") == 0)
        in();
    else {
        printUsage();
        printf("Unknown action %s\n", argv[1]);
	return 2;
    }

    return 0;
    
}

