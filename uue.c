/* ------------------------------------------------------------------------
 * uue.c
 *     UUE encoding/decoding algorithms
 * ------------------------------------------------------------------------
 *
 *  This file is part of EMAILPKT
 *
 * $Id$
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "emailpkt.h"

#define DEC(n) (n - ' ') & 077
#define ENC(n) (n != 0) ? (((n) & 077) + ' ') : '`'

int toUUE(FILE *inFile, FILE *outFile)
{
    char buff[256];
    int i;
    int j = 0;
    int a, b, c, d;

    do {
        memset(buff, 0, 255);

        /* are there less than 45 bytes left? */
        for (i = 0; i < 45; i++) {
            j = i;
            if ((c = fgetc(inFile)) == EOF)
                 break;
            else
                buff[i] = (char)c;
        }

        /* write hoy many bytes are on the actual line */
        fputc(ENC(i), outFile);

        /* and the encoded data itself */
        for (i = 0; i < j; i += 3) {
            a = buff[i] >> 2;
            b = ((buff[i] << 4) & 060) | ((buff[i+1] >> 4) & 017);
            c = ((buff[i+1] << 2) & 074) | ((buff[i+2] >> 6) & 03);
            d = buff[i+2] & 077;

            fputc(ENC(a), outFile);
            fputc(ENC(b), outFile);
            fputc(ENC(c), outFile);
            fputc(ENC(d), outFile);
        }

        fprintf(outFile, "\n");
    } while (i != 0);

    return 0;
}

int fromUUE(char *name, FILE *from)
{
    FILE *to;
    char buff[256];
    int i;
    int a, b, c, d;

    if ((to = fopen(name, "wb")) == NULL)
        return -1;

    while ((i = DEC(fgetc(from))) != 0) {
        while (i > 0) {
            a = DEC(fgetc(from));
            b = DEC(fgetc(from));
            c = DEC(fgetc(from));
            d = DEC(fgetc(from));

            if (i-- > 0)
                fputc((char)(a<<2)|(b>>4), to);
            if (i-- > 0)
                fputc((char)(b<<4)|(c>>2), to);
            if (i-- > 0)
                fputc((char)(c<<6)|(d), to);
        }
        fgets(buff, 255, from);
    }
    fclose(to);

    return 0;
}

