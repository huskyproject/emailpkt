/* EMAILPKT: Husky e-mailer
 *
 * (C) Copyleft Stas Degteff 2:5080/102@FIDOnet, g@grumbler.org
 * Based on code by German Theler <german@linuxfreak.com> 4:905/210
 *
 * $Id$
 * Common declaraions
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>

#include <fidoconf/log.h>
#include "ecommon.h"


char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";


#define BUFSIZE 1024
#define STRINGSIZE 255

/* uu-codec */
#define UUDEC(n) (n - ' ') & 077
#define UUENC(n) (n != 0) ? (((n) & 077) + ' ') : '`'
#define UUBUFSIZE 45    /* length of the data chunk for uuencode to one line */


/* Decode base64-encoded string (store pointer into dstbuffer)
 * Return length of decoded data or -1 (if error)
 */
int base64DecodeLine(unsigned char **dstbuffer, const unsigned char * line)
{ const unsigned char *ip, *ep; /* input & end pointers */
  unsigned char *op;            /* output pointer */
  unsigned char *temp=NULL;
  int  len=0, error=0;
  register int a=-1, b=-1, c=-1, d=-1;
  register unsigned bytes=0;  /* length of decoded data */

  len = strlen(line) & ~3; /* cut to n*4 */
  ep=line+len;
  temp=smalloc((int)(len*0.75)+2);
#ifdef DEBUG
w_log(LL_DEBUGZ, "Process %s", line);
#endif
  for( ip=line, op=temp ; ip<ep; )
  {  a = strchr(base64, *ip) - base64;
     if(a<0) error++;
     if(a<0) break;
     ip++;
     b = strchr(base64, *ip) - base64;
     if(b<0){ b=0; c=64; error++; }  /* Not an base64 char */
     else
     { ip++;
       c = strchr(base64, *ip) - base64;
       if(c<0){ c=64; error++; }     /* Not an base64 char */
       else
       { ip++;
         d = strchr(base64, *ip) - base64;
         if(d<0){ d=64; error++; }   /* Not an base64 char */
         else ip++;
       }
     }
     *op++ = (((a << 6) | b) >> 4) & 0xFF;
     bytes++;
     if( !(c&64) ) {
        *op++ = (((((a << 6) | b) << 6) | c) >> 2) & 0xFF;
        bytes++;
     }
     else break;
     if( !(d&64) ) {
        *op++ = ((((((a << 6) | b) << 6) | c) << 6) | d) & 0xFF;
        bytes++;
     }
     else break;
  }
  if( error ){
    w_log(LL_ERR,"base64DecodeLine(): Not an base64 char, break decoding.");
    if( !bytes ) return -1;
  }
  else if ( !*ep ) /* line cutted */
  *op='\0';
  *dstbuffer = temp;

  return bytes;
}

/* Decode base64-encoded file
 * Return values:
 * OK:                    0
 * Can't create file:    -1  (createFileExcl() display error)
 * Unexpected EOF:        1  display error
 * Can't read from file: -2  display error
 */
int base64decodeFile(char *filename, FILE *in)
{ FILE *out;
  unsigned char buffer[STRINGSIZE];
  int i, a, b, c, d, x, y, z;

  w_log(LL_FUNC,"base64decodefile()");

  out = createFileExcl(filename, "wb");
  if( !out )
    return -1;

  while ((i = getc(in)) != '\n') {
        ungetc(i, in);
        buffer[0]=0;
        if( fgets(buffer, STRINGSIZE, in)==NULL )
        {  if( buffer[0]==0 )
          { w_log(LL_ERR,"base64decodefile():%u Unexpected EOF (error %d)", __LINE__-2, errno);
            fclose(out);
            return 1;
          }
          else
          { w_log(LL_ERR,"base64decodefile():%u Can't read from file (error %d)", __LINE__-5, errno);
            fclose(out);
            return -2;
          }
        }

        for (i = 0; (buffer[i] != '\n') && (buffer[i] != '\0'); i += 4)
        {   a = strchr(base64, buffer[i]) - base64;
            b = strchr(base64, buffer[i+1]) - base64;
            c = strchr(base64, buffer[i+2]) - base64;
            d = strchr(base64, buffer[i+3]) - base64;

            x = (((a << 6) | b) >> 4) & 0xFF;
            y = (((((a << 6) | b) << 6) | c) >> 2) & 0xFF;
            z = ((((((a << 6) | b) << 6) | c) << 6) | d) & 0xFF;

            fputc(x, out);

            if (c != 64)
                fputc(y, out);
	    else
	    {   fclose(out);
		return 0;
	    }

            if (d != 64)
                fputc(z, out);
	    else
            {   fclose(out);
		return 0;
	    }
        }
    }
    fclose(out);

  w_log(LL_FUNC,"base64decodefile() OK");
  return 0;
}

/* Decode quoted-printeble line (store into dstbuffer)
 * Return length of decoded data or -1 (if error)
 */
int quotedPrintableDecodeLine(unsigned char **dstbuffer, const unsigned char *line)
{ unsigned char hex[3]={0,0,0}; /* hex2char convertion buffer */
  unsigned char *ip, *op;  /* pointers to input & output chars in sequence */
  int bytes=0;
  unsigned char *temp=sstrdup(line);

  ip=op=temp;

  for( ; *ip; ip++, op++)
  { if (*ip == '=' && isxdigit(*(ip+1)) )  /* decode =XX */
    { ip++;
      hex[0] = *ip++;   /* ip now ip+=2 */
      if( *ip && isxdigit(*ip) )
      { hex[1] =  *ip;
        *op = (char)strtol(hex, (char **)NULL, 16);
        bytes++;
        continue;
      }
      else
        ip -= 2; /* return ip to '=' sign */
    }
    *op=*ip;
    bytes++;
  }
  *op='\0';
fprintf(stderr, "bytes=%u, op-temp=%u\n", bytes, op-temp );
  realloc(temp, bytes);
  *dstbuffer = temp;

  return bytes;
}


/* base64-encoder for file
 * Return values:
 * OK: 0
 * error: errno
 */
int base64encodeFile(FILE *infd, FILE *outfd)
{
  int a, b, c, d, x, y, z;
  int counter = 0;

  w_log(LL_FUNC,"base64encodeFile()");

  while ((x = getc(infd)) != EOF) {
      if (counter >= 64) {
          fprintf(outfd, "\n");
          counter = 4;
      } else
          counter += 4;

      if ((y = getc(infd)) == EOF)
      {   a = x >> 2;
          b = x << 4 & 0x3F;
          putc(base64[a], outfd);
          putc(base64[b], outfd);
          putc('=', outfd);
          putc('=', outfd);

          fprintf(outfd, "\n");

          break;
      }

      if ((z = getc(infd)) == EOF)
      {   a = x >> 2;
          b = (x << 4 | y >> 4) & 0x3F;
          c = y << 2 & 0x3F;

          putc(base64[a], outfd);
          putc(base64[b], outfd);
          putc(base64[c], outfd);
          putc('=', outfd);

          fprintf(outfd, "\n");

          break;
      }

      a = x >> 2;
      b = (x << 4 | y >> 4) & 0x3F;
      c = (y << 2 | z >> 6) & 0x3F;
      d = z & 0x3F;

      putc(base64[a], outfd);
      putc(base64[b], outfd);
      putc(base64[c], outfd);
      putc(base64[d], outfd);
  }
  fprintf(outfd, "\n");
  if( ferror(infd) )
  { w_log(LL_ERR, "base64encodeFile(): File read error: %s", strerror(errno));
    return errno;
  }

  w_log(LL_FUNC,"base64encodeFile() OK");
  return 0;
}

/* uu-encoder for file
 * Return values:
 * OK: 0
 * error: errno
 */
/* todo: CRC, multisection */
int uuencodeFile( FILE *infd, FILE *outfd, const char*filename,
              const unsigned section, const unsigned sectsize)
{
  int i, a, b, c, d, j = 0;
  unsigned char buf[UUBUFSIZE];

  w_log(LL_FUNC,"uuencodeFile()");

  switch( section ){
   case 1: fprintf(outfd, "section 1 of file %s\n\nbegin 664 %s\n", filename, filename);
   case 0: fprintf(outfd, "\n\nbegin 664 %s\n", filename);
           break;
  default: fprintf(outfd, "section %d of file %s\n\n", section, filename);
  }

  do{ /* uuencode loop */
    memset(buf, 0, sizeof(buf));

    /* Read 45 bytes from input file */
    j=fread(buf, 1, sizeof(buf), infd);

    /* write line length */
    fputc(UUENC(j), outfd);

    /* encode data & write line to ooutput stream */
    for (i = 0; i < j; i += 3)
    {   a = buf[i] >> 2;
        b = ((buf[i] << 4) & 060) | ((buf[i+1] >> 4) & 017);
        c = ((buf[i+1] << 2) & 074) | ((buf[i+2] >> 6) & 03);
        d = buf[i+2] & 077;

        fputc(UUENC(a), outfd);
        fputc(UUENC(b), outfd);
        fputc(UUENC(c), outfd);
        fputc(UUENC(d), outfd);
    }
    fprintf(outfd, "\n");

  }while( j && !feof(infd) && !ferror(infd) );

  if( feof(infd) ) fprintf(outfd, "end\n");
  if( ferror(infd) )
  { w_log(LL_ERR, "uuencodeFile(): File read error: %s", strerror(errno));
    return errno;
  }

  w_log(LL_FUNC,"uuencodeFile() OK");
  return 0;
}

/* Decode uu-string and store into dstbuffer
 * Return decoded array len
 */
unsigned uudecodeLine( unsigned char **dstbuffer, const unsigned char *line )
{  unsigned char *temp=NULL;
   register unsigned char a, b, c, d, *ip, *op;
   register int len;

   nfree(*dstbuffer);   /* not let zero for prevent memory leak */
   if( !sstrlen(line) )
     return 0;

   ip = (unsigned char *)line;
   len = UUDEC(*(ip++));    /* encoded line length */
   if(len>0){
     op = temp = smalloc( ((len+3) >> 2)*3 );  /* round(3/4*len) */
     while( len>0 )
     {  if(*ip) a = UUDEC( *(ip++) ); else break;
        if(*ip) b = UUDEC( *(ip++) ); else break;
        if(*ip) c = UUDEC( *(ip++) ); else break;
        if(*ip) d = UUDEC( *(ip++) ); else break;
        *(op++)=(a<<2)|(b>>4); len--;
        if(len<=0) break;
        *(op++)=(b<<4)|(c>>2); len--;
        if(len<=0) break;
        *(op++)=(c<<6)|(d);    len--;
     }
//     op--;
     len = op - temp;
     realloc( temp, len);
     *dstbuffer = temp;
   }

   return len;
}

/* uudecode file
 * Return values:
 * OK:               0
 * Can't open file: -1
 */
int uudecodeFile(char *name, FILE *from)
{
    FILE *to;
    unsigned char buf[STRINGSIZE];
    int i;
    int a, b, c, d;

  w_log(LL_FUNC,"uudecodeFile()");

    if ((to = fopen(name, "wb")) == NULL)
        return -1;

    while ((i = UUDEC(fgetc(from))) != 0)
    {   while (i > 0)
        {   a = UUDEC(fgetc(from));
            b = UUDEC(fgetc(from));
            c = UUDEC(fgetc(from));
            d = UUDEC(fgetc(from));

            if (i-- > 0)
                fputc((char)(a<<2)|(b>>4), to);
            if (i-- > 0)
                fputc((char)(b<<4)|(c>>2), to);
            if (i-- > 0)
                fputc((char)(c<<6)|(d), to);
        }
        fgets(buf, STRINGSIZE, from);
    }
    fclose(to);

  w_log(LL_FUNC,"uudecodeFile() OK");
  return 0;
}

