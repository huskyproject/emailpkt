/* ------------------------------------------------------------------------
 * config.c
 *     Parses the config file
 * ------------------------------------------------------------------------
 *
 *  This file is part of EMAILPKT
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef HUSKY
  #include <fidoconf/fidoconf.h>
  #include <fidoconf/common.h>
#endif /* HUSKY */

#include "emailpkt.h"

void trim(char *s)
{
    if (s[strlen(s)-1] == '/')
        s[strlen(s)-1] = 0;
}



void getAddress(char *s, AKA *aka)
{
    char buff[MAX];
    int i, j;

    j = 0;
    
    /* get zone */
    i = 0;
    while (s[j] != ':') {
        buff[i] = s[j];
        j++;
        i++;
    }
    buff[i] = 0;
    aka->zone = atoi(buff);

    /* get net */
    i = 0;
    j++;
    while (s[j] != '/') {
        buff[i] = s[j];
        j++;
        i++;
    }
    buff[i] = 0;
    aka->net = atoi(buff);

    /* get node */
    i = 0;
    j++;
    while ((s[j] != '.') && (s[j] != '\0') && (s[j] != '@')) {
        buff[i] = s[j];
        j++;
        i++;
    }
    buff[i] = 0;
    aka->node = atoi(buff);

    /* get point */
    if (s[j] == '.') {
        i = 0;
        j++;
        while ((s[j] != '@') && (s[j] != '\0')) {
            buff[i] = s[j];
            j++;
            i++;
        }
        buff[i] = 0;
        aka->point = atoi(buff);

    }
    /* get domain */
    if (s[j] == '@') {
        i = 0;
        j++;
        while (s[j] != '\0') {
            aka->domain[i] = s[j];
            j++;
            i++;
        }
        aka->domain[i] = 0;
    }
}

void trimComments(char *s)
{
    while (*s == ' ' || *s == '\t')
        strcpy(s, s+1);
    while (s[strlen(s)-1] == ' ' || s[strlen(s)-1] == '\t')
        s[strlen(s)-1] = 0;
}


void processLink(char *s)
{
    char buff[MAX];
    char line[4*MAX];

    strncpy(line, s, 4*MAX);
    strcpy(buff, strtok(line, " \t"));

    getAddress(buff, &(cfg.link[cfg.linkCount].aka));
    strncpy(cfg.link[cfg.linkCount].email, strtok(NULL, " \t"), MAX);
    strncpy(cfg.link[cfg.linkCount].name, strtok(NULL, "#\n"), MAX);

    trimComments(cfg.link[cfg.linkCount].name);
    
    cfg.linkCount += 1;
}



int config(void)
{
    FILE *f;
    char file[MAX];
    char line[2*MAX];
    char keyword[MAX];
    char value[MAX];

#ifdef HUSKY
    s_fidoconfig *husky;
    int i;
#endif
    
    memset(&cfg, 0, sizeof(cfg));

#ifdef HUSKY
    husky = readConfig(NULL);
    strncpy(cfg.name, husky->sysop, MAX);
    cfg.aka.zone = husky->addr->zone;
    cfg.aka.net = husky->addr->net;
    cfg.aka.node = husky->addr->node;
    cfg.aka.point = husky->addr->point;
    strncpy(cfg.inbound, husky->protInbound, MAX);
    trim(cfg.inbound);
    strncpy(cfg.tempinbound, husky->tempInbound, MAX);
    trim(cfg.tempinbound);
    strncpy(cfg.outbound, husky->outbound, MAX);
    trim(cfg.outbound);
    strncpy(cfg.tempoutbound, husky->tempOutbound, MAX);
    trim(cfg.tempoutbound);
    strncpy(cfg.logdir, husky->logFileDir, MAX);
    trim(cfg.logdir);

    for (i = 0; i < husky->linkCount; i++) {
        cfg.link[i].aka.zone = husky->links[i].hisAka.zone;
        cfg.link[i].aka.net = husky->links[i].hisAka.net;
        cfg.link[i].aka.node = husky->links[i].hisAka.node;
        cfg.link[i].aka.point = husky->links[i].hisAka.point;

        if (husky->links[i].name != NULL)
            strncpy(cfg.link[i].name, husky->links[i].name, MAX);
        if (husky->links[i].email != NULL)
            strncpy(cfg.link[i].email, husky->links[i].email, MAX);
    
        cfg.linkCount++;
    }

    disposeConfig(husky);

    if (cfg.name[0] == 0)
        strncpy(cfg.name, "Joe Sysop", MAX);
    if (cfg.email[0] == 0)
        strncpy(cfg.email, "undefined@undefined", MAX);
    if (cfg.subject[0] == 0)
        strncpy(cfg.subject, "FIDONET Packet", MAX);

#endif
        
    if (getenv("EMAILPKT") != NULL)
        strncpy(file, getenv("EMAILPKT"), MAX);
    else
        strncpy(file, DEFAULTCFGFILE, MAX);

#ifdef HUSKY
    if ((f = fopen(file, "rt")) == NULL)
        return 0;
#else
    if ((f = fopen(file, "rt")) == NULL) {
         fprintf(stderr, "Cannot find %s!\n", file);
         return -1;
    }
#endif

    /* parse the fucking config file */
    while (fgets(line, 2*MAX, f) != NULL) {
        if (line[0] != '\n' && line[0] != '#') {
            strcpy(keyword, strtok(line, " \t"));
            strcpy(value, strtok(NULL, "#\n"));
            trimComments(value);

            if (strcasecmp(keyword, "name") == 0)
                strncpy(cfg.name, value, MAX);
            if (strcasecmp(keyword, "email") == 0)
                strncpy(cfg.email, value, MAX);
            if (strcasecmp(keyword, "aka") == 0)
                getAddress(value, &(cfg.aka));
            if (strcasecmp(keyword, "subject") == 0)
                strncpy(cfg.subject, value, MAX);
                
            if (strcasecmp(keyword, "inbound") == 0) {
                strncpy(cfg.inbound, value, MAX);
                trim(cfg.inbound);
            }
            if (strcasecmp(keyword, "tempinbound") == 0) {
                strncpy(cfg.tempinbound, value, MAX);
                trim(cfg.tempinbound);
            }
            if (strcasecmp(keyword, "outbound") == 0) {
                strncpy(cfg.outbound, value, MAX);
                trim(cfg.outbound);
            }
            if (strcasecmp(keyword, "tempoutbound") == 0) {
                strncpy(cfg.tempoutbound, value, MAX);
                trim(cfg.tempoutbound);
            }
            if (strcasecmp(keyword, "logdir") == 0) {
                strncpy(cfg.logdir, value, MAX);
                trim(cfg.logdir);
            }
            if (strcasecmp(keyword, "link") == 0)
                processLink(value);
        }
    }

    if (cfg.inbound[0] == 0) {
        fprintf(stderr, "INBOUND not defined.\n");
        return 1;
    }
    if (cfg.tempinbound[0] == 0) {
        fprintf(stderr, "TEMPINBOUND not defined.\n");
        return 1;
    }
    if (cfg.outbound[0] == 0) {
        fprintf(stderr, "OUTBOUND not defined.\n");
        return 1;
    }
    if (cfg.tempoutbound[0] == 0) {
        fprintf(stderr, "TEMPOUTBOUND not defined.\n");
        return 1;
    }
    if (cfg.aka.zone == 0) {
        fprintf(stderr, "Main AKA not defined.\n");
        return 1;
    }

    /* Defaults */
    if (cfg.name[0] == 0)
        strncpy(cfg.name, "Joe Sysop", MAX);
    if (cfg.email[0] == 0)
        strncpy(cfg.email, "undefined@undefined", MAX);
    if (cfg.subject[0] == 0)
        strncpy(cfg.subject, "FIDONET Packet", MAX);

    fclose(f);

    return 0;
}

