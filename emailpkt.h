/* ------------------------------------------------------------------------
 * emailpkt.h
 *     Structs, define's and function definition
 * ------------------------------------------------------------------------
 *
 *  This file is part of EMAILPKT
 *
 */

#define VERSION          "0.1"
#define MAX              64

#ifndef DEFAULTCFGFILE
  #define DEFAULTCFGFILE   "/usr/local/etc/email.cfg"
#endif

/* Should we backup incoming and outgoing plain texts? */
#ifndef SAVE
  #define SAVE             1
#endif

#define strip(s)  s[strlen(s)-1] = 0

/*
 * note for developers:
 *   I _*HATE*_ pointers. They only cause problems (seg faults) and make weird
 *   code. Thus, I have replaced them with arrays and hardcoded their
 *   subscripts. If you need more, you are having problems with this or you
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
} LINK;

typedef struct config {

    char name[MAX];           /* sysop's name                           */
    AKA aka;                  /* sysop's aka                            */
    char email[MAX];          /* sysop's email                          */

    char subject[MAX];        /* outgoing mail subject                  */

    char inbound[MAX];        /* non-secure inbound                     */
    char tempinbound[MAX];
    char outbound[MAX];
    char tempoutbound[MAX];
    char logdir[MAX];         /* where to store the logfile             */

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
int log(char *string);

/* mime.c */
int fromBase64(char *name, FILE *from);
int fromUUencode(char *name, FILE *from);
int fromText(char *name, char *boundary, FILE *in);
int fromQuoted(char *name, char *boundary, FILE *in);
int toBase64(FILE *inFile, FILE *outFile);

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

