/* EMAILPKT: Husky e-mailer
 *
 * (C) Copyleft Stas Degteff 2:5080/102@FIDOnet, g@grumbler.org
 * (c) Copyleft Husky developers team http://husky.sorceforge.net/team.html
 *
 * $Id$
 * Miscellaneous functions.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#ifdef UNIX
#   include <unistd.h> /*mktemp()*/
#   include <dirent.h>
#else
#  include <share.h>
#endif

/* Husky library */
#include <fidoconf/fidoconf.h>
#include <fidoconf/common.h>
#include <fidoconf/log.h>
#include <fidoconf/xstr.h>
#include <smapi/progprot.h>

#include "ecommon.h"
#include "../h/version.h"

#ifndef O_EXLOCK
# ifdef O_EXCL
#  define  O_EXLOCK O_EXCL
# else
#  define O_EXLOCK 0
# endif
#endif

#if defined(__UNIX_) || defined(__WIN32__) || defined(__FLAT__)
#define COPY_FILE_BUFFER_SIZE 128000
#else
#define COPY_FILE_BUFFER_SIZE 32000
#endif

#ifndef F_OK
#define F_OK 0
#endif

#ifndef R_OK
#define R_OK 04
#endif

#ifndef W_OK
#define W_OK 02
#endif


char *program_name=NULL;

/* Return 0 if file not exist or not file (directory)
 * Return not zero if file exist
 */
int __fexist(const char *filename)
{
    struct stat s;

    if (stat (filename, &s))
        return 0;
    return ((s.st_mode  & S_IFMT) == S_IFREG);
}


/* Strip right CR, LF & FF
 */
char *stripCRLF(char *s)
{  register short l;

/*  w_log(LL_FUNC,"stripCRLF()"); */

  l = strlen(s);
  while( (--l)>=0 )
  {  switch( s[l] ){
     case '\f':
     case '\n':
     case '\r': s[l] = 0;
                break;
       default: l=-1;
    }
  }
/*  w_log(LL_FUNC,"stripCRLF() OK"); */
  return s;
}

/*
 * Remove file, write to log
 */
int delete (const char *path)
{
  int rc;

  w_log(LL_FUNC,"delete()");

  if ((rc = unlink (path)) != 0)
    w_log(LL_ERR, "Error unlinking `%s': %s", path, strerror (errno));
  else
    w_log(LL_DEL, "Unlinked `%s'", path);

  w_log(LL_FUNC,"delete() OK");
  return rc;
}

/*
 * Truncate file, write to log
 */
int truncat (const char *path)
{ int h;

  w_log(LL_FUNC,"truncat()");

  if ((h = open (path, O_WRONLY | O_TRUNC)) == -1)
/*  if( truncate( path, 0 ) ) */
  {
    w_log (LL_ERR, "Cannot truncate `%s': %s", path, strerror (errno));
    return -1;
  }
  close( h );
  w_log(LL_TRUNC, "Truncated '%s'", path);

  w_log(LL_FUNC,"truncat() OK");
  return 0;
}


char *snprintaddr(char *string, const int size, const hs_addr addr)
{ char *temp=NULL;
  w_log(LL_FUNC,"snprintaddr()");

  if( (addr.domain)==NULL || strlen(addr.domain) == 0 )
    if( addr.point == 0 )
      xscatprintf(&temp, "%u:%u/%u", addr.zone, addr.net, addr.node);
    else
      xscatprintf(&temp, "%u:%u/%u.%u", addr.zone, addr.net, addr.node, addr.point);
  else
    if( addr.point == 0 )
      xscatprintf(&temp, "%u:%u/%u@%s", addr.zone, addr.net, addr.node, addr.domain);
    else
      xscatprintf(&temp, "%u:%u/%u.%u@%s", addr.zone, addr.net, addr.node, addr.point, addr.domain);
  if( !string ) /* return allocated string */
    string=temp;
  else if (string != temp) /* copy result to string */
  { string[0]=0;
    strncat(string, temp, size);
    nfree(temp);
  }
  w_log(LL_FUNC,"snprintaddr() OK");
  return string;
}

/* Create .BSY file for link and write PID.
 * if link.bsyFile is null - call setOutboundFilenames()
 * Return file descriptor or NULL
 */
FILE *createbsy(s_link link)
{ FILE *fd; int fh; char buf[ADRS_MAX];

  w_log(LL_FUNC,"createbsy()");

  if( strlen(link.bsyFile)==0 )
    if ( setOutboundFilenames( &link, normal) )
    {
      w_log(LL_ERR, "Error in setOutboundFilenames(), called from createbsy()" );
      w_log(LL_FUNC,"createbsy() failed");
      return NULL;
    }
#if defined (__WIN32__) || defined (__CYGWIN__) || defined (__DOS__)
  if( (fh = sopen(link.bsyFile, O_CREAT | O_TRUNC | O_WRONLY | O_EXCL, SH_DENYWR))<0 )
#else
  if( (fh = open(link.bsyFile, O_CREAT | O_TRUNC | O_WRONLY | O_EXCL))<0 )
#endif
  { if( errno==ENOENT )
      w_log(LL_FILE, "Outbound for link not exist");
    else
      w_log(LL_LINKBUSY, "link %s is busy", snprintaddr(buf, sizeof(buf), link.hisAka) );
    w_log(LL_FUNC,"createbsy() unsuccesful");
    return NULL;
  }
  fd = fdopen(fh, "w");
#ifdef __UNIX__
  fprintf(fd,"%u", (unsigned)getpid());
#endif
  w_log(LL_FILE,"Created '%s'",link.bsyFile );
  w_log(LL_FUNC,"createbsy() OK");
  return fd;
}

/* Remove .BSY file for link (check bsy file on PID).
 * if link.bsyFile is null - call setOutboundFilenames()
 * Return 0 if success, -1 if can't remove file or file not from this process
 */
int
removebsy(s_link link)
{ FILE *fd; int rc=0; char adr[ADRS_MAX];
#ifdef __UNIX__
  char buf[6];
#endif
  w_log(LL_FUNC,"removebsy()");

  if( strlen(link.bsyFile)==0 )
    if ( setOutboundFilenames( &link, normal) )
    { w_log(LL_ERR, "Error in setOutboundFilenames(), called from removebsy()" );
      w_log(LL_FUNC,"removebsy() failed, rc=1");
      return 1;
    }

  snprintaddr(adr, sizeof(adr), link.hisAka);
  if( access(link.bsyFile, F_OK) )
  { w_log(LL_ERR, "BSY for %s not exist.", adr);
    rc=-1;    /* file not exists */
  }else if( (fd=fopen(link.bsyFile,"r")) )
  {
#ifdef __UNIX__
     /* Verify PID */
     fgets(buf, sizeof(buf), fd);
     fclose(fd);
     stripCRLF(buf);
     if( atol(buf)==getpid() )
#endif
       delete(link.bsyFile);
#ifdef __UNIX__
     else w_log(LL_ALERT, "removebsy(): some program (PID %s) change .BSY for %s .", buf, adr);
#endif
  }else if( testfile(link.bsyFile) )
  { w_log(LL_ERR, "removebsy(): permitions denied to .BSY for %s .", adr);
  }else
  { w_log(LL_ERR, "removebsy(): Strange file '%s'", link.bsyFile );
    rc=1;
  }

  w_log(LL_FUNC,"removebsy() rc=%d", rc);
  return rc;
}

/* Write outbound file names for link: floFile, bsyFile, pktFile
 * Return 0 if success
 */
int
setOutboundFilenames(s_link *link, e_flavour prio)
{ char *s=NULL, ch, *buf=NULL, *fn=NULL;
  const char netmailchars[]="ocihd";
  const char echomailchars[]="fcihd";
  unsigned short pos;
  e_bundleFileNameStyle bundleNameStyle = eUndef;

  w_log(LL_FUNC,"setOutboundFilenames()");

  nfree(link->pktFile);
  nfree(link->bsyFile);
  nfree(link->floFile);

  w_log(LL_SRCLINE,"setOutboundFilenames():%d",__LINE__);
  if (link->linkBundleNameStyle!=eUndef)
    bundleNameStyle = link->linkBundleNameStyle;
  else if (config->bundleNameStyle!=eUndef)
         bundleNameStyle = config->bundleNameStyle;

  w_log(LL_SRCLINE,"setOutboundFilenames():%d",__LINE__);
  xstrcat( &buf, config->outbound );
  w_log(LL_SRCLINE,"setOutboundFilenames():%d",__LINE__);
  if( (config->addr[0].domain) && (link->hisAka.domain) && strcmp( link->hisAka.domain, config->addr[0].domain )!=0  && bundleNameStyle != eAmiga )
  { /* other domain */
     buf[strlen(buf)-1]='\0';
     s = strrchr( buf, DIRSEP )+1;
     *s = '\0';
     xscatprintf(&buf, "%s.%03x%c", strLower(link->hisAka.domain), link->hisAka.zone, DIRSEP);
  }
  else if (link->hisAka.zone != config->addr[0].zone && bundleNameStyle != eAmiga)
  { /* Same domain */
    buf[strlen(buf)-1]='\0';
    xscatprintf(&buf, ".%03x%c", link->hisAka.zone, DIRSEP);
  }

  w_log(LL_SRCLINE,"setOutboundFilenames():%d",__LINE__);
  if (bundleNameStyle != eAmiga)
    if (link->hisAka.point)
      xscatprintf(&fn, "%04x%04x.pnt%c%08x.",
                   link->hisAka.net, link->hisAka.node, DIRSEP, link->hisAka.point);
    else
      xscatprintf(&fn, "%04x%04x.",
                   link->hisAka.net, link->hisAka.node);
  else
    xscatprintf(&fn, "%u.%u.%u.%u.", link->hisAka.zone,
                   link->hisAka.net, link->hisAka.node, link->hisAka.point);

  w_log(LL_SRCLINE,"setOutboundFilenames():%d",__LINE__);
  xstrcat( &buf, fn );
  pos = strlen(buf);

  switch(prio){
  case normal:    ch=netmailchars[0]; break;
  case crash:     ch=netmailchars[1]; break;
  case immediate: ch=netmailchars[2]; break;
  case hold:      ch=netmailchars[3]; break;
  case direct:    ch=netmailchars[4]; break;
  default:        ch=netmailchars[0];
  }
  xscatprintf( &buf, "%cut", ch );
  link->pktFile = sstrdup(buf);
  if( link->pktFile == NULL )
     w_log(LL_CRIT, "setOutboundFilenames():%s cannot set name to pktFile", __LINE__);

  w_log(LL_SRCLINE,"setOutboundFilenames():%d",__LINE__);
  switch(prio){
  case normal:    ch=echomailchars[0]; break;
  case crash:     ch=echomailchars[1]; break;
  case immediate: ch=echomailchars[2]; break;
  case hold:      ch=echomailchars[3]; break;
  case direct:    ch=echomailchars[4]; break;
  default:        ch=echomailchars[0];
  }
/*  buf[pos]='\0';
  xscatprintf( &buf, "%clo", ch );*/
  sprintf( buf+pos, "%clo", ch );
  link->floFile = sstrdup(buf);
  if( link->floFile == NULL )
     w_log(LL_CRIT, "setOutboundFilenames():%s cannot set name to floFile", __LINE__);

/*  w_log(LL_SRCLINE,"setOutboundFilenames():%d",__LINE__);*/
/*  buf[pos]='\0';
';xstrcat( &buf, "bsy" );*/
  sprintf( buf+pos, "bsy");
  link->bsyFile = sstrdup(buf);
  if( link->bsyFile == NULL )
     w_log(LL_CRIT, "setOutboundFilenames():%s cannot set name to bsyFile", __LINE__);

  snprintaddr(fn, strlen(fn),link->hisAka);
  w_log(LL_FILENAME,"Pktfile for %s: %s", fn, link->pktFile );
  w_log(LL_FILENAME,"Flofile for %s: %s", fn, link->floFile );
  w_log(LL_FILENAME,"Bsyfile for %s: %s", fn, link->bsyFile );

  if( link->bsyFile == NULL || link->floFile==NULL || link->pktFile==NULL )
  { w_log(LL_FUNC,"setOutboundFilenames() rc=1");
    return 1;
  }

  w_log(LL_FUNC,"setOutboundFilenames() OK");
  return 0;
}

/* Check for: file exist & RW-permittions set
 */
int testfile(const char *filename)
{ int rc=-1;

  w_log(LL_FUNC,"testfile()");

  errno=0;
  rc = access( filename, R_OK | W_OK );

  if( rc )
    switch errno{
    case ENOTDIR: w_log(LL_ERR, "Illegal path to file '%s'", filename);
                 break;
    case ENAMETOOLONG: w_log(LL_ERR, "Path part too long '%s'", filename);
                 break;
    case ENOENT: w_log(LL_FILETEST, "File not exist '%s'", filename);
                 break;
#ifdef ELOOP
    case ELOOP: w_log(LL_ERR, "Too many symlinks in file name '%s'", filename);
                 break;
#endif
    case EROFS: w_log(LL_FILETEST, "Read-only file '%s'", filename);
                 break;
#ifdef ETXTBSY
    case ETXTBSY: w_log(LL_FILETEST, "File is busy (sharing violation) '%s'", filename);
                 break;
#endif
    case EACCES: w_log(LL_FILETEST, "Permission denied: '%s'", filename);
                 break;
    case EFAULT: w_log(LL_ERR, "Memory fault (path points to other process memory)");
                 break;
    case EIO: w_log(LL_ERR, "Filesystem error '%s'", filename);
                 break;
    default: w_log( LL_ERR, "Strange error returned by access() for '%s': %s",filename, strerror(errno) );
    }

/*
  if( (rc=access(filename,F_OK))==0 )
  {
    if( (rc=access(filename,R_OK |W_OK)) )
    { if( access(filename,W_OK))
        w_log(LL_ERR, "File %s is read only (permission error), skip file", filename);
      else if( access(filename,R_OK))
        w_log(LL_FILETEST, "File %s is write only, skip filename", filename);
      else if( access(filename,X_OK))
        w_log(LL_FILETEST, "File %s is execute only, skip filename", filename);
      else
        w_log(LL_FILETEST, "File %s is unreachable, skip filename", filename);
    }
  }
  else w_log(LL_FILETEST, "File not exist: %s", filename);
*/
  w_log(LL_FUNC,"testfile() OK");
  return rc;
}

/* Create new file (exclusive sharing mode) with access mode 'AMODE'
 * in file mode 'fmode'
 * Return values:
 * OK:     file descriptor
 * error:  NULL
 */
FILE *createFileExcl(char *filename, const char *fmode)
{ register int h;
  FILE *tempfd;

  if( (h=open(filename, O_CREAT | O_TRUNC | O_WRONLY | O_EXCL | O_EXLOCK, AMODE))
      < 0 )
  { w_log(LL_CRIT, "Cannot create file '%s': %s", filename, strerror(errno) );
    return NULL;
  }
  tempfd = fdopen(h,fmode);
  if( !tempfd )
  { w_log(LL_CRIT, "Cannot reopen file '%s': %s", filename, strerror(errno) );
    return NULL;
  }
  return tempfd;
}

/* Concatenate files
 * Parameters:
 *   path to files (directory name), slash ('/' or '\\') at end is mandatory!
 *   array of directory entries (NULL-terminated)
 *   output filename.
 * Return 0 if success
 */
int concat_files( const char *path, struct dirent **ls, const char *dstfilename)
{
  char *buffer;
  size_t read;
  int i;
  FILE *fin, *fout;
  char *pathname=NULL;

  w_log( LL_FUNC, "concat_files() begin");
  if( !dstfilename || !dstfilename[0] ){
    w_log(LL_ERR, "concat_files(): 'dstfilename' is empty!" );
    w_log( LL_FUNC, "concat_files() failed");
    return -1;
  }
  if( path[strlen(path)-1]==DIRSEP ){
    w_log(LL_ERR, "concat_files(): pathname is not end with slash, sorry. (%s)", path);
    w_log( LL_FUNC, "concat_files() failed");
    return -1;
  }
  if( __fexist(dstfilename) ){
    w_log(LL_ERR, "concat_files(): output file '%s' exist!", dstfilename);
    w_log( LL_FUNC, "concat_files() failed");
    return -1;
  }

  buffer = malloc(COPY_FILE_BUFFER_SIZE);
  if (buffer == NULL) {
    w_log( LL_ERR, "concat_files(): low memory (need %ul bytes)", COPY_FILE_BUFFER_SIZE );
    w_log( LL_FUNC, "concat_files() failed");
    return -1;
  }	
  fout = fopen(dstfilename, "wb");
  if (fout == NULL) {
    w_log( LL_ERR, "concat_files(): can't open output file: %s", strerror(errno) );
    nfree(buffer);
    w_log( LL_FUNC, "concat_files() failed");
    return -1;
  }

  for(i=0; ls[i] && i>=0; i++) {
    xscatprintf( &pathname, "%s%s", path, ls[i]->d_name);

    fin = fopen(pathname, "rb");
    if (fin == NULL) {
      w_log( LL_ERR, "concat_files(): can't open '%s': %s", pathname, strerror(errno));
      nfree(buffer);
      nfree(pathname);
      fclose(fout);
      remove(dstfilename);
      w_log( LL_FUNC, "concat_files() failed");
      return -1;
    }

    w_log( LL_FILE, "Append '%s' to '%s'", dstfilename, pathname);

    while ((read = fread(buffer, 1, COPY_FILE_BUFFER_SIZE, fin)) > 0)
    {
        if (fwrite(buffer, 1, read, fout) != read)
        {
          w_log( LL_ERR, "concat_files(): write to %s error: written less readed", dstfilename);
          nfree(buffer);
          nfree(pathname);
          fclose(fout);
          remove(dstfilename);
          fclose(fin);
          w_log( LL_FUNC, "concat_files() failed");
          return -1;
        }
    }

    if (ferror(fout))
    {
      w_log( LL_ERR, "concat_files(): write to %s error: %s", dstfilename, strerror(errno) );
      nfree(buffer);
      nfree(pathname);
      fclose(fin);
      fclose(fout);
      remove(dstfilename);
      w_log( LL_FUNC, "concat_files() failed");
      return -1;
    }
    if (ferror(fin))
    {
      w_log( LL_ERR, "concat_files(): read from %s error: %s", pathname, strerror(errno) );
      nfree(buffer);
      nfree(pathname);
      fclose(fin);
      fclose(fout);
      remove(dstfilename);
      w_log( LL_FUNC, "concat_files() failed");
      return -1;
    }
    fclose(fin);

  }

  fclose(fout);
  nfree(buffer);
  nfree(pathname);
  w_log( LL_FUNC, "concat_files() OK");
  return 0;
}


/* Move file into inbound, if file exist - change name:
 * if *.pkt then change before point char else increment last chars
 * Return 0 if success
 */
int move_file_to_inbound( const char *from,  const char *to )
{ int rc=-1;
  char *fn=NULL;

  w_log( LL_FUNC, "move_file_to_inbound()");
  rc=move_file(from, to, 0);
  while( rc )
    if( (fn=next_inbound_filename(to)) )
    { rc = move_file(from, fn, 0);
      nfree(fn);
    }else break;
  w_log( LL_FUNC, "move_file_to_inbound() rc=%i", rc);
  return rc;
}

/* Generate next inbound file name
 * if *.pkt then change before point char else increment last chars
 * Return new file name or NULL
 */
char *next_inbound_filename(const char *filename)
{ char *tempname=NULL, *cp=NULL;
  unsigned short counter=1;

  w_log( LL_FUNC, "next_inbound_filename()");
  if(!filename || !(*filename)){
    w_log(LL_ERR, __FILE__ "::next_inbound_filename() FAILED: empty filename!");
    return NULL;
  }
  tempname=strdup(filename);
  if(!tempname){
    w_log(LL_ERR, "Out of memory!");
    return NULL;
  }
  cp = strrchr(tempname, '.'); /* find ext */
  if( cp && cp!=tempname && cp>strrchr(tempname, DIRSEP) )
    do{
      counter++;
      if( stricmp(cp+1, "pkt") || stricmp(cp+1, "tic") || stricmp(cp+1, "zic") )
      {      /* increment chars before point */
        if( cp-tempname>3 && tolower(*(cp-3))=='z' ) /* change prev-3 char */
          while( !strchr( "0123456789_ABCDEFGHIJKLMNOPQRSTUVWXYZ", toupper(++(*(cp-4))) ) );
        if( cp-tempname>2 && tolower(*(cp-2))=='z' ) /* change prev-2 char */
          while( !strchr( "0123456789_ABCDEFGHIJKLMNOPQRSTUVWXYZ", toupper(++(*(cp-3))) ) );
        if( cp-tempname>1 && tolower(*(cp-1))=='z' ) /* change prev char */
          while( !strchr( "0123456789_ABCDEFGHIJKLMNOPQRSTUVWXYZ", toupper(++(*(cp-2))) ) );
        while( !strchr( "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ", ++(*(cp-1)) ) );
      }else{ /* increment last char */
        ++tempname[strlen(tempname)-1];
      }
      w_log( LL_DEBUG, "Trying '%s'...", tempname );
    }while( fexist(tempname) && counter>0 ); /* test sl prevent unlimited cycle **/
  if( counter )
    w_log( LL_FUNC, "next_inbound_filename() OK");
  else{ w_log( LL_FUNC, "next_inbound_filename() FAILED, return NULL");
    nfree(tempname);
  }
  return tempname;
}

#if 0 /* moved to fidoconfig */
/* Case-incensitive string compare, return values eq. strncmp()
 */
int strincmp(const char *str1, const char *str2, unsigned len)
{ unsigned char *temp1, *temp2;
  int res;

  temp1=sstrdup(str1);
  temp2=sstrdup(str2);
  strLower(temp1);
  strLower(temp2);
  res = strncmp( temp1, temp2, len );
  nfree(temp1);
  nfree(temp2);
  return res;
}
#endif

#if 0 /* Moved to fidoconf/common.c */
/*DOC
  Input:  str is a \0-terminated string
          chr contains a list of characters.
  Output: stripTrailingChars returns a pointer to a string
  FZ:     all trailing characters which are in chr are deleted.
          str is changed and returned (not reallocated, simply shorted).
*/
char *stripTrailingChars(char *str, const char *chr)
{
   char *i;

   if( (str != NULL) && strlen(str)>0 ) {
      i = str+strlen(str)-1;
      while( (NULL != strchr(chr, *i)) && (i>=str) )
         *i-- = '\0';
   }
   return str;
}
#endif

/*
char *stripRoundingChars(char *str, const char *chrs)
{ return stripTrailingChars(stripLeadingChars((str),(chrs)),(chrs));
}
*/

/* Create new file into inbound directory, if file exist increment:
 * - base part of filename - tic or pkt,
 * - suffix ("extension") for other.
 */
FILE *createInboundFile(char **filename)
{ FILE *fd=NULL;
  char *tempname=NULL;

  if(!(*filename) || !(*filename)[0] )
  { w_log( LL_ERR, "createInboundFile(NULL): Can't create file with empty name" );
    return NULL;
  }
  else  w_log( LL_FUNC, "createInboundFile(%s)", *filename);

  if( access(*filename, F_OK) ){
    if( !(fd=fopen(*filename,"wb")) )
    { w_log( LL_ERR, "Can't create '%s': %s", *filename, strerror(errno) );
      w_log( LL_FUNC, "createInboundFile() rc=NULL" );
      return NULL;
    }
  }else{
    w_log( LL_INFO, "File '%s' exist...", *filename );

      tempname = next_inbound_filename(*filename);
      if( tempname ){
        if( !(fd=fopen(tempname,"wb")) )
          w_log( LL_ERR, "Can't create '%s': %s", tempname, strerror(errno) );
        else{
          w_log( LL_DEBUG, "Use '%s'", tempname );
          if( strlen(*filename)>=strlen(tempname) ) {
            strcpy(*filename,tempname); nfree(tempname);
          }else{
            free(*filename); *filename = tempname;
          }
        }
      }else  /* sl==0 if all filenames already used */
         w_log(LL_ERROR, "Can't create file: all possible filenames already used");
  }
  w_log( LL_FUNC, "createInboundFile() %s", fd ? "OK": "failed" );
  return fd;
}

/* Move file into inbound directory. If file exist increment:
 * - base part of filename - tic or pkt,
 * - suffix ("extension") for other.
 * Return 0 if success.
 */
int moveInboundFile( const char *srcfname, const char *dstdir )
{ int rc=0;
  FILE*ttfd;
  char *tofname=NULL;

  xstrscat( &tofname, dstdir, basename(srcfname), NULL );
  ttfd = createInboundFile(&tofname); /*to select notused filename*/
  if(!ttfd){
    rc++;
    w_log( LL_ERR, "Can't move %s to %s: can't create destination file: %s", srcfname, tofname, strerror(errno) );
  }else{
    fclose(ttfd);
    if( move_file(srcfname,tofname, 1) )
    { rc++;
      w_log( LL_ERR, "Can't move %s to %s: %s", srcfname, tofname, strerror(errno) );
    }else
    {    w_log( LL_FILE, "File %s moved to %s", srcfname, tofname );
    }
  }
  nfree(tofname);
  return rc;
}

void set_module_vars()
{
   char buff[sizeof(VER_MAJOR)*3+sizeof(VER_MINOR)*3+sizeof(VER_PATCH)*3+3];

   setvar("module", "emailpkt");
   sprintf(buff, "%u.%u.%u", VER_MAJOR, VER_MINOR, VER_PATCH);
   setvar("version", buff);
   SetAppModule(M_EMAILPKT);
}
