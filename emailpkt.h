/* ------------------------------------------------------------------------
 * emailpkt.h
 *     Structs, defines and function definitions
 * ------------------------------------------------------------------------
 *
 *  This file is part of EMAILPKT
 *
 * $Id$
 */

#if UNIX==1
#include <sys/syslimits.h>     /* for PATH_MAX */
#else
#define PATH_MAX 128
#endif

#define VERSION          "0.3"
#define MAX              64

#ifndef CFGDIR
  #define CFGDIR           "/usr/local/etc"
#endif

#ifndef DEFAULTCFGFILE
  #define DEFAULTCFGFILE   CFGDIR"/email.cfg"
#endif

/* Should we backup incoming and outgoing plain texts? I don't see why not...
   TODO: put this in the configuration */
#ifndef SAVE
  #define SAVE             1
#endif

#define strip(s)  if(s[strlen(s)-1]=='\n') s[strlen(s)-1] = 0


#define UNKNOWN          0
#define MULTIPART        1
#define APPLICATION      2

#define TEXT             3
#define BASE64           4
#define QUOTED_PRINTABLE 5
#define UUENCODE         6

/* Default path to the sendmail binary. This can be a symlink! */
#define SENDMAIL    "/usr/sbin/sendmail"

#define LOGFILENAME "emailpkt.log"

/* String to put in the body of the message if no one is found */
#define DEFAULTBODY "This message contains a Fidonet bundle."


/*
 * note for developers:
 *   I _*HATE*_ pointers. They only cause problems (seg faults) and make weird
 *   code. Thus, I have replaced them with arrays and hardcoded their
 *   sizes. If you need more, you are having problems with this or you
 *   think you will, either change the hardcoded values or write a new
 *   pointer version. It's up to you.
 *
*/

typedef struct address {
    unsigned int zone;
    unsigned int net;
    unsigned int node;
    unsigned int point;
    char domain[MAX];
} AKA;

typedef struct system {
    AKA aka;                  /* link's aka                             */
    char name[MAX];           /* link's name                            */
    char email[MAX];          /* link's email                           */
    char emailFrom[MAX];      /* our email to send from                 */
    char emailSubject[MAX];   /* email's subject                        */
    int encoding;             /* method of encoding                     */
    char filebox[PATH_MAX];   /* path to filebox                        */
} LINK;

typedef struct config {

    char name[MAX];           /* sysop's name                           */
    AKA aka;                  /* sysop's main aka                       */
    char email[MAX];          /* sysop's main email                     */

    char subject[MAX];        /* outgoing mail default subject          */

    char inbound[MAX];        /* non-secure inbound                     */
    char tempinbound[MAX];
    char outbound[MAX];
    char tempoutbound[MAX];
    char logdir[MAX];         /* where to store the logfile             */
    char sendmail[PATH_MAX];  /* command to send mail                   */

    unsigned int linkCount;
    LINK link[512];           /* is 512 enough? I think yes...          */
} CONFIG;

CONFIG cfg;

/* config.c */
void getAddress(char *s, AKA *aka);
int config(void);
void parseLink(char *s);
void trimComments(char *s);

/* emailpkt.c */
int main(int argc, char *argv[]);
void printUsage(void);

/* log.c */
int log(char *string, ...);

/* mime.c */
int fromBase64(char *name, FILE *from);
int fromText(char *name, char *boundary, FILE *in);
int fromQuoted(char *name, char *boundary, FILE *in);
int toBase64(FILE *inFile, FILE *outFile);

/* uue.c */
int fromUUE(char *name, FILE *from);
int toUUE(FILE *inFile, FILE *outFile);

/* receive.c */
int findName(char *dir, char *name);
int in(void);
void lowercase(char *s);
void normalize(char *s);
int parseMessages(char *fileName);
int parseMultipart(char *fileName);
int readBoundary(char *boundary, FILE *file);
int readWholeMsg(char *dir, char *name);
int receive(void);
int skip(char *boundary, FILE *file);

/* send.c */
int encodeAndSend(char *fullFileName, int n);
int out(void);
int processFlow(int n);
int processNetmail(int n);
int send(void);

