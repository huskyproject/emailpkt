/* ------------------------------------------------------------------------
 * send.c
 *     Process flowfiles for each link and sends netmails
 * ------------------------------------------------------------------------
 *
 *  This file is part of EMAILPKT
 *
 * $Id$
 */

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>

#include "emailpkt.h"

int sent = 0;           /* count how many files we have sent */

#define BUFF_SIZE PATH_MAX+MAX
int encodeAndSend(char *fullFileName, int n)
{
    char buff[BUFF_SIZE];
    char shortFileName[MAX];
    char random[MAX];
    int result;

    FILE *input;
    FILE *output;

    /* make a pseudo-random filename to store the temp file */
    snprintf(random, MAX, "%08x", (unsigned int)time(0));

    /* search for an available name in the temp dir*/
    findName(cfg.tempoutbound, random);
    snprintf(buff, BUFF_SIZE, "%s/%s", cfg.tempoutbound, random);

    if ((output = fopen(buff, "wt")) == NULL) {
        snprintf(buff, BUFF_SIZE, "[!] Can't write to %s/%s\n", cfg.tempoutbound, random);
        log(buff);
        return 1;
    }

    /* get the fileName */
    strncpy(shortFileName, strrchr(fullFileName, '/')+1, MAX);

    /* is it a netmail pkt? if so, change its name */
    if (shortFileName[10] == 'u' && shortFileName[11] == 't')
        snprintf(shortFileName, MAX, "%04x.pkt", (unsigned int)time(0));


    /* print some headers */
    fprintf(output, "From: \"%s\" <%s>\n", cfg.name, (cfg.link[n].emailFrom[0] != 0) ? cfg.link[n].emailFrom : cfg.email);
    fprintf(output, "To: %s\n", cfg.link[n].email);
    fprintf(output, "Subject: %s\n", (cfg.link[n].emailSubject[0] != 0) ? cfg.link[n].emailSubject : cfg.subject);
    fprintf(output, "X-Mailer: EmailPKT v%s\n", VERSION);

    if (cfg.link[n].encoding == BASE64) {
        fprintf(output, "Mime-Version: 1.0\n");
        fprintf(output, "Content-Type: multipart/mixed; boundary=\"-emailpktboundary\"\n\n");
        fprintf(output, "This MIME encoded message is a FTN packet created by\n");
        fprintf(output, "EMAILPKT, available at http://husky.physcip.uni-stuttgart.de\n");
        fprintf(output, "Decode it with EMAILPKT or any MUA supporting MIME.\n");

        fprintf(output, "\n---emailpktboundary\n\n");

        /* now the body text */
        /* TODO: come up with a better idea to do this */
        fprintf(output, DEFAULTBODY);

        fprintf(output, "\n\n---emailpktboundary\n");

        /* and finally the encoded file */
        fprintf(output, "Content-Type: application/octet-stream; name=\"%s\"\n", shortFileName);
        fprintf(output, "Content-Transfer-Encoding: base64\n");
        fprintf(output, "Content-Disposition: attachment; filename=\"%s\"\n\n", shortFileName);

        if ((input = fopen(fullFileName, "r")) == NULL) {
            snprintf(buff, BUFF_SIZE, "[!] Can't open %s\n", fullFileName);
            log(buff);
            fclose(output);
            return 2;
        }
        toBase64(input, output);
        fprintf(output, "\n---emailpktboundary--");
        fclose(input);

    } else if (cfg.link[n].encoding == UUENCODE) {

        /* now the body text */
        /* TODO: come up with a better idea to do this */
        fprintf(output, "\n\n");
        fprintf(output, DEFAULTBODY);

        /* should the mode be taken seriously? */
        fprintf(output, "\n\nbegin 664 %s\n", shortFileName);

        if ((input = fopen(fullFileName, "r")) == NULL) {
            snprintf(buff, BUFF_SIZE, "[!] Can't open %s\n", fullFileName);
            log(buff);
            fclose(output);
            return 2;
        }
        toUUE(input, output);
        fprintf(output, "end\n");
        fclose(input);

    }

    fclose(output);


    /* now we send the email */
    snprintf(buff, BUFF_SIZE, "%s %s < %s/%s", cfg.sendmail, cfg.link[n].email, cfg.tempoutbound, random);
    if( (result = system(buff)) != 0) {
        log("[!] Sendmail calling error!\n");
        log(strncat(buff, "\n", BUFF_SIZE));
        fprintf(stderr, "[!] Sendmail calling error (rc=%d)!\n", result);
        fprintf(stderr, "%s\n", buff);
        snprintf(buff, BUFF_SIZE, "Resut code: %d\n", result);
        log(buff);
        return -1;
    }

    /* we should keep this file until we receive a confirmation that the
       file was received OK -> TODO! */
    if (!SAVE) {
        snprintf(buff, BUFF_SIZE, "%s/%s", cfg.tempoutbound, random);
        remove(buff);
    }

    return 0;

}

int processfilebox(int linkno){
    DIR * boxP;
    struct dirent * dp;
    char buff[PATH_MAX];
    unsigned pathlen=0;
    int rc;

    pathlen = strlen(cfg.link[linkno].filebox);
    if ( pathlen==0 ) return 0;

    strncpy(buff,cfg.link[linkno].filebox,PATH_MAX);
    if ( (boxP = opendir(cfg.link[linkno].filebox))!=NULL ){
      while( (dp = readdir(boxP)) != NULL ){
	if( dp->d_type != DT_REG) continue; /* Send only regular files! */
	strncpy(buff+pathlen,dp->d_name,PATH_MAX-pathlen);
        if (encodeAndSend(buff, linkno) == 0) {
          sent++;
                log( "Sent %s to %s (%d:%d/%d.%d)\n",
                               buff,
                               cfg.link[linkno].email,
                               cfg.link[linkno].aka.zone,
                               cfg.link[linkno].aka.net,
                               cfg.link[linkno].aka.node,
                               cfg.link[linkno].aka.point);
                printf( "Sent %s to %s (%d:%d/%d.%d)\n",
                               buff,
                               cfg.link[linkno].email,
                               cfg.link[linkno].aka.zone,
                               cfg.link[linkno].aka.net,
                               cfg.link[linkno].aka.node,
                               cfg.link[linkno].aka.point);
          if( (rc=remove(buff)) ){
            log("File %s not removed! unlink() error code: %d\n", buff, rc);
            printf("File %s not removed! unlink() error code: %d\n", buff, rc);
          }
        }else{
          log("Don't send: %s\n", buff);
          printf("Don't send: %s\n", buff);
        }
      }
      (void)closedir(boxP);
    }else return 1;
    return 0;
}	

int processFlow(int n)
{
    FILE *flowFile,*temp;
    char flowName[MAX];
    char bsyName[MAX];
    char pktName[MAX];
    char buff[BUFF_SIZE];

    int delete = 0;
    int truncate = 0;
    int lenvar = 0;

    if (cfg.link[n].aka.point != 0)
        if (cfg.link[n].aka.zone == cfg.aka.zone){
            snprintf(flowName, MAX, "%s/%04x%04x.pnt/%08x.flo", cfg.outbound,
                                                        cfg.link[n].aka.net,
                                                        cfg.link[n].aka.node,
                                                        cfg.link[n].aka.point);
           snprintf(bsyName, MAX, "%s/%04x%04x.pnt/%08x.bsy", cfg.outbound,
                                                        cfg.link[n].aka.net,
                                                        cfg.link[n].aka.node,
                                                        cfg.link[n].aka.point);
        }else{
            snprintf(flowName, MAX, "%s.%03x/%04x%04x.pnt/%08x.flo", cfg.outbound,
                                                        cfg.link[n].aka.zone,
                                                        cfg.link[n].aka.net,
                                                        cfg.link[n].aka.node,
                                                        cfg.link[n].aka.point);
           snprintf(bsyName, MAX, "%s.%03x/%04x%04x.pnt/%08x.bsy", cfg.outbound,
                                                        cfg.link[n].aka.zone,
                                                        cfg.link[n].aka.net,
                                                        cfg.link[n].aka.node,
                                                        cfg.link[n].aka.point);
        }
    else
        if (cfg.link[n].aka.zone == cfg.aka.zone){
            snprintf(flowName, MAX, "%s/%04x%04x.flo", cfg.outbound,
                                                 cfg.link[n].aka.net,
                                                 cfg.link[n].aka.node);
            snprintf(bsyName, MAX, "%s/%04x%04x.bsy", cfg.outbound,
                                                 cfg.link[n].aka.net,
                                                 cfg.link[n].aka.node);
        }else{
            snprintf(flowName, MAX, "%s.%03x/%04x%04x.flo", cfg.outbound,
                                                      cfg.link[n].aka.zone,
                                                      cfg.link[n].aka.net,
                                                      cfg.link[n].aka.node);
            snprintf(bsyName, MAX, "%s.%03x/%04x%04x.bsy", cfg.outbound,
                                                      cfg.link[n].aka.zone,
                                                      cfg.link[n].aka.net,
                                                      cfg.link[n].aka.node);
        }
    /* if no flowfile found */
    if ((flowFile = fopen(flowName, "rb")) == NULL) {
        lenvar=strlen(flowName)-3;
        if( lenvar<0 )
          return 0;  /* strange flow file name */
        flowName[lenvar] = 'd';     /* try direct routing */
        if ((flowFile = fopen(flowName, "rb")) == NULL) {
          flowName[lenvar] = 'c'; /* try crash finally */
          if ((flowFile = fopen(flowName, "rb")) == NULL)
                return 0;                     /* no flow file for this link */
        }
    }

    if( (temp = fopen(bsyName,"w"))==NULL ){
	log("%s exist, skip link", bsyName);
        return 2;
    }
    fclose(temp);
    log("Process %s\n",flowName);
    while (fgets(buff, MAX, flowFile) != NULL) {

        if (buff[0] == '^') {
            delete = 1;
            strncpy(pktName, buff+1, MAX);
        } else if (buff[0] == '#') {
            truncate = 1;
            strncpy(pktName, buff+1, MAX);
        } else
            strncpy(pktName, buff, MAX);

        strip(pktName);   /* remove the trailing \n */
        if(strlen(pktName)==0) continue;

        if (encodeAndSend(pktName, n) == 0) {
            if (cfg.link[n].aka.point != 0) {
                snprintf(buff, BUFF_SIZE, "Sent %s to %s (%d:%d/%d.%d)\n",
                               pktName,
                               cfg.link[n].email,
                               cfg.link[n].aka.zone,
                               cfg.link[n].aka.net,
                               cfg.link[n].aka.node,
                               cfg.link[n].aka.point);
                log(buff);
                printf("279: %s", buff);
                sent++;
            } else {
                snprintf(buff, BUFF_SIZE, "Sent %s to %s (%d:%d/%d)\n",
                                                       pktName,
                                                       cfg.link[n].email,
                                                       cfg.link[n].aka.zone,
                                                       cfg.link[n].aka.net,
                                                       cfg.link[n].aka.node);
                log("288: %s",buff);
                printf(buff);
                sent++;
            }
            if (delete)
                remove(pktName);

            if (truncate) {
                if( (temp = fopen(pktName, "wb")) )
                   fclose(temp);
            }
        } else
            return 1;
    }
    log("Remove files\n");

    fclose(flowFile);
    remove(flowName);
    temp = fopen(bsyName, "wb");
    fclose(temp);

    return 0;
}

int processNetmail(int n)
{
    FILE *pktFile;
    char buff[BUFF_SIZE];
    char fullPath[PATH_MAX];
    char flavourSuffix[MAX];

    log("processNetmail\n");

    /* only route crash netmail, this can change in the future */
    strncpy(flavourSuffix, "cut", MAX);

    if (cfg.link[n].aka.point != 0)
        if (cfg.link[n].aka.zone == cfg.aka.zone)
            snprintf(fullPath, PATH_MAX, "%s/%04x%04x.pnt/%08x.%s",
                                                 cfg.outbound,
                                                 cfg.link[n].aka.net,
                                                 cfg.link[n].aka.node,
                                                 cfg.link[n].aka.point,
                                                 flavourSuffix);
        else
           snprintf(fullPath, PATH_MAX, "%s.%03x/%04x%04x.pnt/%08x.%s",
                                                 cfg.outbound,
                                                 cfg.link[n].aka.zone,
                                                 cfg.link[n].aka.net,
                                                 cfg.link[n].aka.node,
                                                 cfg.link[n].aka.point,
                                                 flavourSuffix);
    else
        if (cfg.link[n].aka.zone == cfg.aka.zone)
            snprintf(fullPath, PATH_MAX, "%s/%04x%04x.%s", cfg.outbound,
                                                 cfg.link[n].aka.net,
                                                 cfg.link[n].aka.node,
                                                 flavourSuffix);
        else
           snprintf(fullPath, PATH_MAX, "%s.%03x/%04x%04x.%s", cfg.outbound,
                                                 cfg.link[n].aka.zone,
                                                 cfg.link[n].aka.net,
                                                 cfg.link[n].aka.node,
                                                 flavourSuffix);

    if ((pktFile = fopen(fullPath, "r")) != NULL) {
        fclose(pktFile);

        if (encodeAndSend(fullPath, n) == 0)
            if (cfg.link[n].aka.point != 0) {
                snprintf(buff, BUFF_SIZE, "Sent netmail to %s (%d:%d/%d.%d)\n",
                                                      cfg.link[n].email,
                                                      cfg.link[n].aka.zone,
                                                      cfg.link[n].aka.net,
                                                      cfg.link[n].aka.node,
                                                      cfg.link[n].aka.point);
                log(buff);
                printf("%s", buff);
                sent++;
            } else {
                snprintf(buff, BUFF_SIZE, "Sent netmail to %s (%d:%d/%d)\n",
                                                      cfg.link[n].email,
                                                      cfg.link[n].aka.zone,
                                                      cfg.link[n].aka.net,
                                                      cfg.link[n].aka.node);
                log(buff);
                printf("%s", buff);
                sent++;
            }
        else
            return 1;

        remove(fullPath);
    }

    return 0;
}

int send(void)
{
    int i,rc=0;

    for (i = 0; i < cfg.linkCount; i++){
        if (cfg.link[i].email[0] != 0) {
            rc=processFlow(i)  +
               processNetmail(i) +
               processfilebox(i);
        }
    }
    return rc;
}

int out(void)
{
    int error;

    error = send();

    switch (error) {
        case 0:
            if (sent == 0)
                printf("Nothing to send!\n");
            break;
        case 1:
            fprintf(stderr, "Error processing a flowfile. See logs for details.\n");
            log("[!] Error processing echomail.\n");
        break;
        case 2:
            fprintf(stderr, "Error processing netmail. See logs for details.\n");
            log("[!] Error processing netmail.\n");
        break;

    }

    return error;

}

