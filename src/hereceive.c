/* EMAILPKT: Husky e-mailer
 *
 * (C) Copyleft Stas Degteff 2:5080/102@FIDOnet, g@grumbler.org
 * (c) Copyleft Husky developers team http://husky.sorceforge.net/team.html
 *
 * $Id$
 * Receive FTN files from email
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

/* libc */
/*#include <math.h> */  /*log()*/
#include <string.h>
#include <stdio.h>  /*FILE operations*/
#include <unistd.h> /*mktemp()*/
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <dirent.h>
#ifdef UNIX
#  include <sys/syslimits.h>
#else
#  include <limits.h>
#endif
#include <ctype.h>
#include <fcntl.h>

/* husky */
#include <fidoconf/log.h>
#include <fidoconf/temp.h>
#include <fidoconf/xstr.h>
#include <fidoconf/crc.h>
#include <fidoconf/common.h>
#include <smapi/patmat.h>
/*#include <smapi/strextra.h>*/
#include <smapi/progprot.h>


/* ./ */
#include "ecommon.h"

#define BUF_SIZE 65530      /* for DOS compatibility */

/* processUUE inteernal codes (negative!) */
#define processUUE_end    -1   /* end line detected */
#define processUUE_sum    -2   /* sum line detected */
#define processUUE_eof    -3   /* EOF */
#define processUUE_ierr   -4   /* input error (fread() error) */
#define processUUE_oerr   -5   /* output error (fwrite() error) */
#define processUUE_uue    -6   /* uue line decoding error */
#define processUUE_err    -10  /* other error */

s_fidoconfig *config=NULL;
struct s_msgHeader msgHeader;
struct s_SEAT_header SEAThdr;
char buf[BUF_SIZE];     /* input buffer (one line from RFC822 message)*/
int debugflag=0; /* Execute in debug mode (not call extern programs,
                    not remove files, save processed e-mail messages */

#define disposeSEATHeader() { if(SEAThdr.FileID) nfree(SEAThdr.FileID); \
                              if(SEAThdr.SegID) nfree(SEAThdr.SegID); \
                              memset(&SEAThdr,0,sizeof(SEAThdr)); }

/* Return not 0 if str is SEAT header line */
#define isSEATheader(str) ( !sstrnicmp(str,"SEAT_HEADER_SIGNATURE", sizeof(SEAT_HEADER_SIGNATURE)-1) )

/* This file functions (declarations) */
FILE *saveToTemp(FILE *fd);
void dispose_msgHeader();
void storeHdrLine(const char *line);
unsigned getMsgHeader(FILE *fd);
int getSEATHeader(FILE *fd);
int storeSEATHeaderLine( const char *line );
char *processMIMESection( const char *inbound, FILE *fd, const s_link *link );
char *processBASE64( const char *inbound, FILE *fd, const char *filename );
char *processUUE( const char *inbound, FILE *ifd, const char *filename );
enum contents getContentType(const char *buf);
char *get_Filename_From_ContentDisposition(const char *buf);
static enum encodings getContentTransferEncoding(const char *buf);
int processMime( const char *inbound, FILE *fd, const s_link *link );
int processSEAT( const char *inbound, FILE *fd, const s_link *link );
char *processTextPlain( const char *inbound, FILE *fd, const s_link *link );
char *processTextOrUUE( const char *inbound, FILE *fd, const s_link *link );
int processAttUUCP( const char *inbound, FILE *fd, const s_link *link );
int process( const char *inbound, FILE *fd, const s_link *link );
int processEml( FILE *fd, const s_link *link );
int receive(FILE *fd);
void printver();
void usage();



/*
 * Copy file to temp file, return temp file descriptor
 */
FILE *saveToTemp(FILE *fd)
{ FILE *tempfd=NULL; char *tempfilename=NULL;

  w_log( LL_FUNC, "saveToTemp()" );
  if( (tempfd=createTempFileIn( config->tempInbound, TEMPEXT, 't', &tempfilename)) )
  {
      while( fwrite(buf, 1, fread( buf, 1, sizeof(buf), fd), tempfd )>0 )
      {
        if( feof(fd) )
          break;
        if( ferror(fd) )
        { fclose (tempfd);
          w_log( LL_ERR, "savemsg():%d Can't read message: \"%s\"", __LINE__, strerror(errno) );
          return NULL;
        }
        if( ferror(tempfd) )
        { fclose (tempfd);
          w_log( LL_ERR, "savemsg():%d Can't save message to temp file: \"%s\"", __LINE__, strerror(errno) );
          w_log( LL_FUNC, "saveToTemp() failed" );
          return NULL;
        }
      }
      w_log( LL_FILE, "Message saved to temp inbound: %d bytes", ftell(tempfd) );

      rewind(tempfd);
  }
  else
  {   w_log( LL_CRIT, "savemsg(): Can't create temp file" );
      return NULL;
  }

  w_log( LL_FUNC, "saveToTemp() OK" );
  return tempfd;
}

/*
 * De-allocate memory & clear msgHeader
 */
void dispose_msgHeader()
{
  w_log( LL_FUNC, "dispose_msgHeader()" );

  nfree(msgHeader.from_);
  nfree(msgHeader.From);
  nfree(msgHeader.To);
  nfree(msgHeader.Subject);
  nfree(msgHeader.Date);
  nfree(msgHeader.MessageID);
  nfree(msgHeader.Organization);
  nfree(msgHeader.MimeVersion);
  nfree(msgHeader.ContentType);
  nfree(msgHeader.ContentTransferEncoding);
  nfree(msgHeader.XConfirmTo);
  nfree(msgHeader.boundary);
  nfree(msgHeader.endboundary);
  nfree(msgHeader.fromaddr);
  nfree(msgHeader.XMailer);
  nfree(msgHeader.XFTNattachVersion);
  if( msgHeader.othercount )
  {
    do{
  w_log( LL_DEBUGX, __FILE__ ":%u:", __LINE__);
      --msgHeader.othercount;
      nfree(msgHeader.other[msgHeader.othercount]);
    }while( msgHeader.othercount>0 );
  }
  nfree(msgHeader.other);
  memset(&msgHeader, 0, sizeof(msgHeader)); /* Clean structure */

  w_log( LL_FUNC, "dispose_msgHeader() OK" );
}


/*
 * Check & store header line to msgHeader structure.
 */
void storeHdrLine(const char *line)
{ int len=0; char *cp=NULL;

  w_log( LL_FUNC, "storeHdrLine()" );

  w_log( LL_DEBUG, "Process '%s'", line );

  len=sstrlen(line);
  if( !len )
  {  w_log( LL_ERR, "storeHdrLine(): Empty line" );
     w_log( LL_FUNC, "storeHdrLine() OK" );
     return;
  }

  if( line[0]==' ' || line[0]=='\t'  /* continues from previous skip */
      || !sstrnicmp(line, "Received:", 9 ) ); /* 'Received:' skip */

  else if( !sstrnicmp(line, "From ", 5) )
  {  if(msgHeader.from_)
     {  w_log(LL_WARN,"Duplicate \"From \" line in header! Message is bad.");
        msgHeader.isbad=1;
     }
     else
       msgHeader.from_ = sstrdup(line+5); /*first line, envelope_from address*/
     w_log( LL_DEBUG, "msgHeader.from_='%s'", msgHeader.from_ );
  }

  else if( !sstrnicmp(line, "From:", 5) )
  {  if(msgHeader.From)
     {  w_log(LL_WARN,"Duplicate \"From: \" line in header! Message is bad.");
        msgHeader.isbad=1;
     }
     else
        msgHeader.From = stripRoundingChars(sstrdup(line+5), " \t");
     w_log( LL_DEBUG, "msgHeader.From='%s'", msgHeader.From );
     len = sstrlen(msgHeader.From)-1;
     if( msgHeader.From[len]=='>' )
     {  /* extract email address from "From:" kluge value */
        msgHeader.fromaddr = strrchr(msgHeader.From,'<') + 1;
        msgHeader.From[len]='\0';
        msgHeader.fromaddr = sstrdup(msgHeader.fromaddr);
        msgHeader.From[len]='>';
     }
     else
       msgHeader.fromaddr = sstrdup(msgHeader.From);
     w_log( LL_DEBUG, "msgHeader.fromaddr='%s'", msgHeader.fromaddr );
  }

  else if( !sstrnicmp(line, "To:", 3) )
  {  msgHeader.To = stripRoundingChars(sstrdup(line+3), " \t");
     w_log( LL_DEBUG, "msgHeader.To='%s'", msgHeader.To );
  }

  else if( !sstrnicmp(line, "Date:", 5) )
  {  msgHeader.Date = stripRoundingChars(sstrdup(line+5), " \t");
     w_log( LL_DEBUG, "msgHeader.Date='%s'", msgHeader.Date );
  }

  else if( !sstrnicmp(line, "Subject:", 8) )
  {  msgHeader.Subject = stripRoundingChars(sstrdup(line+8), " \t");
     w_log( LL_DEBUG, "msgHeader.Subject='%s'", msgHeader.Subject );
  }

  else if( !sstrnicmp(line, "X-Mailer:", 9) )
  {  msgHeader.XMailer = stripRoundingChars(sstrdup(line+9), " \t");
     w_log( LL_DEBUG, "msgHeader.XMailer='%s'", msgHeader.XMailer );
     if( sstrnicmp( msgHeader.XMailer, "Internet Rex", 52 ) )
     { msgHeader.f_IREX = 1;
       w_log( LL_INFO, "Internet Rex detected" );
     }
  }

  else if( !sstrnicmp(line, "Message-ID:", 11) )
  {  msgHeader.MessageID = stripRoundingChars(sstrdup(line+11), " \t");
     w_log( LL_DEBUG, "msgHeader.MessageID='%s'", msgHeader.MessageID );
  }

  else if( !sstrnicmp(line, "Organization:", 13) )
  {  msgHeader.Organization = stripRoundingChars(sstrdup(line+13), " \t");
     w_log( LL_DEBUG, "msgHeader.Organization='%s'", msgHeader.Organization );
  }

  else if( !sstrnicmp(line, "Mime-Version:", 13) )
  {  msgHeader.MimeVersion = stripRoundingChars(sstrdup(line+13), " \t");
     w_log( LL_DEBUG, "msgHeader.MimeVersion='%s'", msgHeader.MimeVersion );
  }

  /* X-FTNattach-Version: 1.0 (LuckyGate 6.02) */
  else if( !sstrnicmp(line, "X-FTNattach-Version:", 20) )
  { cp = strchr(line, '(');
    if( cp )
    { *cp='\0';
      msgHeader.XFTNattachVersion = stripRoundingChars(sstrdup(line+20), " \t");
      w_log( LL_DEBUG, "msgHeader.XFTNattachVersion='%s'", msgHeader.XFTNattachVersion );
      if( fc_stristr( ++cp, "LuckyGate") )
        w_log(LL_INFO, "LuckyGate message detected");
        msgHeader.f_LuckyGate=1;
    }
  }

  else if( !sstrnicmp(line, "X-Confirm-To:", 13) )
  {  msgHeader.XConfirmTo = stripRoundingChars(sstrdup(line+13), " \t");
     w_log( LL_DEBUG, "msgHeader.XConfirmTo='%s'", msgHeader.XConfirmTo );
  }

  else if( !sstrnicmp(line, "Content-Type:", 13) )
  {  msgHeader.ContentType = stripRoundingChars(sstrdup(line+13), " \t");
     w_log( LL_DEBUG, "msgHeader.ContentType='%s'", msgHeader.ContentType );

     if( !sstrncmp(msgHeader.ContentType, "multipart/mixed", 15) )
       msgHeader.f_multipart_mixed=1;
     else if( !sstrncmp(msgHeader.ContentType, "multipart/alternative", 21) )
       msgHeader.f_multipart_alternative=1;
     if( msgHeader.f_multipart_alternative || msgHeader.f_multipart_alternative
         || !sstrncmp(msgHeader.ContentType, "multipart", 9) )
     {  cp=strstr(line,"boundary=");
        if( cp )
        {
          cp = stripRoundingChars(sstrdup(cp+9), "\" \t");
          xscatprintf(&msgHeader.boundary, "--%s", cp);
          xscatprintf(&msgHeader.endboundary, "--%s--", cp);
          nfree(cp);
          if( (cp = strchr(msgHeader.ContentType,';')) )
          { *cp=0;
            stripLeadingChars(msgHeader.ContentType, "\" \t");
            realloc(msgHeader.ContentType, sstrlen(msgHeader.ContentType)+1);
          }
        }
        else
        {  msgHeader.isbad=1;
         /*msgHeader.boundary = NULL;*/
           w_log( LL_WARN, "Error in message header: miltipart without boundary" );
           w_log( LL_WARN, "Error line: \'%s\'", line );
        }
     }
     if(msgHeader.boundary)
       w_log( LL_DEBUG, "msgHeader.boundary='%s'", msgHeader.boundary );
     if(msgHeader.endboundary)
       w_log( LL_DEBUG, "msgHeader.endboundary='%s'", msgHeader.endboundary );
  }

  else if( !sstrnicmp( line, "Content-Transfer-Encoding:", 26 ) )
     msgHeader.ContentTransferEncoding = stripRoundingChars( sstrdup(line+26), " \t" );

  else if( !sstrnicmp( line, "Content-Disposition:", 20 ) )
     msgHeader.ContentDisposition = stripRoundingChars( sstrdup(line+20), " \t" );

  else
  { /* Add new item to array msgHeader.other */
    msgHeader.othercount++;
    if( msgHeader.other )
    {  srealloc(msgHeader.other,(msgHeader.othercount)*sizeof(msgHeader.other));
       msgHeader.other[msgHeader.othercount-1]=NULL;
    }else
      msgHeader.other = scalloc((msgHeader.othercount),sizeof(msgHeader.other));
    msgHeader.other[msgHeader.othercount-1] = sstrdup(line);
  }
  w_log( LL_FUNC, "storeHdrLine() OK" );
}

/*
 * Get header from message, store known fields for use
 */
unsigned getMsgHeader(FILE *fd)
{
  w_log( LL_FUNC, "getMsgHeader()" );
  dispose_msgHeader();
  do{

    buf[0]=0;
    fgets(buf, sizeof(buf), fd);
    if( feof(fd) || ferror(fd) )
    { w_log( LL_ERR, "Can't read message header: %s", feof(fd)? "EOF" : strerror(errno) );
      w_log( LL_FUNC, "getMsgHeader() rc=1" );
      return 1;
    }
    stripCRLF(buf);
    if( buf && buf[0] )
      storeHdrLine(buf);
    else
      break;

  }while( 1 );   /* End of RFC822 header: empty line*/

  /* skip to not empty line */
  do{
    buf[0]=0;
    fgets(buf, sizeof(buf), fd);
    if( feof(fd) || ferror(fd) )
    { w_log( LL_ERR, "Can't read message header: %s", feof(fd)? "EOF" : strerror(errno) );
      w_log( LL_FUNC, "getMsgHeader() rc=1" );
      return 1;
    }
    stripCRLF(buf);
  }while( buf[0]==0 );

  if(isSEATheader(buf))
  { getSEATHeader(fd);
    msgHeader.f_SEAT = 1;
  }

  w_log( LL_FUNC, "getMsgHeader() OK (rc=0)" );
  return 0;
}


/* buf can be 1st SEAT line ('Ftn-*')
 *
 */
int getSEATHeader(FILE *fd)
{
  w_log( LL_FUNC, "getSEATHeader()" );

  disposeSEATHeader();

  while(storeSEATHeaderLine(buf))
  {
    buf[0]=0;
    fgets(buf, sizeof(buf), fd);
    if( feof(fd) || ferror(fd) )
    { w_log( LL_ERR, "Can't read SEAT header: %s", feof(fd)? "EOF" : strerror(errno) );
      w_log( LL_FUNC, "getSEATHeader() rc=1" );
      return 1;
    }
    stripCRLF(buf);
  }

  w_log( LL_FUNC, "getSEATHeader() OK (rc=0)" );
  return 0;
}


int storeSEATHeaderLine(const char *buf)
{ register int rc=0;
  char *cp;

  w_log( LL_FUNC, "storeSEATHeaderLine() begin" );

    if( !sstrnicmp( buf, "Ftn-File-ID: ", 13 ) )
    {  SEAThdr.FileID = stripLeadingChars(sstrdup(buf + 13), " \t");
       w_log(LL_DEBUG, "SEAThdr.FileID='%s'", SEAThdr.FileID);
    }
    else if( !sstrnicmp( buf, "Ftn-Crc32: ", 11 ) )
    {  SEAThdr.Crc32 = (uint32_t)strtoul(buf + 11, NULL, 16);
       w_log(LL_DEBUG, "SEAThdr.Crc32='%x'", (unsigned long)SEAThdr.Crc32);
    }
    else if( !sstrnicmp( buf, "Ftn-Seg: ", 9 ) )
    {  SEAThdr.SegNo = (unsigned short)strtoul(buf + 9, &cp, 10);
       if( *cp && *cp=='-' )
         SEAThdr.SegCount = (unsigned short)strtoul(cp+1, NULL, 10);
       w_log(LL_DEBUG, "SEAThdr.SegNo='%u'", (unsigned)SEAThdr.SegNo);
       w_log(LL_DEBUG, "SEAThdr.SegCount='%u'", (unsigned)SEAThdr.SegCount);
    }
    else if( !sstrnicmp( buf, "Ftn-Seg-Crc32: ", 15 ) )
    {  SEAThdr.SegCrc32 = (uint32_t)strtoul(buf + 15, NULL, 16);
       w_log(LL_DEBUG, "SEAThdr.SegCrc32='%x'", (unsigned long)SEAThdr.SegCrc32);
    }
    else if( !sstrnicmp( buf, "Ftn-Seg-ID: ", 12) )
    {  SEAThdr.SegID = stripLeadingChars(sstrdup(buf + 12), " \t");
       w_log(LL_DEBUG, "SEAThdr.SegID='%s'", SEAThdr.SegID);
    }
    else
       rc=1;

  w_log( LL_FUNC, "storeSEATHeaderLine() end, rc=%d", rc );
  return rc;
}

/*  Return content-type enum value
 */
enum contents getContentType(const char *buf)
{
   enum contents content=con_Text;

   if( fc_stristr( buf, "text/plain" ) ) content = con_TextPlain;
   else if( fc_stristr( buf, "text/" ) )  content = con_Text;
   else if( fc_stristr( buf, "application/octet-stream" ) )
          content = con_File;
   else   content = con_File;

   return content;
}

/*  Return encoding enum value
 */
static enum encodings getContentTransferEncoding(const char *buf)
{
  if( fc_stristr( buf, "BASE64" ) )  return  en_BASE64;
  else if( fc_stristr( buf, "x-uuencode" ) )  return  en_UUE;
  else if( fc_stristr( buf, "quoted-printable" ) )  return  en_QUOTED;
  return en_Unknown;
}


/*  Parse "Content-Disposition:" value and return attachment filename or NULL
 */
char *get_Filename_From_ContentDisposition(const char *buf)
{ char *temp=NULL, *filename=NULL;

  if( fc_stristr( buf+21, "attachment;" ) ){
    temp = fc_stristr( buf+22, "filename=" );
    if( temp ){
      filename = stripRoundingChars(sstrdup(temp+9), " \"\t");
      temp=NULL;
      w_log(LL_DEBUG, "filename=%s (from Content-Disposition: ... ; filename)", filename);
    }
  }

  return filename;
}


/* Return name of written file or NULL pointer
 */
char *processMIMESection( const char *inbound, FILE *fd, const s_link *link )
{ static enum contents  content  = con_Text;
  static enum encodings encoding = en_None;
  char *filename=NULL;
  char *temp=NULL;
  int res=0;

  w_log( LL_FUNC, "processMIMESection()" );

  if( msgHeader.boundary )
    do{  /* Process MIME section header lines */
      buf[0]='\0';
      fgets(buf, sizeof(buf), fd);
      if( feof(fd) || ferror(fd) )
      { w_log( LL_ERR, "Can't read MIME section header: %s", feof(fd)? "EOF" : strerror(errno) );
        w_log( LL_FUNC, "processMIMESection() rc=NULL" );
        return NULL;
      }
      stripCRLF(buf);
      w_log( LL_DEBUG, "Process '%s'", buf);
      if( !buf[0]) break; /* End of subheader, exit from 'do' cycle */
  /*
              Content-Type: application/octet-stream; name="EC5A0069.SU3"
              Content-Transfer-Encoding: Base64
              Content-Disposition: attachment; filename="EC5A0069.SU3"
  */

      nfree(temp); /* Prevent use already allocated string */

      if( !sstrnicmp( buf, "Content-Type:", 13) )
      {  content = getContentType(buf+13);
         if( content == con_File )
         {    temp = fc_stristr( buf+25, "name=" );
              if( temp ){
                filename = stripRoundingChars(sstrdup(temp+5), " \"\t");
                temp=NULL;
                w_log(LL_DEBUG, __FILE__ ":%u:filename=%s (from 'Content-Type: ...; name')", __LINE__, filename);
              }
         }
      }

      if( fc_stristr( buf, "Content-Transfer-Encoding:" ) == buf )
        encoding = getContentTransferEncoding(buf+26);

      if( !strnicmp( buf, "Content-Disposition:", 20 ) &&
          (temp = get_Filename_From_ContentDisposition(buf+22))
        ){ filename=temp; temp=NULL; }

    }while( sstrcmp( buf, msgHeader.boundary ) || sstrcmp( buf, msgHeader.endboundary ) );
  else
  {
    if( msgHeader.ContentType )
    {  content = getContentType(msgHeader.ContentType);
      if( content == con_File )
      { temp = fc_stristr( buf+25, "name=" );
        if( temp ){
          filename = stripRoundingChars(sstrdup(temp+5), " \"\t");
          temp=NULL;
          w_log(LL_DEBUG, "filename=%s (from 'Content-Type: ...; name')", filename);
        }
      }
    }

    if( msgHeader.ContentTransferEncoding )
      encoding = getContentTransferEncoding(buf+26);

    if( msgHeader.ContentDisposition &&
        (temp = get_Filename_From_ContentDisposition(buf+22))
      ){ filename=temp; temp=NULL; }

  }

  switch( content ){
  case con_TextPlain: filename=processTextPlain( inbound, fd, link ); break;
  case con_Text:      filename=processTextOrUUE( inbound, fd, link ); break;
  case con_File:
                switch( encoding ){
                case en_BASE64:
                                 if( sstrlen(filename)==0 )
                                   createTempFileIn(inbound,UnknownFileExt,'b',&filename);
                                 res = !(temp=processBASE64( inbound, fd, filename ));
                                 nfree(filename);
                                 filename=temp;
                                 break;
                case en_UUE:
                                 if( sstrlen(filename)==0 )
                                   createTempFileIn(inbound,UnknownFileExt,'b',&filename);
                                 res = !(temp=processUUE( inbound, fd, filename ));
                                 nfree(filename);
                                 filename=temp;
                                 break;
                default:         filename=processTextOrUUE( inbound, fd, link );
                }
                break;
  default:      filename=processTextOrUUE( inbound, fd, link );
  }
  if( res && msgHeader.boundary ) /* skip to boundary if not plain MIME */
    while( sstrcmp( buf, msgHeader.boundary ) || sstrcmp( buf, msgHeader.endboundary ) )
    {
      buf[0]='\0';
      fgets(buf, sizeof(buf), fd);
      if( feof(fd) || ferror(fd) )
      { w_log( LL_ERR, "Can't read MIME section: %s", feof(fd)? "EOF" : strerror(errno) );
        w_log( LL_FUNC, "processMIMESection() rc=NULL" );
        nfree(filename);
        return NULL;
      }
    }

  w_log( LL_FUNC, "processMIMESection() OK (rc='%s')", filename );
  return filename;
}

char *processBASE64( const char *inbound, FILE *fd, const char *filename )
{ int len=0;
  FILE *fout=NULL;
  unsigned char *buf2=NULL;
  char *pathname=NULL;

  w_log( LL_FUNC, "processBASE64(%s,,%s)", inbound, filename );

  if( sstrlen(filename)==0 )
  { w_log(LL_ERR, "File name is empty!");
    w_log( LL_FUNC, "processBASE64() rc=NULL" );
    return NULL;
  }

  xstrscat(&pathname, inbound, filename, NULL);
  fout = createInboundFile(&pathname);
  w_log(LL_INFO, "Created '%s'", pathname);

  do{  /* Process base64 lines */
    buf[0]='\0';
    fgets(buf, sizeof(buf), fd);
    if( feof(fd) || ferror(fd) )
    { w_log( LL_ERR, "Can't read line: %s", feof(fd)? "EOF" : strerror(errno) );
      w_log( LL_FUNC, "processBASE64() rc=NULL" );
      fclose(fout);
      return NULL;
    }
    stripCRLF(buf);
    if( !buf[0] ) continue; /* skip empty line */
    if( !(sstrcmp( buf, msgHeader.boundary ) && sstrcmp( buf, msgHeader.endboundary )) )
      break;
    len = base64DecodeLine(&buf2,(unsigned char*)buf);
    if( len>0 ) fwrite( buf2, len, 1, fout );
    nfree(buf2);
  }while( len>=0 );
  fclose(fout);

  w_log( LL_FUNC, "processBASE64() OK (rc=%s)", pathname );
  return pathname;
}

/* Decode uudecoded file or section.
 * Return values:
 * OK:        file name (malloc())
 * error: NULL
 * not uue:   empty string (const)
 */
char *processUUE( const char *inbound, FILE *ifd, const char *_filename )
{ FILE *file=NULL;
  char *filename=NULL; /* main filename */
  char *tp=NULL;       /* temp file name (with path) */
  char *cp=NULL;       /* temp pointer */
  unsigned char *linebuf=NULL;  /* decoded line buffer */
  int i=0, ci=0;
  UINT32 sectnum=0, sectlast=0;
  UINT32 CRC32val=0xFFFFFFFFUl; /*  CRC32; init value */
  UINT32 sectsumr=0, sectsize=0, fullsumr=0, fullsize=0; /* from sum lines */
  struct dirent **ls=NULL;       /* 2nd arg of scandir() */
  DIR *dirp=NULL;

#if UNIX
  /* scandir only for UNIXes */
  /* compare dirent with mask: inbound/file.suffix
     use in scandir */
  int dirfilter(struct dirent *ds) /* directory filter: 3rd arg of scandir() */
  { int j;
    for( j=1; j<=sectlast; j++ )
    { sprintf(cp+i,"%u",j);   /* 'i' var: suffix pos, set in function processUUE()*/
      if( !sstrcmp(ds->d_name,cp) ) return 1;
    }
    return 0;
  }

  /* for sort of directory */
  int dircmp(struct dirent *dp1, struct dirent *dp2) /* directory compare (4th arg of scandir())*/
  { return sstrcmp(dp1->d_name,dp2->d_name);
  }
#endif
  w_log( LL_FUNC, "processUUE() (%s)",buf );

  if( !strncmp(buf,"begin",5) )
  {
    /* begin 644 12345678.pkt */
    filename = scalloc( sstrlen(buf)-5, 1 );
    i=sscanf( buf, "begin %*o %s", filename);
    if( i ){
      w_log(LL_INFO, "uu-decode filename: '%s'", filename);
//#if 0
//     /* skip from "begin" to name of file (ignore permissions field) */
//    for( cp=buf+6 /*after 'begin '*/; *cp && !isalpha(*cp); cp++ );
//    if(*cp) /* filename present */
//    {  filename = cp;
//       strchr(cp,' ');
//       *cp = '\0';
//       filename = sstrdup( filename );
//    }
//#endif
      xstrscat( &tp,  inbound, filename, NULL );
      if( !(file = createInboundFile(&tp)) )
      {  nfree(filename);
         nfree(tp);
         return NULL;
      }
      *filename='\0';
      xstrcat( &filename, tp );
    }else
    { if( _filename ){
        w_log(LL_WARN, "Filename not found in 'begin' clause of uue: '%s', use value from RFC822 header",buf);
        filename[0]='\0';
        xstrcat( &filename, _filename );
        xstrscat( &tp,  inbound, filename, NULL );
        if( !(file = createInboundFile(&tp)) )
        {  nfree(filename);
           nfree(tp);
           return NULL;
        }
        *filename='\0';
        xstrcat( &filename, tp );
      }else{ /* begin clause without filename, generate random */
        w_log(LL_WARN, "Filename not found in 'begin' clause of uue: '%s', generate random", buf);
        file = createTempFileIn( inbound, UnknownFileExt, 'b', &filename );
        if( !file )
        {  nfree(filename);
           nfree(tp);
           return NULL;
        }
      }
    }
  }
  else if( !sstrnicmp(buf,"section",sizeof("section")-1) )
  { /* section 1 of 3 of file 12345678.zip */
    /* section 1 of file 12345678.zip      */
    filename = smalloc(sstrlen(buf)-18);
    i = sscanf( buf, "section %u of %u of file %s", &sectnum, &sectlast, filename );
    if( i<3 ) i = sscanf( buf, "section %u of file %s", &sectnum, filename );
    if( !i )
    { w_log(LL_WARN, "Illegal 'section' clause in uue: '%s'. This is plain text (possible).", buf);
      nfree(filename);
      w_log( LL_FUNC, "processUUE() failed" );
      return NULL;
    }

    /* use subdir for collect uue sections */
    xscatprintf( &tp, "%s" SECTIONS_SUBDIR "%c%s", inbound, DIRSEP, filename );
/*    if( direxist(tp) || mkdir(tp)==0 )*/
    if( _createDirectoryTree(tp) ) /* 0:success 1:error */
    { w_log(LL_ERR, "Can't make directory '%s': %s.", tp, strerror(errno));
      nfree(filename);
      w_log( LL_FUNC, "processUUE() failed" );
      return NULL;
    }
    else
    {
      if(sectnum<=99999999){
        xscatprintf( &cp, "%s%c%08u", tp, DIRSEP, sectnum );
      }else{
        w_log( LL_CRIT, "Too big section number (%lu), skip message.", (unsigned long)sectnum );
        nfree(tp);
        nfree(filename);
        w_log( LL_FUNC, "processUUE() failed" );
        return NULL;
      }
    }

//  Use this instead previous block. Possible...
//#if defined(__DOS__) || defined(__MSDOS__) || defined(MSDOS)  /* this branch are spare ? */
//    /* generate section file name (main filename with num suffix) */
//    xstrscat( &tp, inbound, filename, NULL );
//    i = sstrlen( inbound );
//    cp = strchr( tp+i, '.' );
//    if( cp>tp+i ) *cp='\0';
//    if(sectnum<=0xFFF)
//      xscatprintf( &tp, ".%03x", sectnum );
//    else{
//      w_log( LL_CRIT, "Too big section number (%u), skip message.", sectnum );
//      nfree(tp);
//      nfree(filename);
//      w_log( LL_FUNC, "processUUE() failed" );
//      return NULL;
//    }
//#else
//    /* section file name (main filename with added num suffix) */
//    xscatprintf( &tp, "%s%s.%3u", inbound, filename, sectnum );
//#endif

    if( !(file = createFileExcl(cp,"wb")) )
    {  nfree(filename);
       return NULL;
    }
    *filename='\0';
    xstrcat( &filename, tp );
  }

  i=0;
  do{

    fgets( buf, sizeof(buf), ifd );
    stripCRLF(buf);
    if ( !sstrnicmp(buf,"end",3) )      /* end uuencoded file */
      i = processUUE_end;
    else if ( !sstrnicmp(buf,"sum",3) ) /* end section */
      i = processUUE_sum;
    else if( feof(ifd) )
      i = processUUE_eof;
    else if( ferror(ifd) )
      i = processUUE_ierr;
    else{
      i = uudecodeLine(&linebuf, (unsigned char*)buf);
      if ( i<0 )
        i = processUUE_uue;
      else if( i>0 ){
        CRC32val = memcrc32( (char*)linebuf, i, CRC32val);
        if( fwrite( linebuf, 1, i, file) != i )
        { w_log( LL_ERR, "Write error where decoding file '%s': %s. "
             "File saved partially (%d bytes)", filename, strerror(errno), ci+i);
          i=processUUE_oerr;
        }else ci+=i;
      }
    }

  }while( i>=0 );

  fclose(file);

  switch( i )
  { case processUUE_uue:    /* uudecodeLine() error */
       w_log( LL_ERR, "UU-decoder error, save file partially: '%s'", tp );
       break;
    case processUUE_oerr: /* write message & save file... */
       fclose(file);
       w_log( LL_ERR, "Write error: %s, decoded part of file '%s' saved as %s",
                                            strerror(errno), filename, tp );
       // TODO: rename file
       w_log( LL_FUNC, "processUUE() FAILED" );
       nfree(tp);
       nfree(cp);
       return NULL;
       break;
    case processUUE_ierr:
       fclose(file);
       w_log( LL_ERR, "Read error: %s, decoded part of file '%s' saved as %s",
                                            strerror(errno), filename, tp );
       // TODO: rename file
       w_log( LL_FUNC, "processUUE() FAILED" );
       nfree(tp);
       nfree(cp);
       return NULL;
    case processUUE_eof:
       if( sectnum )
       { if( sectlast )
           w_log( LL_WARN, "EOF, section %u of %u of file '%s' saved as %s",
                                          sectnum, sectlast , filename, tp );
         else
           w_log( LL_WARN, "EOF, section %u of file '%s' saved as %s", sectnum,
                                                           filename, tp );
       }else
         w_log( LL_WARN, "EOF, beginning part of file '%s' saved", filename );
       break;
    case processUUE_end:
       if( sectnum )
       { if( sectlast )
           w_log( LL_WARN, "Section %u of %u of file '%s' saved (%d bytes) as %s",
                                        sectnum, sectlast , filename, ci, tp );
         else
           w_log( LL_WARN, "Section %u of file '%s' saved (%d bytes) as %s",
                                                   sectnum, filename, ci, tp );
       }else
           w_log( LL_INFO, "File '%s' saved (%d bytes), CRC32=%08X",
                filename, ci, CRC32val);
/*     w_log( LL_INFO, "CRC32 of file '%s' is %08X", tp, filecrc32(tp)^0xFFFFFFFFl ); */

//       fgets( buf, sizeof(buf), ifd );   /* read 'sum...' (if present)*/
//       if( sstrnicpy(buf,"sum",3) ) break; /* not 'sum...', exit*/

       /* read untile 'sum' line or eof  */
       while( fgets(buf,sizeof(buf),ifd)  &&  (*buf=='\r' || *buf=='\n')
           && !feof(ifd) && !ferror(ifd) && sstrnicmp(buf,"sum",3) ) *buf='\0';
       if( feof(ifd) || ferror(ifd) ) break; /*else parce "sum -r/size"*/

    case processUUE_sum:
       strLower(buf);
       if( !sstrstr(buf,"section") )
       { sscanf( buf, "sum -r/size %u/%u section", &sectsumr, &sectsize );
         while( fgets(buf,sizeof(buf),ifd)  &&  (*buf=='\r' || *buf=='\n')
                && !feof(ifd) && !ferror(ifd) );
       }
       if( !sstristr(buf,"entire input file") )
         sscanf( buf, "sum -r/size %u/%u entire input file", &fullsumr, &fullsize );
  }

  if(sectnum /*&& sectnum==sectlast*/)
  {  /* check for all parts received (& compose file) */

#if UNIX
     if ( scandir( tp, &ls, dirfilter, dircmp)==sectlast ) /* all part files exists */
#else
     i=0;
     if(( dirp = opendir(tp) )){
       ls = calloc(sectlast+1, sizeof(*ls));
/*       xstrcat( &cp, "*") ; */
       while( (ls[i]=readdir(dirp)) ){
/*         if( patmat(ls[i]->d_name, cp) ) {
           i++;
         }else nfree(ls[i]);
*/
       }
       (void)closedir(dirp);
     }
     if( i>0 && (i==sectlast || sectlast==0) )
#endif
     { w_log( LL_INFO, "All parts of '%s' received, compose file...", filename );
       /* concatenate files (inbound + ls[j]->d_name) */
       dirsort(ls);
       if( cp ) *cp='\0';
       xscatprintf( &cp, "%s%c%08u", tp, DIRSEP, filename );
       if( concat_files( tp, ls, cp ) == 0 )
       { RemoveDirectoryTree(tp);
         nfree(tp);
         tp=cp;
       }else{
         w_log( LL_ERR, "All sections received into %s, but not concatenated", tp );
       }
     }else
       w_log( LL_INFO, "Not all parts of '%s' received, wait other part.", filename);
     while(--i>=0){nfree(ls[i])};
     nfree(ls);
  }

  nfree(tp);
  nfree(cp);
  w_log( LL_FUNC, "processUUE() OK" );
  return filename;
}

int processMime( const char *inbound, FILE *fd, const s_link *link )
{
  int rc=0;
  char *fname=NULL, *tofname=NULL;
/*char *temp=NULL;*/

  w_log( LL_FUNC, "processMime(%s)", inbound );

  if( msgHeader.boundary )
    do{  /* skip to boundary */
      buf[0]='\0';
      fgets(buf, sizeof(buf), fd);
      if( feof(fd) || ferror(fd) )
      { w_log( LL_ERR, "Can't find MIME section: %s", feof(fd)? "EOF" : strerror(errno) );
        w_log( LL_FUNC, "processMime() end (rc=1)" );
        return 1;
      }
      stripCRLF(buf);
      w_log( LL_DEBUG, "Process '%s'", buf);
    }while( sstrcmp( buf, msgHeader.boundary ) );

  do{
    fname = processMIMESection( config->tempInbound, fd, link );
    if( fname )
    { /* move file from temporary to sec/unseq/listed inbound */
      if(tofname) *tofname='\0';
      xstrscat( &tofname, inbound, basename(fname), NULL );
      createInboundFile(&tofname); /*to select notused filename*/
      if( move_file(fname,tofname, 1) )
      { rc++;
        w_log( LL_ERR, "Can't move %s to %s: %s", fname, tofname, strerror(errno) );
      }else
      {    w_log( LL_FILE, "File %s moved to %s", fname, tofname );
           w_log( LL_INFO, "File %s received", tofname );
      }
      nfree(tofname);
    }
  }while( msgHeader.boundary && !feof(fd) && sstrcmp( buf, msgHeader.endboundary ) );

  w_log( LL_FUNC, "processMime() OK" );
  return 0;
}


int composeSEAT(const char *inbound, const char *fname)
{
  int rc=0, i=0;
  char *tofname=NULL;
  struct dirent **ls=NULL;       /* 2nd arg of scandir() */
  DIR *dirp=NULL;

  w_log( LL_FUNC, "composeSEAT()" );

  if( SEAThdr.SegCount>1 )
  { /* Rename to wait other sections */
    tofname = sstrdup(config->tempInbound);
    xscatprintf( &tofname, "%8X.%u", SEAThdr.Crc32, SEAThdr.SegNo );
    createInboundFile(&tofname); /*to select notused filename*/
    if( move_file(fname,tofname, 1) )
    { rc++;
      w_log( LL_ERR, "Can't rename %s to %s: %s", fname, tofname, strerror(errno) );
    }else
      w_log( LL_DEBUG, "File %s renamed to %s", fname, tofname );
    nfree(tofname);
    /* Check for all sections */
    i=0;
    if(( dirp = opendir(inbound) )){
      ls = scalloc(SEAThdr.SegCount+1, sizeof(*ls));
      xscatprintf( &tofname, "%8X.*", SEAThdr.Crc32 ) ;
      while( (ls[i]=readdir(dirp)) ){
           if( patmat(ls[i]->d_name, tofname) ) {
             i++;
           }else nfree(ls[i]);
      }
      (void)closedir(dirp);
    }
    if( i== SEAThdr.SegCount )
    { w_log( LL_INFO, "All parts of '%s' received, compose file...", fname );
      rc= concat_files( config->tempInbound, ls, tofname );
    }else{
      w_log( LL_INFO, "Not all parts of '%s' received, wait other part.", fname);
    }
    while(--i>=0){nfree(ls[i])};
    nfree(ls);
    nfree(tofname);
  }
  w_log( LL_FUNC, "composeSEAT() end (rc=%d)", rc );
  return rc;
}

int processSEAT( const char *inbound, FILE *fd, const s_link *link )
{
  int rc=0, i=0;
  char *fname=NULL,*tofname=NULL;
 /*char *temp=NULL;*/
  struct dirent **ls=NULL;       /* 2nd arg of scandir() */
  DIR *dirp=NULL;

  w_log( LL_FUNC, "processSEAT()" );

  do{
    buf[0]='\0';
    fgets(buf, sizeof(buf), fd);
    if( feof(fd) || ferror(fd) )
    { w_log( LL_ERR, "Can't read SEAT message prebody: %s", feof(fd)? "EOF" : strerror(errno) );
      w_log( LL_FUNC, "processSEAT() end (rc=1)" );
      return 1;
    }
    stripCRLF(buf);
    w_log( LL_DEBUG, "Process '%s'", buf);
    if( !buf[0]) continue;
    rc=2;
    if( isSEATheader(buf) )    /* SEAT kludge */
      storeSEATHeaderLine( buf );
  }while( sstrcmp( buf, msgHeader.boundary ) );
  if( feof(fd) ){
    w_log( LL_FUNC, "processSEAT() end (rc=%d)", rc );
    return rc;
  }
  rc=0;
  do{ w_log( LL_DEBUG, "Process MIME section" );
    fname = processMIMESection( config->tempInbound, fd, link );
    if( fname )
    {
      if( SEAThdr.SegCount>1 )
      { /* Rename to wait other sections */
        tofname = sstrdup(config->tempInbound);
        xscatprintf( &tofname, "%8X.%u", SEAThdr.Crc32, SEAThdr.SegNo );
        createInboundFile(&tofname); /*to select notused filename*/
        if( move_file(fname,tofname, 1) )
        { rc++;
          w_log( LL_ERR, "Can't rename %s to %s: %s", fname, tofname, strerror(errno) );
        }else
          w_log( LL_DEBUG, "File %s renamed to %s", fname, tofname );
        nfree(tofname);
        /* Check for all sections */
#if UNIX && 0
     /* Need declare dirfilter() and dircmp() local for processSEAT() */
        if ( scandir( inbound, &ls, dirfilter, dircmp)==SEAThdr.SegCount ) /* all part files exists */
#else
        i=0;
        if(( dirp = opendir(inbound) )){
          ls = scalloc(SEAThdr.SegCount+1, sizeof(*ls));
          xscatprintf( &tofname, "%8X.*", SEAThdr.Crc32 ) ;
          while( (ls[i]=readdir(dirp)) ){
               if( patmat(ls[i]->d_name, tofname) ) {
                 i++;
               }else nfree(ls[i]);
          }
          (void)closedir(dirp);
        }
        if( i== SEAThdr.SegCount )
#endif
        { w_log( LL_INFO, "All parts of '%s' received, compose file...", fname );
          // concatenate files (inbound + ls[j]->d_name)
          rc= concat_files( inbound, ls, tofname );
        }else{
          w_log( LL_INFO, "Not all parts of '%s' received, wait other part.", fname);
        }
        while(--i>=0){nfree(ls[i])};
        nfree(ls);
        nfree(tofname);
      }
      else
      { /* move file from temporary to sec/unseq/listed inbound */
        if(tofname) *tofname='\0';
        xstrscat( &tofname, inbound, basename(fname), NULL );
        createInboundFile(&tofname); /*to select notused filename*/
        if( move_file(fname,tofname, 1) )
        { rc++;
          w_log( LL_ERR, "Can't move %s to %s: %s", fname, tofname, strerror(errno) );
        }else
          w_log( LL_DEBUG, "File %s moved to %s", fname, tofname );
        nfree(tofname);
      }
    }
  }while( !feof(fd) && sstrcmp( buf, msgHeader.endboundary ) );

  w_log( LL_FUNC, "processSEAT() OK (rc=%d)", rc );
  return rc;
}

/* save message body to .txt file  (todo: save to .pkt)
 * Return file name or NULL
 *
 */
char *processTextPlain( const char *inbound, FILE *fd, const s_link *link )
{ char *filename=NULL;
  FILE *ofd=NULL;
  unsigned long got=0;

  w_log( LL_FUNC, "processTextPlain()" );

  ofd = createTempFileIn(inbound, "txt", 't', &filename);
  if(ofd)
    while( (got = fread( buf, 1, sizeof(buf), fd))>0 )
    {
      if( feof(fd) )
        break;
      if( ferror(fd) )
      {
        w_log( LL_ERR, "Can't read message: \"%s\"", strerror(errno) );
        w_log( LL_FUNC, "processTextPlain() failed" );
        if( got )  fwrite(buf, 1, got, ofd);
        fclose (ofd);
        if( ftell(ofd)>0 )  return filename;
        else{ nfree(filename); remove(filename); return NULL; }
      }
      fwrite(buf, 1, got, ofd);
      if( ferror(ofd) )
      {
        w_log( LL_ERR, "Can't save message to temp file: \"%s\"", strerror(errno) );
        w_log( LL_FUNC, "processTextPlain() failed" );
        fclose (ofd);
        if( ftell(ofd)>0 )  return filename;
        else{ nfree(filename); remove(filename); return NULL; }
      }
    }
  w_log( LL_FILE, "Message saved to temp inbound: %d bytes", ftell(ofd) );

  w_log( LL_FUNC, "processTextPlain() OK" );
  return filename;
}

/* Return file name or NULL
 *
 */
char *processTextOrUUE( const char *inbound, FILE *fd, const s_link *link )
{ char *filename=NULL;

  w_log( LL_FUNC, "processTextOrUUE()" );

  while( buf[0]==0 || buf[0]==10 || buf[0]==13 ) /* while empty string: cr or lf*/
  { /* skip empty lines */
    buf[0]='\0';
    fgets(buf, sizeof(buf), fd);
    if( feof(fd) )
    { w_log( LL_WARN, "Empty message body" );
      w_log( LL_FUNC, "processTextOrUUE() rc=0" );
      return NULL;
    }
    else if( ferror(fd) )
    { w_log( LL_ERR, "Error reading message body (empty body): %s", strerror(errno) );
      w_log( LL_FUNC, "processTextOrUUE() rc=NULL" );
      return NULL;
    }
  }

  stripCRLF(buf);
/*  w_log( LL_DEBUGA, __FILE__ ":%u:Process '%s'", __LINE__, buf ); */

  if( !sstrnicmp(buf,"begin",sizeof("begin")-1) || !sstrnicmp(buf,"section",sizeof("section")-1) )
      filename = processUUE( inbound, fd, NULL );
  else
      processTextPlain( inbound, fd, link );

  w_log( LL_FUNC, "processTextOrUUE() OK (rc=0)" );
  return filename;
}

int processAttUUCP( const char *inbound, FILE *fd, const s_link *link )
{
  w_log( LL_FUNC, "processAttUUCP()" );
  w_log( LL_FUNC, "processAttUUCP() OK (rc=0)" );
  return 0;
}


/* заготовка */
int process( const char *inbound, FILE *fd, const s_link *link )
{
  w_log( LL_FUNC, "process()" );
  w_log( LL_FUNC, "process() OK (rc=0)" );
  return 0;
}


/*
 * Receive bundle(s) or text from message body
 */
int processEml( FILE *fd, const s_link *link )
{ int rc=0;
  char *inbound=NULL, *pwd=NULL;

  w_log( LL_FUNC, "processEml()" );

  if( link )
  { if( link->defaultPwd && link->defaultPwd[0] )
    { pwd=link->defaultPwd;
      inbound = config->protInbound;
    }
    else  inbound = config->listInbound;
  }
  else inbound = config->inbound;

/*
  if( msgHeader.XMailer && sstricmp( msgHeader.XMailer, "Internet Rex" ) )
  { w_log( LL_INFO, "Internet Rex detected" );
     processSEAT(inbound, fd, link);
  }
*/
  if( msgHeader.f_SEAT )
     processSEAT(inbound, fd, link);
  else if( msgHeader.MimeVersion || msgHeader.ContentType )
     processMime(inbound, fd, link);
  else
     processTextOrUUE(inbound, fd, link);

  w_log( LL_FUNC, "processEml() OK" );
  return rc;
}

/*
 * Process message
 */
int receive(FILE *fd)
{ int rc=0, i=0;
  FILE *myfd;

  w_log( LL_FUNC, "receive()" );

  if( debugflag && fd==stdin )
  { if( (myfd = saveToTemp(fd))==NULL )  /* Save message to temp file */
    { w_log( LL_FUNC, "receive() failed" );
      return -1;
    }
  }
  else myfd=fd;
  if( getMsgHeader(myfd) ) return 1; /* Read error */

  /* Check sender */
  i = 0;
  if( msgHeader.fromaddr )
    for (; i < config->linkCount; i++)
    {
      /* Known sender? */
      if( config->links[i].email &&
          !sstricmp(config->links[i].email,msgHeader.fromaddr) )
      {  w_log( LL_LINK, "Email from %s (%s)", aka2str(config->links[i].hisAka), msgHeader.fromaddr );
         rc=processEml( myfd, config->links+i );
         break;
      }
    }
  if( !msgHeader.fromaddr || i >= config->linkCount ) /* Unknown sender */
  {  w_log( LL_LINK, "Email from unknown (%s)", msgHeader.fromaddr );
     rc=processEml( myfd, NULL );
  }

  if( debugflag && fd!=myfd ) fclose(myfd); /* Close temporary file */

  w_log( LL_FUNC, "receive() OK" );
  return rc;
}

void printver()
{ fprintf( stderr,	/* ITS4: ignore */
           "hereceive v%s - receive FTN files via email\n\n(part of the EmailPkt package, The Husky FidoSoft Project module) \n\n",
           version() );
}

void usage()
{ printver();
  fprintf( stderr,	/* ITS4: ignore */
/*           "Usage: %s [-qVD] [-c configfile]\n", program_name);*/
           "Usage: hereceive [-qVD] [-c configfile] [input_file]\n");
#ifndef UNIX
  fprintf( stderr,	/* ITS4: ignore */
/*           "or:    %s [--help] [--version] [--debug] [--quiet] [--config=configfile]\n", program_name);*/
           "or:    hereceive [--help] [--version] [--debug] [--quiet] [--config=configfile] [input_file]\n");
#endif
  exit(-1);
}

int
main( int argc, char **argv)
{ int rc=0, op=0, quiet=0;
  char *cp=NULL, *inputfile=NULL;
  FILE *fd;

  program_name = sstrdup(basename(argv[0]));

#if UNIX
  opterr = 0;
  while ((op = getopt(argc, argv, "DVc:d:hq")) != -1)
    switch (op) {
    case 'c': /* config file */
              cp = sstrdup(optarg);
    case 'D': /* debug mode */
              debugflag=1;
              break;
    case 'q': /* quiet mode */
              quiet=0;
              break;
    case 'V': /* version */
              printver();
              nfree(cp); nfree(program_name);
              return 0;
    case 'h': /* help */
    default :
              usage();
              nfree(cp); nfree(program_name);
              return 0;
    }
   if(optind<argc)  /* parameter present */
     inputfile=sstrdup(argv[optind]);
#else

   for( op=1; op<argc; op++ ){
     if(argv[op][0]=='-'){
       if(argv[op][1]=='-'){
         if(sstrcmp(argv[op]+2,"quiet"))
                quiet=0;
         if(sstrcmp(argv[op]+2,"debug"))
                debugflag=1;
         if( strstr(argv[op]+2,"config=")==(argv[op]+2) ) {
             cp = strchr(argv[op], '=');
             if( cp ) cp = sstrdup(++cp);
             else{
               fprintf(stderr, "Strange error: --config=... not contents '='");
                nfree(program_name);
                nfree(inputfile);
               return 2;
             }
         }
         if(sstrcmp(argv[op]+2,"version")) {
                printver();
                nfree(program_name);
                nfree(cp);
                nfree(inputfile);
                return 0;
         }
         if(sstrcmp(argv[op]+2,"help")) {
                usage();
                nfree(program_name);
                nfree(cp);
                nfree(inputfile);
                return 0;
         }
       }else{
         if(strchr(argv[op],'q'))
                quiet=0;
         if(strchr(argv[op],'D'))
                debugflag=1;
         if(strchr(argv[op],'c')) {
                if(++op < argc){
                  if( argv[op] && argv[op])	
                    cp = sstrdup(argv[op]); /* config-file */
                  else{
                    fprintf(stderr, "Config file name is empty!\n");
                    nfree(program_name);
                    nfree(inputfile);
                    return 2;
                  }
                }else{
	          fprintf(stderr, "Config file name don't present after '-c' option!\n");
                  nfree(program_name);
                  nfree(inputfile);
                  return 2;
                }
         }
         if(strchr(argv[op],'V')) {
                printver();
                nfree(program_name);
                nfree(cp);
                nfree(inputfile);
                return 0;
         }
         if(strchr(argv[op],'h')) {
                usage();
                nfree(program_name);
                nfree(cp);
                nfree(inputfile);
                return 0;
         }
       }
     }else{
	if( inputfile ) {
          /* parameter already exist */
          fprintf(stderr,"Illegal parameter: %s", argv[op]);
          nfree(program_name);
          nfree(cp);
          nfree(inputfile);
          return 1;
        }else
          inputfile = sstrdup(argv[op]);
     }
   }
#endif

  config = readConfig(cp);
  nfree(cp);
  if(quiet) config->logEchoToScreen=0;

  if( !openLog( LOGFILE, program_name, config) )
    fprintf(stderr, "Can't init log! Use stderr instead\n");

  if( argc>0 )
    { cp = sstrdup(argv[0]);
      for ( rc=1; rc<argc; rc++ )
        xstrscat( &cp, " ", argv[rc], NULL );
      w_log(LL_PRG,"Start %s %s ( %s )", program_name, version(), cp);
      nfree(cp);
    }
    else
      w_log(LL_PRG,"Start %s %s", program_name, version() );


  if( !sstrlen(config->protInbound) )
    w_log(LL_CRIT, "protInbound not defined in config file! Abort");
  else if( !sstrlen(config->listInbound) )
    w_log(LL_CRIT, "listInbound not defined in config file! Abort");
  else if( !sstrlen(config->inbound) )
    w_log(LL_CRIT, "inbound not defined in config file! Abort");
  else{
    /* Normal operations */

      memset(&msgHeader, 0, sizeof(msgHeader)); /* Clean structure msgHeader */
      memset(&SEAThdr, 0, sizeof(SEAThdr));     /* Clean structure SEAThdr */
      if( inputfile && inputfile[0] ) {
        fd=fopen(inputfile, "r");
        if( fd==NULL )
          w_log(LL_ERR, "Can't open input file: '%s'", inputfile);
        nfree(inputfile);
      }else{
        fd=stdin;
      }
      if( fd )
      { rc = receive(fd);
        if( fd != stdin )
          fclose(fd);
      }
  }
  dispose_msgHeader();
  disposeConfig(config);
  w_log(LL_PRG,"Stop %s", program_name);
  nfree(program_name);
  closeLog();

  return rc;
}
