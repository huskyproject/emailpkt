/* ------------------------------------------------------------------------
 * send.c
 *     Process flowfiles for each link and sends netmails
 * ------------------------------------------------------------------------
 *
 *  This file is part of EMAILPKT
 *
 */

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include "send.h"
#include "emailpkt.h"

int sent = 0;           /* count how many files we have sent */

int encodeAndSend(char *fullFileName, int n)
{
    char buff[4*MAX];
    char shortFileName[MAX];
    char random[10];

    FILE *input;
    FILE *output;

    /* make a pseudo-random filename to store the temp file */
    sprintf(random, "%04x", (unsigned int)time(0));

    /* search for an available name in the temp dir*/
    findName(cfg.tempoutbound, random);
    sprintf(buff, "%s/%s", cfg.tempoutbound, random);

    /* get the fileName */
    strcpy(shortFileName, strrchr(fullFileName, '/')+1);

    /* is it a netmail pkt? change its name */
    if (shortFileName[10] == 'u' && shortFileName[11] == 't')
        sprintf(shortFileName, "%s.pkt", random);


    if ((output = fopen(buff, "wt")) == NULL) {
        sprintf(buff, "[!] Can't write to %s/%s\n", cfg.tempoutbound, random);
        log(buff);
        return 1;
    }

    /* print some headers */
    fprintf(output, "From: \"%s\" <%s>\n", cfg.name, cfg.email);
    fprintf(output, "X-Mailer: EmailPKT v%s\n", VERSION);
    fprintf(output, "To: %s\n", cfg.link[n].email);
    fprintf(output, "Subject: %s\n", cfg.subject);

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
        sprintf(buff, "[!] Can't open %s\n", fullFileName);
        log(buff);
        fclose(output);
        return 2;
    }
    toBase64(input, output);
    fclose(input);

    fprintf(output, "\n---emailpktboundary--");

    fclose(output);

    /* now we send the email */
    sprintf(buff, "%s %s < %s/%s", SENDMAIL, cfg.link[n].email, cfg.tempoutbound, random);
    if (system(buff) != 0) {
        log("[!] Error while calling sendmail!\n");
        log(strcat(buff, "\n"));
        return -1;
    }

    /* we should keep this file until we receive a confirmation that the
       file was received OK -> TODO! */
    sprintf(buff, "%s/%s", cfg.tempoutbound, random);
    if (!SAVE)
        remove(buff);

    return 0;

}

int processFlow(int n)
{
    FILE *flowFile;
    char flowName[MAX];
    char pktName[MAX];
    char buff[4*MAX];

    int delete = 0;
    int truncate = 0;

    if (cfg.link[n].aka.point != 0)
        if (cfg.link[n].aka.zone == cfg.aka.zone)
            sprintf(flowName, "%s/%04x%04x.pnt/%08x.flo", cfg.outbound,
                                                        cfg.link[n].aka.net,
                                                        cfg.link[n].aka.node,
                                                        cfg.link[n].aka.point);
        else
            sprintf(flowName, "%s.%03x/%04x%04x.pnt/%08x.flo", cfg.outbound,
                                                        cfg.link[n].aka.zone,
                                                        cfg.link[n].aka.net,
                                                        cfg.link[n].aka.node,
                                                        cfg.link[n].aka.point);
    else
        if (cfg.link[n].aka.zone == cfg.aka.zone)
            sprintf(flowName, "%s/%04x%04x.flo", cfg.outbound,
                                                 cfg.link[n].aka.net,
                                                 cfg.link[n].aka.node);
        else
            sprintf(flowName, "%s.%03x/%04x%04x.flo", cfg.outbound,
                                                      cfg.link[n].aka.zone,
                                                      cfg.link[n].aka.net,
                                                      cfg.link[n].aka.node);

    if ((flowFile = fopen(flowName, "rb")) == NULL) {
        flowName[strlen(flowName)-3] = 'd';     /* else try direct routing */
        if ((flowFile = fopen(flowName, "rb")) == NULL)
            flowName[strlen(flowName)-3] = 'c'; /* try crash finally */
            if ((flowFile = fopen(flowName, "rb")) == NULL)
                return 0;                     /* no flow file for this link */
    }

    while (fgets(buff, MAX, flowFile) != NULL) {
        if (buff[0] == '^') {
            delete = 1;
            strcpy(pktName, buff+1);
        } else if (buff[0] == '#') {
            truncate = 1;
            strcpy(pktName, buff+1);
        } else
            strcpy(pktName, buff);

        strip(pktName);   /* remove the final \n */

        if (encodeAndSend(pktName, n) == 0) {
            if (cfg.link[n].aka.point != 0) {
                sprintf(buff, "Sent %s to %s (%d:%d/%d.%d)\n", pktName,
                                                       cfg.link[n].email,
                                                       cfg.link[n].aka.zone,
                                                       cfg.link[n].aka.net,
                                                       cfg.link[n].aka.node,
                                                       cfg.link[n].aka.point);
                log(buff);
                printf("%s", buff);
                sent++;
            } else {
                sprintf(buff, "Sent %s to %s (%d:%d/%d)\n", pktName,
                                                       cfg.link[n].email,
                                                       cfg.link[n].aka.zone,
                                                       cfg.link[n].aka.net,
                                                       cfg.link[n].aka.node);
                log(buff);
                printf("%s", buff);
                sent++;
            }
        }
    }

    fclose(flowFile);
    remove(flowName);

    if (delete)
        remove(pktName);

    if (truncate) {
        flowFile = fopen(pktName, "wb");
        fclose(flowFile);
    }

    return 0;
}

int processNetmail(int n)
{
    FILE *pktFile;
    char buff[4*MAX];
    char fullPath[2*MAX];
    char flavourSuffix[MAX];

    /* only route crash netmail, this can change in the future */
    strcpy(flavourSuffix, "cut");

    if (cfg.link[n].aka.point != 0)
        if (cfg.link[n].aka.zone == cfg.aka.zone)
            sprintf(fullPath, "%s/%04x%04x.pnt/%08x.%s", cfg.outbound,
                                                 cfg.link[n].aka.net,
                                                 cfg.link[n].aka.node,
                                                 cfg.link[n].aka.point,
                                                 flavourSuffix);
        else
           sprintf(fullPath, "%s.%03x/%04x%04x.pnt/%08x.%s", cfg.outbound,
                                                 cfg.link[n].aka.zone,
                                                 cfg.link[n].aka.net,
                                                 cfg.link[n].aka.node,
                                                 cfg.link[n].aka.point,
                                                 flavourSuffix);
    else
        if (cfg.link[n].aka.zone == cfg.aka.zone)
            sprintf(fullPath, "%s/%04x%04x.%s", cfg.outbound,
                                                 cfg.link[n].aka.net,
                                                 cfg.link[n].aka.node,
                                                 flavourSuffix);
        else
           sprintf(fullPath, "%s.%03x/%04x%04x.%s", cfg.outbound,
                                                 cfg.link[n].aka.zone,
                                                 cfg.link[n].aka.net,
                                                 cfg.link[n].aka.node,
                                                 flavourSuffix);

    if ((pktFile = fopen(fullPath, "r")) != NULL) {
        fclose(pktFile);

        if (encodeAndSend(fullPath, n) == 0)
            if (cfg.link[n].aka.point != 0) {
                sprintf(buff, "Sent netmail to %s (%d:%d/%d.%d)\n", cfg.link[n].email,
                                                      cfg.link[n].aka.zone,
                                                      cfg.link[n].aka.net,
                                                      cfg.link[n].aka.node,
                                                      cfg.link[n].aka.point);
                log(buff);
                printf("%s", buff);
                sent++;
            } else {
                sprintf(buff, "Sent netmail to %s (%d:%d/%d)\n", cfg.link[n].email,
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
    int i;
    
    for (i = 0; i < cfg.linkCount; i++)
        if (cfg.link[i].email[0] != 0) {
            if (processFlow(i) == 1)
                return 1;
            if (processNetmail(i) == 1)
                return 2;
        }

    return 0;
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

