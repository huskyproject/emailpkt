/* ------------------------------------------------------------------------
 * mime.c
 *     MIME encoding/decoding algorithms
 * ------------------------------------------------------------------------
 *
 *  This file is part of EMAILPKT
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "emailpkt.h"

char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";


int fromBase64(char *name, FILE *from)
{
    FILE *to;
    char buffer[255];
    int i;
    int a, b, c, d;
    int x, y, z;

    if ((to = fopen(name, "wb")) == NULL)
        return -1;

    while ((i = getc(from)) != '\n') {
        ungetc(i, from);
        fgets(buffer, 254, from);

        for (i = 0; (buffer[i] != '\n') && (buffer[i] != '\0'); i += 4) {
            a = strchr(base64, buffer[i]) - base64;
            b = strchr(base64, buffer[i+1]) - base64;
            c = strchr(base64, buffer[i+2]) - base64;
            d = strchr(base64, buffer[i+3]) - base64;

            x = (((a << 6) | b) >> 4) & 0xFF;
            y = (((((a << 6) | b) << 6) | c) >> 2) & 0xFF;
            z = ((((((a << 6) | b) << 6) | c) << 6) | d) & 0xFF;

            fputc(x, to);

            if (c != 64)
                fputc(y, to);
	    else {
	        fclose(to);
		return 0;
	    }

            if (d != 64)
                fputc(z, to);
	    else {
		fclose(to);
		return 0;
	    }
        }
    }
    fclose(to);
    
    return 0;
}


int fromText(char *name, char *boundary, FILE *in)
{
    FILE *out;
    char buff[4*MAX];

    out = fopen(name, "w");

    fgets(buff, 255, in);
    do {
        fputs(buff, out);
        fgets(buff, 255, in);
    } while (strncmp(buff+2, boundary, strlen(boundary)) != 0);
    fseek(in, -strlen(buff), SEEK_CUR);
    fclose(out);

    return 0;
}

int fromQuoted(char *name, char *boundary, FILE *in)
{
    FILE *out;
    char buff[4*MAX];
    char hex[MAX];
    int i;

    out = fopen(name, "w");

    fgets(buff, 255, in);
    do {
        i = 0;
        while (buff[i] != 0)
            if (buff[i] == '=')
                if (buff[i+1] != '\n') {
                    hex[0] = buff[i+1];
                    hex[1] = buff[i+2];
                    hex[2] = 0;
                    fputc((char)strtol(hex, (char **)NULL, 16), out);
                    i += 3;
                } else
                    i += 2;
            else if (buff[i] == '\n') {
                fputc(0x0D, out);
                fputc(0x0A, out);
                i += 1;
            }  else
                fputc(buff[i++], out);

        fgets(buff, 255, in);

    } while (strncmp(buff+2, boundary, strlen(boundary)) != 0);
    fseek(in, -strlen(buff), SEEK_CUR);
    /* we wrote a trailing \n that is part of the boundary, not the file */
    fseek(out, -2, SEEK_CUR);
    fputc(0, out);
    fputc(0, out);
    
    fclose(out);

    return 0;
}

int toBase64(FILE *inFile, FILE *outFile)
{
    int a, b, c, d;
    int x, y, z;
    int counter = 0;

    while ((x = getc(inFile)) != EOF) {
        if (counter >= 64) {
            fprintf(outFile, "\n");
            counter = 0;
        } else
            counter += 4;
            
        if ((y = getc(inFile)) == EOF) {
            a = x >> 2;
            b = x << 4 & 0x3F;
            putc(base64[a], outFile);
            putc(base64[b], outFile);
            putc('=', outFile);
            putc('=', outFile);

            fprintf(outFile, "\n");

            return 0;
        }

        if ((z = getc(inFile)) == EOF) {
            a = x >> 2;
            b = (x << 4 | y >> 4) & 0x3F;
            c = y << 2 & 0x3F;

            putc(base64[a], outFile);
            putc(base64[b], outFile);
            putc(base64[c], outFile);
            putc('=', outFile);

            fprintf(outFile, "\n");
            
            return 0;
        }

        a = x >> 2;
        b = (x << 4 | y >> 4) & 0x3F;
        c = (y << 2 | z >> 6) & 0x3F;
        d = z & 0x3F;

        putc(base64[a], outFile);
        putc(base64[b], outFile);
        putc(base64[c], outFile);
        putc(base64[d], outFile);
    }

    fprintf(outFile, "\n");
    return 0;
    
}



