/* ------------------------------------------------------------------------
 * receive.c
 *     Process an incoming email from stdin and stores the attachments
 * ------------------------------------------------------------------------
 *
 *  This file is part of EMAILPKT
 *
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "emailpkt.h"
#include "receive.h"


int readWholeMsg(char *dir, char *name)
{
    int c;
    FILE *temp;
    char buff[4*MAX];

    /* let's start with this name. If it is already in use, find a new one */
    sprintf(name, "%04x.msg", (unsigned int)time(0));
    findName(dir, name);
    sprintf(buff, "%s/%s", dir, name);
    strcpy(name, buff);

    if ((temp = fopen(buff, "w")) == NULL)
        return 1;

    while ((c = getchar()) != EOF)
        fputc(c, temp);

    fclose(temp);

    return 0;
}


int readBoundary(char *boundary, FILE *file)
{
    char buff[4*MAX];

    do {
        fgets(buff, 255, file);

        if (feof(file))
            return -1;

        if (strncasecmp(buff, "Content-type: ", 13) == 0) {
            strncpy(buff, strchr(buff, '=')+1, 254);
            if (buff[0] == '"') {
                sprintf(boundary, "--%s", buff+1);
                boundary[strlen(boundary)-2] = 0;
            } else {
                sprintf(boundary, "--%s", buff);
                boundary[strlen(boundary)-1] = 0;
            }
        }

    } while ((strncmp(buff, boundary, (strlen(boundary) == 0)?10:strlen(boundary)) != 0)) ;

    return 0;
}


int skip(char *boundary, FILE *file)
{
    char buff[4*MAX];

    do
        fgets(buff, 255, file);
    while(strncmp(buff+2, boundary, strlen(boundary)) != 0);

    return 0;
}


void lowercase(char *s)
{
    int i=0;

    while(s[i] != 0) {
        if ((s[i] >= 'A') && (s[i] <= 'Z'))
            s[i] += 'a' - 'A';
        i++;
    }
}

int findName(char *dir, char *name)
{
    FILE *file;
    char foo[MAX];
    char bar[MAX];

    /* check if the name ends in .out, .cut or .dut and translate to .pkt */
    strcpy(bar, name);
    if ((bar[strlen(bar)-2] == 'u') && (bar[strlen(bar)-1] == 't'))
        strcpy(bar+strlen(bar)-3, "pkt");

    sprintf(foo, "%s/%s", dir, bar);
    while((file = fopen(foo, "r")) != NULL) {
        fclose(file);
        /* the file name is in use, make a new one up */
        sprintf(bar, "%04x.%s", (unsigned int)time(0), name+strlen(name)-3);
        sprintf(foo, "%s/%s", dir, bar);
    };

    strcpy(name, bar);

    return 0;
}

void normalize(char *s)
{
    int i;
    char buff[4*MAX];
    
    /* strip what is after the string */
    i = 0;
    while (s[i] != ';' && s[i] != '\n')
        i++;
    s[i] = 0;

    /* is it quoted? remove the quotes */
    if (s[0] == '"') {
        strcpy(buff, s+1);
        buff[strlen(s)-2] = 0;
        strcpy(s, buff);
    }
}


int parseMultipart(char *fileName)
{
    FILE *file;
    char buff[4*MAX];
    char boundary[4*MAX];
    char name[MAX];
    int encoding;

    file = fopen(fileName, "r");
    
    fgets(buff, 4*MAX, file);
    while(buff[0] != '\n') {
        if (strncasecmp(buff, "Content-Type: multipart", 23) == 0)
            strcpy(boundary, strstr(buff, "boundary")+strlen("boundary="));

        fgets(buff, 4*MAX, file);
    }

    normalize(boundary);

    do {
        fgets(buff, 4*MAX, file);         /* wait for the first boundary */
    } while(strncmp(buff+2, boundary, strlen(boundary)) != 0);

    /* start parsing the mime parts */
    while(buff[strlen(boundary)+2] != '-' && buff[strlen(boundary)+3] != '-') {
        name[0] = 0;
        encoding = TEXT;
        fgets(buff, 4*MAX, file);       /* now we start parsing the headers */
        while (buff[0] != '\n') {
            if (strncasecmp(buff, "Content-Type:", 13) == 0)
                if (strstr(buff, "name=") != NULL)
                    strcpy(name, strstr(buff, "name=")+strlen("name="));
            if (strncasecmp(buff, "Content-Disposition: attachment", 31) == 0)
                if (strstr(buff, "name=") != NULL)
                    strcpy(name, strstr(buff, "name")+strlen("name="));
        
            if (strncasecmp(buff, "Content-Transfer-Encoding: quoted-printable", 43) == 0)
                encoding = QUOTED_PRINTABLE;
            if (strncasecmp(buff, "Content-Transfer-Encoding: base64", 33) == 0)
                encoding = BASE64;
                
            fgets(buff, 4*MAX, file);
        }

        if (name[0] != 0) {
            normalize(name);
            lowercase(name);
            findName(cfg.inbound, name);
            sprintf(buff, "%s/%s", cfg.inbound, name);
            switch (encoding) {
                case QUOTED_PRINTABLE:
                    fromQuoted(buff, boundary, file);
                break;
                case BASE64:
                    fromBase64(buff, file);
                break;
                case TEXT:
                    fromText(buff, boundary, file);
                break;
            }
            sprintf(buff, "Received %s\n", name);
            log(buff);
            
            do {
                fgets(buff, 4*MAX, file);
            } while(strncmp(buff+2, boundary, strlen(boundary)) != 0);
            
        } else
            skip(boundary, file);
    }

    fclose(file);

    return 0;
}

int parseUUencode(char *fileName)
{
    FILE *file;
    char buff[4*MAX];
    char name[MAX];
    int perms;

    file = fopen(fileName, "r");

    while (!feof(file)) {
        name[0] = 0;
        do {
            fgets(buff, 4*MAX, file);
            if (feof(file))
                return 0;
        } while (strncasecmp(buff, "begin ", 5) != 0);

        sscanf(buff, "begin %o %s", &perms, name);
        
        if (name[0] != 0) {
            normalize(name);
            lowercase(name);
            findName(cfg.inbound, name);
            sprintf(buff, "%s/%s", cfg.inbound, name);
            fromUUE(buff, file);
            sprintf(buff, "Received %s\n", name);
            log(buff);
        }
    }

    fclose(file);

    return 0;
}


int parseApplication(char *fileName)
{
    FILE *file;
    char buff[4*MAX];
    char name[MAX];
    int encoding;

    file = fopen(fileName, "r");

    name[0] = 0;
    encoding = TEXT;

    fgets(buff, 4*MAX, file);
    while(buff[0] != '\n') {
    
        if (strncasecmp(buff, "Content-Type:", 13) == 0)
            if (strstr(buff, "name=") != NULL)
                strcpy(name, strstr(buff, "name=")+strlen("name="));
        if (strncasecmp(buff, "Content-Disposition: attachment", 31) == 0)
            if (strstr(buff, "name=") != NULL)
                strcpy(name, strstr(buff, "name")+strlen("name="));
        
        if (strncasecmp(buff, "Content-Transfer-Encoding: quoted-printable", 43) == 0)
            encoding = QUOTED_PRINTABLE;
        if (strncasecmp(buff, "Content-Transfer-Encoding: base64", 33) == 0)
            encoding = BASE64;

        fgets(buff, 4*MAX, file);
    }

    if (name[0] != 0) {
        normalize(name);
        lowercase(name);
        findName(cfg.inbound, name);
        sprintf(buff, "%s/%s", cfg.inbound, name);
        switch (encoding) {
            case BASE64:
                fromBase64(buff, file);
            break;
        }
        sprintf(buff, "Received %s\n", name);
        log(buff);
    }

    fclose(file);

    return 0;
}


int parseMessage(char *fileName)
{
    FILE *file;
    char buff[4*MAX];

    file = fopen(fileName, "r");
    
    while(!feof(file)) {
        fgets(buff, 4*MAX, file);
        
        if (strncasecmp(buff, "Content-Type: multipart", 23) == 0) {
            fclose(file);
            return MULTIPART;
        }
        if (strncasecmp(buff, "Content-Type: application", 25) == 0) {
            fclose(file);
            return APPLICATION;
        }
        if (strncasecmp(buff, "begin ", 5) == 0) {
            fclose(file);
            return UUENCODE;
        }
    }
    fclose(file);

    return UNKNOWN;
}
            

int receive(void)
{
    int c;
    char fileName[MAX];

    if (readWholeMsg(cfg.tempinbound, fileName) != 0) {
        fprintf(stderr, "[!] Severe error. Can't write to tempoutbound.\n");
        return 1;
    }

    c = parseMessage(fileName);
    if (c == MULTIPART)
        parseMultipart(fileName);
    else if (c == APPLICATION)
        parseApplication(fileName);
    else if (c == UUENCODE)
        parseUUencode(fileName);
    else {
        fprintf(stderr, "[!] Unsuported encoding.\n");
        log("[!] Unsuported encoding.\n");
    }

    if (!SAVE)
        remove(fileName);

    return 0;
}

int in(void)
{
    int error;

    error = receive();

    switch (error) {
        case 0:
            break;
        case 1:
            log("[!] Error!\n");
        break;
    }

    return error;
}

