/* EMAILPKT: Husky e-mailer
 *
 * (C) Copyleft Stas Degteff 2:5080/102@FIDOnet, g@grumbler.org
 * (c) Copyleft Husky developers team http://husky.sorceforge.net/team.html
 *
 * $Id$
 * Send FTN via email
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
#include <string.h>
#include <stdio.h>  /*FILE operations*/
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <ctype.h>
#ifdef UNIX
#  include <sys/syslimits.h>
#  include <unistd.h>
#else
#  include <limits.h>
#endif

/* husky */
#include <fidoconf/log.h>
#include <fidoconf/temp.h>
#include <fidoconf/xstr.h>

/* ./ */
#include "ecommon.h"

#if defined(__MINGW32__)
#  define random() rand()
#endif

#ifndef nfree
#  define nfree(a) ({ if (a != NULL) { fprintf(stderr,"hesend.c:%d nfree()\n",__LINE__); free(a); a = NULL; } })
#endif

#define TEMPEXT "eml"
#define TEMPFLOEXT "flo"
#define myboundary MIMEBOUNDARY
/*static char *myboundary=MYBOUNDARY;*/

s_fidoconfig *config=NULL;
static int sent = 0, allsent=0;           /* sent files count */

int SAVEFLAG=0;  /* Save processed e-mail messages (for debugging) */
int debugflag=0; /* Execute in debug mode (not call sendmail, not remove files) */

/* Write e-mail messsage with attached file (bundle)
 * Return filename or NULL
 */
char *writeMessage(const char *fullFileName, s_link link)
{ char adr[ADRS_MAX];
  char basefilename[NAME_MAX];
  char *tempfilename=NULL, timebuf[32], *msgidbuf;
  FILE *infd=NULL, *tempfd=NULL;
  char *cp=NULL;
  time_t t=0;
  struct tm *tm=NULL;

  w_log(LL_FUNC,"writeMessage()");

  snprintaddr(adr, sizeof(adr), link.hisAka);

  w_log(LL_FILENAME, "Full file name: '%s'", fullFileName);
  /* input file name */
  if ((infd = fopen(fullFileName, "r")) == NULL) {
      w_log( LL_ERR, "Can't open '%s': %s", fullFileName, strerror(errno));
      return NULL;
  }

  tempfd = createTempFileIn(config->tempOutbound, TEMPEXT, 'b', &tempfilename);
  if( !tempfd ) {
      fclose(infd);
      w_log( LL_ERR, "Can't create temp file in '%s': %s",
                     config->tempOutbound, strerror(errno));
      return NULL;
  }

  /* basename of file */
  strncpy(basefilename, strrchr(fullFileName, DIRSEP)+1, sizeof(basefilename));
  w_log(LL_FILENAME, "Base file name: '%s'", basefilename);

  /* for netmail (.?ut) change ext to pkt */
  if( tolower(basefilename[10]) == 'u' && tolower(basefilename[11]) == 't' )
  { if( (cp = makeUniqueDosFileName( "", "pkt", config))==NULL )
      w_log(LL_CRIT, "Can't generate unique dos file name: memory too low" );
    strncpy(basefilename, cp, NAME_MAX);
    nfree( cp );
    w_log(LL_FILENAME, "Base file name changed to: '%s'", basefilename);
  }

  /* print some headers */
  if( link.emailFrom == NULL )
  {
     fprintf(tempfd, "From: \"%s\" <%s>\n", DEFAULT_FROMNAME, DEFAULT_FPOMADDR);
     w_log(LL_WARN, "Undefined emailFrom for %s, use default: %s",
          adr, DEFAULT_FPOMADDR );
  }
  else
    fprintf(tempfd, "From: \"%s\" <%s>\n", DEFAULT_FROMNAME, link.emailFrom );

  fprintf(tempfd, "To: %s\n", link.email );

  if( link.emailSubj == NULL )
  {
     fprintf(tempfd, "Subject: %s\n", DEFAULT_SUBJECT );
     w_log(LL_WARN, "Undefined emailSubj for %s, use default: %s",
             link.emailSubj, DEFAULT_SUBJECT );
  }
  else fprintf(tempfd, "Subject: %s\n", link.emailSubj );


  time (&t);
  tm = localtime (&t);

  strftime(timebuf, sizeof (timebuf), "%a, %d %b %Y %H:%M:%S %z", tm);
  fprintf(tempfd, "Date: %s\n", timebuf);

  strftime(timebuf, sizeof (timebuf), "%s", tm);
  if( link.emailFrom )
    xscatprintf( &msgidbuf, "%s.%lu.%s", timebuf, random(), link.emailFrom );
  else if( link.ourAka[0].point )
    xscatprintf( &msgidbuf,
                    "%s.%lu@p%u.f%u.n%u.z%u.%s.org\n", timebuf,
                    random(), link.ourAka[0].point, link.ourAka[0].node,
                    link.ourAka[0].net, link.ourAka[0].zone,
                    link.ourAka[0].domain ? link.ourAka[0].domain : "fidonet" );
  else
    xscatprintf( &msgidbuf,
                    "%s.%lu@f%u.n%u.z%u.%s.org\n", timebuf,
                    random(), link.ourAka[0].node,
                    link.ourAka[0].net, link.ourAka[0].zone,
                    link.ourAka[0].domain ? link.ourAka[0].domain : "fidonet" );

  fprintf( tempfd, "Message-ID: <%s>\n", msgidbuf );
  fprintf( tempfd, "X-Mailer: %s %s\n", program_name, version());

  switch( link.emailEncoding ){

  case eeUUE:  /* uuencode file */

      w_log(LL_ENCODE, "Encode method for %s: UUE", adr );

      if( strlen(DEFAULTBODY)>0 )
      {
        fprintf(tempfd, "\n\n%s\n", DEFAULTBODY);
      }

      uuencodeFile(infd, tempfd, basefilename, 0, 0);
      break;

/* IREX internal "protocol" - MIME/base64 encoding */
/*
  case eeIREX:

      fprintf(tempfd, "Mime-Version: 1.0\n");
      fprintf(tempfd, "Content-Type: multipart/mixed; boundary=\"%s\"\n",myboundary);

      fprintf(tempfd, "Ftn-File-ID: %s\n", msgidbuf );
      fprintf(tempfd, "Ftn-Crc32: %X\n", crc32 );
      fprintf(tempfd, "Ftn-Seg: %u-%u\n", segno, seg );
      fprintf(tempfd, "Ftn-Seg-Crc32: %X\n", crc32seg );
      fprintf(tempfd, "Ftn-Seg-ID: %u.%s", segno, msgidbuf );

      if( strlen(DEFAULTBODY)>0 )
      {
        fprintf(tempfd, "\n\n%s\n", DEFAULTBODY);
      }

//This is a MIME encoded message generated by Internet Rex.
//If your mailer doesn't support this type of coding,
//you can decode the file attached using a program like
//'munpack'.  'munpack' is available for anonymous FTP
//from ftp://ftp.andrew.cmu.edu/pub/mpack

*/
  case eeSEAT:  /* SEAT "protocol" - MIME/base64 encoding */
      w_log(LL_ENCODE, "Encode method for %s: SEAT (MIME)", adr );
  case eeMIME:  /* MIME/base64 */
      w_log(LL_ENCODE, "Encode method for %s: MIME", adr );
  default:
      w_log(LL_ENCODE, "Encode method for %s: MIME (default)", adr );

      /* Main section */
      fprintf(tempfd, "Mime-Version: 1.0\n");
      fprintf(tempfd, "Content-Type: multipart/mixed; boundary=\"%s\"\n",myboundary);

      fprintf(tempfd, "\n--%s\n",myboundary);
      fprintf(tempfd, "Content-Type: text/plain");
      if( strlen(DEFAULTBODY)>0 )
      {
        fprintf(tempfd, "\n\n%s\n", DEFAULTBODY);
      }

      /* Attachment section */
      fprintf(tempfd, "\n--%s\n",myboundary);
      fprintf(tempfd, "Content-Type: application/octet-stream; name=\"%s\"\n", basefilename);
      fprintf(tempfd, "Content-Transfer-Encoding: base64\n");
      fprintf(tempfd, "Content-Disposition: attachment; filename=\"%s\"\n\n", basefilename);

      base64encodeFile(infd, tempfd);

      fprintf(tempfd, "\n\n--%s--\n",myboundary);
      break;


  } /* switch */

  fclose(infd);
  fclose(tempfd);

  nfree(tempfilename);
  nfree(msgidbuf);
  w_log(LL_FILENAME, "Return file name: '%s'", tempfilename );
  w_log(LL_FUNC,"writeMessage() OK");
  return sstrdup(tempfilename);
}

/* Send written message
 */
int sendFile(const char *filename, s_link link)
{ char *buf=NULL, *cmd=NULL, *p=NULL;
  int rc;

  w_log(LL_FUNC,"sendFile()");

  /* Contruct sendmail command
     If $a and $f not present use 'sendmailcmd address < file'
  */
  buf = sstrdup( config->sendmailcmd );
w_log(LL_DEBUGS,"%s::sendFile():%u Command construct: '%s'", __FILE__, __LINE__, buf);
  p=strstr(buf, "$a");
  if( p ){
    *p++ = '%'; *p='s';
w_log(LL_DEBUGS,"%s::sendFile():%u Command construct: '%s'", __FILE__, __LINE__, buf);
    xscatprintf( &cmd, buf, link.email);
  }
  else
    xscatprintf( &cmd, "%s %s", buf, link.email);
  nfree(buf);

  buf = sstrdup( cmd );
  p=strstr(buf, "$f");
  if( p ){
    *p++ = '%'; *p='s';
w_log(LL_DEBUGS,"%s::sendFile():%u Command construct: '%s'", __FILE__, __LINE__, buf);
    xscatprintf( &cmd, buf, filename);
  }
  else
    xscatprintf( &cmd, "%s < %s", buf, filename);
  nfree(buf);

  w_log(LL_EXEC,"Execute '%s'",cmd);
  if( debugflag ){
    w_log(LL_ERR, "(debug) Cmd line: '%s'", cmd);
  }else
    if( (rc = system(cmd)) != 0)
    {
      w_log(LL_ERR, "Calling error (rc=%d)", rc );
      w_log(LL_ERR, "(cmd line: '%s')", cmd);
      nfree(cmd);
      w_log(LL_FUNC,"sendFile() failed");
      return rc;
    }

  w_log(LL_FILESENT, "Sent '%s' to %s (%s)", filename, link.email,
          snprintaddr(cmd, strlen(buf), link.hisAka)
     );

  /* todo: save to confirmation */
  if( debugflag && !SAVEFLAG )
     w_log(LL_DEL, "(debug) Remove '%s'",filename);
  else if (!SAVEFLAG)
     delete(filename);
  else
     w_log(LL_DEL, "(Save flag is set) Keep '%s'",filename);

  nfree(cmd);
  w_log(LL_FUNC,"sendFile() OK");
  return 0;
}


/* Process outbound netmail for link (.?ut): scan BSO & send ?ut for this link
 */
int processNet(s_link link)
{ char adr[ADRS_MAX], *messagefile=NULL;
  int lenvar=0, rc=0, RC=0, ch=0;
  char flavorCharsUse[] = "ocid";   /* flavores for send netmail (hold not sent) */

  w_log(LL_FUNC,"processNet()");
  adr[0]=0;

  if( (lenvar = strlen(link.pktFile)-3) <0 )
  { w_log(LL_ERR, "Illegal pktFile for link %s (%s)",
          snprintaddr(adr, sizeof(adr), link.hisAka), link.pktFile );
    return 1;
  }

  for( ch=strlen(flavorCharsUse)-1; ch >=0; ch--)
  {
    link.pktFile[lenvar]=flavorCharsUse[ch];

    if ( testfile(link.pktFile) )
      continue;
    w_log(LL_FILENAME, "pktFile '%s'", link.pktFile);

    if( (messagefile=writeMessage(link.pktFile, link)) == NULL)
    { w_log(LL_ERR, "Skip on compose error: '%s'", link.pktFile );
      RC++;
      continue;
    }
    if( (rc = sendFile(messagefile,link)) !=0 )
    { w_log(LL_ERR, "Skip on send error: '%s'", link.pktFile );
      RC++;
      continue;
    }
    sent++;
    w_log(LL_SENT, "Sent '%s' to %s (%s)\n", link.pktFile, link.email,
            snprintaddr(messagefile, strlen(messagefile) ,link.hisAka)
       );

    if( debugflag )
        w_log(LL_ERR, "(debug) Remove '%s'",link.floFile);
    else if( (rc=delete(link.pktFile)) )
    {    w_log(LL_ERR, "File '%s' not removed: %s", link.pktFile, strerror(rc));
         RC++;
    }
    else
        w_log(LL_DEL, "Removed '%s'", link.pktFile);

  }
  w_log(LL_FUNC,"processNet() OK");
  return rc;
}

/* Process outbound echomail for link (.?lo): scan outbound for ?lo, read its
 * and send files listed in.
 */
int processFlo(s_link link)
{ FILE *flofd=NULL, *tempflofd=NULL;
  char buf[PATH_MAX], filetosend[PATH_MAX], *messagefile=NULL;
  char *tempfloname=NULL;
  int ch=0, replacefloflag=0, delflag = 0, truncflag = 0, lenvar = 0, rc=0;
  char flavorCharsUse[] = "fcid";   /* flavores for send echomail */

  w_log(LL_FUNC,"processFlo()");

  if( (lenvar = strlen(link.floFile)-3) <0 )
  { w_log(LL_ERR, "Illegal floFile for link %s (%s)",
          snprintaddr(buf, sizeof(buf), link.hisAka), link.floFile );
    return 1;
  }

  /* scan directory for ?lo files */
  for( ch=strlen(flavorCharsUse)-1; ch >=0; ch--)
  {
    link.floFile[lenvar]=flavorCharsUse[ch];

    if( testfile(link.floFile) )
      continue;

    if( (flofd = fopen(link.floFile, "rt")) == NULL)
    {  w_log(LL_ERR, "Cannot open '%s': %s", link.floFile, strerror(errno));
       continue;
    }

    if( NULL==(tempflofd = createTempFileIn(config->tempOutbound, TEMPFLOEXT, 't', &tempfloname)) )
    { w_log(LL_ERR, "Can't create temporary .FLO file");
      return 1;
    }
    w_log(LL_FILE,"Process '%s'",link.floFile);
    replacefloflag=0;
    /* read lines from flo file */
    while (fgets(buf, sizeof(buf), flofd) != NULL)
    { delflag = 0;
      truncflag = 0;
      if (buf[0] == '^')
      {
        delflag = 1;
        strncpy(filetosend, buf+1, sizeof(filetosend));
        if( strlen(filetosend)!=strlen(buf+1) )
        { w_log(LL_ERR,"processFlo(): Too long filename '%s' in '%s', can't send", buf+1, link.floFile);
          replacefloflag=1;
          fprintf(tempflofd, "%s\r\n", buf);
          continue;
        }
      }else if (buf[0] == '#')
      {
        truncflag = 1;
        strncpy(filetosend, buf+1, sizeof(filetosend));
        if( strlen(filetosend)!=strlen(buf+1) )
        { w_log(LL_ERR,"processFlo(): Too long filename '%s' in '%s', can't send", buf+1, link.floFile);
          replacefloflag=1;
          fprintf(tempflofd, "%s\r\n", buf);
          continue;
        }
      }else
      { strncpy(filetosend, buf, sizeof(filetosend));
        if( strlen(filetosend)!=strlen(buf) )
        { w_log(LL_ERR,"processFlo(): Too long filename '%s' in '%s', can't send", buf, link.floFile);
          replacefloflag=1;
          fprintf(tempflofd, "%s\r\n", buf);
          continue;
        }
      }

/*        stripLeadingChars(filetosend,"\r\n");*/   /* remove the trailing \r & \n */
      stripCRLF(filetosend);
      if(strlen(filetosend)==0)
        continue;

      /* Next block: don't remove file from .flo if file not exist */
      if( testfile(filetosend) )
      { w_log(LL_FLO, "File not exist! '%s'",filetosend);
        replacefloflag=1;
        if(delflag) fprintf(tempflofd, "^");
        else if(truncflag) fprintf(tempflofd, "#");
        fprintf( tempflofd, "%s\r\n", filetosend );
        continue;
      }
      w_log(LL_FILE, "Bundle '%s'", filetosend);

      nfree(messagefile);
      if( (messagefile=writeMessage(filetosend, link)) == NULL)
      { w_log(LL_ERR, "Skip on compose error: '%s'", buf );
        replacefloflag=1;
        if(delflag) fprintf(tempflofd, "^");
        else if(truncflag) fprintf(tempflofd, "#");
        fprintf( tempflofd, "%s\r\n", filetosend );
        continue;
      }
      if( (rc = sendFile(messagefile,link)) !=0 )
      { w_log(LL_ERR, "Skip on send error: '%s'", buf );
        nfree(messagefile);
        replacefloflag=1;
        if(delflag) fprintf(tempflofd, "^");
        else if(truncflag) fprintf(tempflofd, "#");
        fprintf( tempflofd, "%s\r\n", filetosend );
        continue;
      }
      sent++;
      w_log(LL_SENT, "Sent '%s' to %s (%s)\n", filetosend, link.email,
              snprintaddr(messagefile, strlen(messagefile) ,link.hisAka)
         );

      if( debugflag && delflag )
          w_log(LL_ERR, "(debug) Delete file: '%s'", filetosend);
      else if( delflag )
      {
        if( (rc=delete(filetosend)) )
          w_log(LL_ERR, "Can't remove file '%s': %s", filetosend, strerror(rc));
        else
          w_log(LL_DEL, "Removed '%s'", filetosend);
      }
      else if( debugflag && truncflag )
        w_log(LL_ERR, "(debug) Truncate file: '%s'", filetosend);
      else if( truncflag )
      {
        if( (rc=truncat(filetosend)) )
          w_log(LL_ERR, "Can't truncate file '%s': %s", filetosend, strerror(errno));
        else
          w_log(LL_TRUNC, "Truncated '%s'", filetosend);
      }

      nfree(messagefile);
    } /* while */
    fclose(flofd);
    fclose(tempflofd);
    tempflofd=flofd=NULL;

    if( debugflag ){
      if( replacefloflag )
         w_log(LL_DEL, "Replace '%s'",link.floFile);
      else w_log(LL_ERR, "(debug) Remove '%s'",link.floFile);
    }
    else
    { delete(link.floFile);
      if(replacefloflag)
      {  w_log(LL_DEL, "Replace '%s'",link.floFile);
         if( move_file(tempfloname, link.floFile, 1) )
         w_log(LL_FILE, "Replaced '%s' with '%s'",link.floFile, tempfloname);
      }else
         delete( tempfloname );
    }

  }/*for*/
  if(tempflofd)
  { fclose(tempflofd);
    delete(tempfloname);
  }
  nfree(tempfloname);
  w_log(LL_FUNC, "processFlo() OK");
  return 0;
}


/* Process outbound filebox for link (.?lo): scan & send all files
 */
int processBox(s_link link)
{   DIR *boxP;
    struct dirent * dp;
    char path[PATH_MAX], adr[30], *messagefile;
    unsigned pathlen=0;
    int rc;

    w_log(LL_FUNC,"processBox()");
    w_log(LL_FILENAME, "Filebox: %s", link.fileBox);
    snprintaddr(adr,sizeof(adr),link.hisAka);
    if ( link.fileBox==NULL || (pathlen=strlen( link.fileBox ))==0 )
    { w_log(LL_BOX, "Not defined filebox for %s", adr );
      return 0;
    }

    if ( (boxP = opendir(link.fileBox))==NULL )
    { w_log(LL_ERR, "Can't open filebox for %s: %s", adr, link.fileBox );
      return 1;
    }
    w_log(LL_LINK, "Process filebox for %s (%s)", adr, link.fileBox );
    strncpy( path, link.fileBox, sizeof(path) );
    while( (dp = readdir(boxP)) != NULL ){
	path[pathlen]=0;
#ifdef DT_REG
/* struct direntry have member d_type in *NIX */
        if( dp->d_type != DT_REG)
        { w_log(LL_FILE, "%s is not regular file", dp->d_name );
           continue; /* Send only regular files! */
        }
#endif
	strncat(path,dp->d_name, sizeof(path));
        if( testfile(path) )
	  continue;
	w_log(LL_FILE, "Sending file %s to %s", path, adr);

        if( (messagefile=writeMessage(path, link)) == NULL)
        { w_log(LL_ERR, "Composing error: '%s', skip", path );
	  continue;
	}
        if( (rc = sendFile(messagefile,link)) !=0 )
        { w_log(LL_ERR, "Sending error: '%s', skip", path );
	  continue;
	}
        sent++;
        w_log(LL_SENT, "Sent %s to %s (%s)\n",
                      path, link.email, adr, link.hisAka
           );

        if( debugflag ){
          w_log(LL_ERR, "(debug) Delete file: %s", path);
        }else
          if( (rc=delete(path)) )
            w_log(LL_ERR, "File %s not removed: %s", path, strerror(rc));
    }
    (void)closedir(boxP);

  w_log(LL_FUNC,"processBox() OK");
  return 0;
}	

/* Do send files.
 *
 */
int send(void)
{ FILE *bsyfd;
  int i, rc=0;
  char adr[30];

  w_log(LL_FUNC,"send()");
  adr[0]=0;

  for (i = 0; i < config->linkCount; i++)
  {
      if(   config->links[i].email != NULL
        &&  setOutboundFilenames( &(config->links[i]), normal )==0 )
      {
          sent=0;
          snprintaddr(adr,sizeof(adr),config->links[i].hisAka);
          w_log(LL_LINK, "Process link %s", adr);

          if( (bsyfd = createbsy(config->links[i]))==NULL )
            continue;
          if( (rc += processNet(config->links[i])) )
          {
            w_log(LL_ERR,"Netmail send error.\n");
          }
          rc *= 10;
          if( (rc = processFlo(config->links[i])) )
          {
            w_log(LL_ERR,"Echomail send error.\n");
          }
          rc *= 10;
          if( (rc += processBox(config->links[i])) )
          {
            w_log(LL_ERR,"Filebox send error.\n");
          }
          fclose(bsyfd);
          if(!delete(config->links[i].bsyFile))
            w_log(LL_DEL, "Deleted bsy for %s", adr);
          else
            w_log(LL_ERR, "Can't delete bsy for %s: %s", adr, strerror(errno));

          if( sent )
          {
            allsent += sent;
            w_log(LL_INFO, "Sent %d files to %s", sent, adr );
          }
          else
            w_log(LL_INFO, "Nothing to send for %s", adr );

      }/*if*/
  }/*for*/

  w_log(LL_FUNC,"send() OK");
  return rc;
}


void
printver()
{ fprintf( stderr,	/* ITS4: ignore */
           "hesend v%s - send FTN over email\n(part of the EmailPkt package, The Husky FidoSoft Project module) \n\n",
           version() );
}

void
usage()
{ printver();
  fprintf( stderr,	/* ITS4: ignore */
/*           "Usage: %s [-qVD] [-c configfile]\n", program_name); */
           "Usage: hesend [-qVD] [-c configfile]\n");
#ifndef UNIX
  fprintf( stderr,	/* ITS4: ignore */
/*           "or:    %s [--help] [--version] [--debug] [--quiet] [--config=configfile]\n", program_name);*/
           "or:    hesend [--help] [--version] [--debug] [--quiet] [--config=configfile]\n");
#endif
  exit(-1);
}

int
main( int argc, char **argv)
{ int rc=0, op=0, quiet=0;
  char *cp=NULL;

  if ((cp = strrchr(argv[0], '/')) != NULL)
  {  program_name = sstrdup(cp + 1);
     cp=NULL;
  }
  else
    program_name = sstrdup(argv[0]);

#ifdef UNIX
  opterr = 0;
  while ((op = getopt(argc, argv, "DVc:d:hq")) != EOF)
    switch (op) {
    case 'c': /* config file */
              cp=sstrdup(optarg);
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

#else
   for( op=1; op<argc; op++ ){
     if(argv[op][0]=='-'){
       if(argv[op][1]=='-'){
         if(strcmp(argv[op]+2,"quiet"))
                quiet=0;
         if(strcmp(argv[op]+2,"debug"))
                debugflag=1;
         if( strstr(argv[op]+2,"config=")==(argv[op]+2) ) {
             cp = strchr(argv[op], '=');
             if( cp ) cp = sstrdup(++cp);
             else fprintf(stderr, "Strange error: --config=... not contents '='");
         }
         if(strcmp(argv[op]+2,"version")) {
                printver();
                nfree(program_name);
                return 0;
         }
         if(strcmp(argv[op]+2,"help")) {
                usage();
                nfree(program_name);
                return 0;
         }
       }else{
         if(strchr(argv[op],'q'))
                quiet=0;
         if(strchr(argv[op],'D'))
                debugflag=1;
         if(strchr(argv[op],'c')) {
                cp = sstrdup(argv[++op]); /* config-file */
         }
         if(strchr(argv[op],'V')) {
                printver();
                nfree(program_name);
                return 0;
         }
         if(strchr(argv[op],'h')) {
                usage();
                nfree(program_name);
                return 0;
         }
       }
     }else{
        fprintf(stderr,"Illegal parameter: %s", argv[op]);
        nfree(program_name);
        return 1;
     }
   }
#endif

  config = readConfig(cp);
  nfree(cp);
  if(quiet) config->logEchoToScreen=0;

  if( openLog( LOGFILE, program_name, config) )
  { fprintf(stderr, "Can't init log()! Aborting\n");
    nfree(cp);
  }
  else
  {
    if( debugflag )
      w_log(LL_PRG,"Start %s %s (debug mode)", program_name, version());
    else w_log(LL_PRG,"Start %s %s", program_name, version());

    rc = send();
    w_log(LL_SUMMARY, "Summary sent %d files", allsent);

    disposeConfig(config);
    w_log(LL_PRG,"Stop %s", program_name);
    nfree(program_name);
    closeLog();
  }
  return rc;
}
