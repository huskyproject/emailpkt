/* Emailpkt: Husky e-mailer
 *
 * (C) Copyleft Stas Degteff 2:5080/102@FIDOnet, g@grumbler.org
 * (c) Copyleft Husky developers team http://husky.sorceforge.net/team.html
 *
 * $Id$
 * Common declarations
 *
 * For latest version see http://husky.sorceforge.net/emailpkt
 *
 * This file is part of EMAILPKT, module of The HUSKY Fidonet Software.
 *
 * EMAILPKT is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * EMAILPKT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; see file COPYING. If not, write to the Free
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * See also http://www.gnu.org, license may be found here.
 *****************************************************************************
 */

#ifndef __ECOMMON_H
#define __ECOMMON_H

/***** Default configuretion *****/

#ifndef LOGFILE
# define LOGFILE "emailpkt.log"
#endif

#define DEFAULT_FROMNAME "EmailPkt"
#define DEFAULT_FPOMADDR "unknown@fidonet.org"
#define DEFAULT_SUBJECT  "FTNbundle"

/* String to put in the body of the message if no one is found */
#ifndef DEFAULTBODY
#define DEFAULTBODY "This message contains a FTN bundle.\nDecode attachment and save file into FTN inbound directory."
/* "(Latest version of emailpkt available at http://husky.physcip.uni-stuttgart.de)\n" */
#endif
/* MIME booundary string */
#ifndef MIMEBOUNDARY
#define MIMEBOUNDARY   "emailpkt_boundary"
#endif

/* Sub-directory names in temporary inbound, outbound & etc */
#define TempSubDir     "emailpkt"
#define TEMPEXT "eml"
#define SECTIONS_SUBDIR "sections"
#define TempInSubDir   "hereceive"
#define TempOutSubDir  "hesend"
#define UnknownFileExt "unk" /* suffix for received files without names */

/* First chars of SEAT special lines */
#define SEAT_HEADER_SIGNATURE "Ftn-"

#define AMODE    0644    /* mode (see chmod(1)) for create file */
#define TempMODE 0600    /* mode (see chmod(1)) for create temp file */
#define TempDIRMODE 0700 /* mode (see chmod(1)) for create temp directory */

/**** Do not change after this line *****************************/
#include <fidoconf/dirlayer.h>
#include <fidoconf/fidoconf.h>
#include <fidoconf/common.h>
#ifdef UNIX
#  include <sys/syslimits.h>
#else
#  include <limits.h>
#endif
#if defined (_MSC_VER) || defined (__CYGWIN__) || defined (__MINGW32__) || defined (_WIN32)
#  include <stdint.h>
#else
#  include <inttypes.h>
#endif

#ifndef DIRSEP
# if (OSTYPE==UNIX)  && !defined (__MINGW32__)
#  define DIRSEP '/'
# else
#  define DIRSEP '\\'
# endif
#endif

#ifndef PATH_MAX
# define PATH_MAX 80
#endif
#ifndef NAME_MAX
# define NAME_MAX 13
#endif

#define ADRS_MAX 31  /* Max. length of FTN address string: "zzzz:nnnnn/nnnnn.ppppp@dddddddd" */

/* Types */

/* for processMIMESection() */
enum contents{ con_Unknown, con_TextPlain, con_TextHtml, con_Text, con_File };
enum encodings{ en_Unknown, en_None, en_UUE, en_BASE64, en_QUOTED };

struct s_msgHeader{
	char *from_, *From, *To, *Subject, *Date, *XMailer, *MessageID;
	char *Organization, *MimeVersion, *ContentType;
	char *ContentTransferEncoding, *ContentDisposition;
	char *XConfirmTo;     /* LuckyGate's confirmation address */
	char *fromaddr;       /* sender's email */
        char *boundary;       /* MIME boundary string */
        char *endboundary;    /* MIME boundary string: message end */
	char **other;         /* other (unknown) lines */
	unsigned othercount;  /* other lines count */
        unsigned short isbad; /* Bad message (duplicate kludges)*/
        char *XFTNattachVersion;  /* X-FTNattach-Version: 1.0 */
        unsigned char f_multipart_mixed;      /* MIME multipart/mixed */
        unsigned char f_multipart_alternative;/* MIME multipart/alternative */
        unsigned char f_LuckyGate; /* Flag: detected LuckyGate */
        unsigned char f_IREX;      /* Flag: detected Internet REX */
        unsigned char f_SEAT;      /* Flag: detected SEAT (RFC-0015) message */
};

struct s_SEAT_header{
        char *FileID;          /* Ftn-File-ID:   */
        uint32_t Crc32;          /* Ftn-Crc32:     */
        unsigned short SegNo, SegCount; /* Ftn-Seg:       */
        uint32_t SegCrc32;       /* Ftn-Seg-Crc32: */
        char *SegID;           /* Ftn-Seg-ID:    */
};

/* misc.c vars */
extern char *program_name;

/* misc.c funcs */

#ifndef sstr
/* safety string: not return NULL */
#define sstr(sss) (sss ? sss : "(-null-)")
#endif

void  dispose_SEAT_header(struct s_SEAT_header);
int   truncat(const char *path);
int   delete(const char *path);
char *snprintaddr(char *string, const int size,const s_addr addr);
int   setOutboundFilenames(s_link *link, e_flavour prio);
FILE *createbsy(s_link link);
int   removebsy(s_link link);
int   testfile(const char *filename);
char *stripCRLF(char *s);
char *version();

/* Create new file (exclusive sharing mode) with access mode 'AMODE'
 * in file mode 'fmode'
 * Return values:
 * OK:     file descriptor
 * error:  NULL
 */
FILE *createFileExcl(char *filename, const char *fmode);


/* Concatenate files
 * Parameters:
 *   path to files (directory name), slash ('/' or '\\') at end is mandatory!
 *   array of directory entries (NULL-terminated)
 *   output filename.
 * Return 0 if success
 */
int concat_files( const char *path, struct dirent **ls, const char *dstfilename);

/* Move file into inbound, if file exist - change name:
 * if *.pkt then change before point char else increment last chars
 * Return 0 if success
 */
int move_file_to_inbound( const char *from,  const char *to );

/* Generate next inbound file name
 * if *.pkt then change before point char else increment last chars
 * Return new file name or NULL
 */
char *next_inbound_filename(const char *filename);

/* encdec.c funcs ==============================================*/
/* decoders *****************************************************/

/* Decode uu-string and store into dstbuffer
 * Return decoded array len
 */
unsigned uudecodeLine( unsigned char **dstbuffer, const unsigned char *line );

/* Decode base64-encoded string (store pointer into dstbuffer)
 * Return length of decoded data or -1 (if error)
 */
int base64DecodeLine( unsigned char **dstbuffer,
                           const unsigned char * line);

/* Decode quoted-printeble line (store into dstbuffer)
 * Return length of decoded data or -1 (if error)
 */
int quotedPrintableDecodeLine( unsigned char **dstbuffer,
                                    const unsigned char *line);

/* uudecode file
 * Return values:
 * OK:               0
 * Can't open file: -1
 */
int uudecodeFile(char *name, FILE *from);

/* Decode base64-encoded file
 * Return values:
 * OK:                    0
 * Can't create file:    -1  display critical error
 * Can't reopen file:    -2  display critical error
 * Unexpected EOF:        1  display error
 * Can't read from file: -3  display error
 */
int base64decodeFile(char *name, FILE *in);


/* encoders *****************************************************/

/* base64-encoder for file
 * Return values:
 * OK: 0
 * error: errno
 */
int base64encodeFile(FILE *infd, FILE *outfd);

/* uu-encoder for file
 * Return values:
 * OK: 0
 * error: errno
 */
int uuencodeFile( FILE *infd, FILE *outfd, const char*filename,
              const unsigned section, const unsigned sectsize);


/* Case-incensitive string compare, return values eq. strncmp()
 */
//int strincmp(const char *str1, const char *str2, unsigned len);


/* Create new file (into inbound directory), if file exist increment:
 * - base part of filename - tic or pkt,
 * - suffix ("extension") for other.
 */
FILE *createInboundFile(char **pathname);


/*
 * Return pointer to base ('clean') filename in pathname
 */
const char *basename(const char *pathname);

/* main vars ****************************************************/

extern s_fidoconfig *config;


#define dirsort(ls) {w_log(LL_ALERT,"dirsort() not implemented yet, sorry");}
#define RemoveDirectoryTree(tp) {w_log(LL_ALERT,"RemoveDirectoryTree() not implemented yet, sorry");}

#endif
