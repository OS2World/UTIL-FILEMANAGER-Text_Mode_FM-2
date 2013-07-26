#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_VIO
#define INCL_KBD

#include <os2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>
#include "fmt.h"

#pragma alloc_text(VIEWER,LoadFile,ShowBorders,ShowLine,ShowPage)
#pragma alloc_text(VIEWER,PrevPage,PrevLine,NextLine,NextPage,ViewFile)
#pragma alloc_text(VIEWER,index,literal,memstr,RestartFile)

extern void ThreadMsg (char *str);

static char *foundstr,find[162];
ULONG        findlen;


char *memstr (register char *t,char *s,ULONG lent,ULONG lens,BOOL ins) {

  /* find s in t */

  char          *ot = t + lent,*os = s + lens;
  register char *t1,*s1;
  int            cmp;

  while(t < ot) {
    t1 = t;
    s1 = s;
    while(s1 < os) {
      cmp = (ins) ? (toupper(*s1) != toupper(*t)) : (*s1 != *t);
      if(cmp)
        break;
      else {
        s1++;
        t++;
      }
    }
    if(s1 == os)
      return t1;
    t = t1 + 1;
  }
  return NULL;
}


int index (const char *s,const char c) {

  char *p;

  p = strchr(s,c);
  if(p == NULL || !*p)
    return -1;
  return (int)(p - s);
}


/*
 * literal.c
 * Translate a string with tokens in it according to several conventions:
 * 1.  \x1b translates to char(0x1b)
 * 2.  \27  translates to char(27)
 * 3.  \"   translates to "
 * 4.  \'   translates to '
 * 5.  \\   translates to \
 * 6.  \r   translates to carriage return
 * 7.  \n   translates to linefeed
 * 8.  \b   translates to backspace
 * 9.  \t   translates to tab
 * 10. \a   translates to bell
 * 11. \f   translates to formfeed
 *
 * Synopsis
 *    *s = "this\x20is\32a test of \\MSC\\CSM\7"
 *    literal(s);
 *
 *    ( s now equals "this is a test of \MSC\CSM")
 */

#define HEX "0123456789ABCDEF"
#define DEC "0123456789"

int literal (char *fsource) {

  register int wpos,w;
  int          len,oldw;
  char        *fdestin,*freeme,wchar;

  if(!fsource || !*fsource)
    return 0;
  len = strlen(fsource) + 1;
  freeme = fdestin = malloc(len + 1);
  memset(fdestin,0,len);              /* start out with zero'd string */

  w = 0;                              /* set index to first character */
  while(fsource[w]) {                 /* until end of string */
    switch(fsource[w]) {
      case '\\':
        switch(fsource[w + 1]) {
          case 'x' :                  /* hexadecimal */
            wchar = 0;
            w += 2;                   /* get past "\x" */
            if(index(HEX,(char)toupper(fsource[w])) != -1) {
              oldw = w;
              while(((wpos = index(HEX,(char)toupper(fsource[w]))) != -1) &&
                    w < oldw + 2) {
                wchar = (char)(wchar << 4) + (char)wpos;
                w++;
              }
            }
            else
              wchar = 'x';            /* just an x */
            w--;
            *fdestin++ = wchar;
            break;

          case '\\' :                 /* we want a "\" */
            w++;
            *fdestin++ = '\\';
            break;

          case 't' :                  /* tab char */
            w++;
            *fdestin++ = '\t';
            break;

          case 'n' :                  /* new line */
            w++;
            *fdestin++ = '\n';
            break;

          case 'r' :                  /* carr return */
            w++;
            *fdestin++ = '\r';
            break;

          case 'b' :                  /* back space */
            w++;
            *fdestin++ = '\b';
            break;

          case 'f':                   /* formfeed */
            w++;
            *fdestin++ = '\x0c';
            break;

          case 'a':                   /* bell */
            w++;
            *fdestin++ = '\07';
            break;

          case '\'' :                 /* single quote */
            w++;
            *fdestin++ = '\'';
            break;

          case '\"' :                 /* double quote */
            w++;
            *fdestin++ = '\"';
            break;

          default :                   /* decimal */
            w++;                      /* get past "\" */
            wchar = 0;
            if(index(DEC,fsource[w]) != -1) {
              oldw = w;
              do {                    /* cvt to binary */
                wchar = (char)(wchar * 10 + (fsource[w++] - 48));
              } while (index(DEC,fsource[w]) != -1 && w < oldw + 3);
              w--;
            }
            else
              wchar = fsource[w];
            *fdestin ++ = wchar;
            break;
        }
        break;

      default :
        *fdestin++ = fsource[w];
        break;
   }
   w++;
  }
  *fdestin = 0;                               /* terminate the string */

  len = fdestin - freeme;
  memcpy(fsource,freeme,len + 1);             /* swap 'em */
  free(freeme);

  return len;                                 /* return length of string */
}


ULONG LoadFile (char *filename,char **text,BOOL *hexmode) {

  FILE *fp;
  ULONG len = 0;
  char *p;

  *hexmode = FALSE;
  fp = fopen(filename,"rb");
  if(fp) {
    fseek(fp,0,SEEK_END);
    len = ftell(fp);
    fseek(fp,0,SEEK_SET);
    if(len) {
      *text = malloc(len + 1);
      if(*text) {
        if(fread(*text,1,len,fp) != len) {
          free(*text);
          *text = NULL;
          len = 0;
        }
        else {
          p = *text;
          while(p - *text < min(len,128)) {
            if(*p < ' ' && *p != '\r' && *p != '\n' && *p != '\t' &&
               *p != '\x1b' && *p != '\x1a' && *p != '\07' &&
               *p != '\x0c') {
              *hexmode = TRUE;
              break;
            }
            p++;
          }
        }
      }
      else
        len = 0;
    }
    fclose(fp);
  }
  return len;
}


void ShowBorders (char *filename,ULONG len,BOOL hexmode) {

  int   attr = ((7 << 4) << 8) | ' ';
  char  str[161] = "Arrow/page keys, F/N/B = find, H = hexmode, P = print, Esc = exit";
  ULONG xlen = 0;

  VioWrtNCell((char *)&attr,vio.col,0,0,0);
  VioWrtNCell((char *)&attr,vio.col,vio.row - 2,0,0);
  WriteCenterString(str,vio.row - 2,0,vio.col);
  if(len) {
    if(len > 1024) {
      CommaFormat(str,sizeof(str),len / 1024);
      strcat(str,"k");
    }
    else {
      CommaFormat(str,sizeof(str),len);
      strcat(str,"b");
    }
    xlen = strlen(str);
    VioWrtCharStr(str,xlen,0,vio.col - xlen,0);
  }
  VioWrtCharStr(filename,
                min(strlen(filename),vio.col - xlen),0,0,0);
  if(strlen(filename) > vio.col - xlen) {
    VioWrtCharStr("...",3,0,(vio.col - xlen) - (xlen) ? 5 : 3,0);
    if(xlen)
      VioWrtCharStr("  ",2,0,(vio.col - xlen) - 2,0);
  }
  VioWrtCharStr((hexmode) ? "[Hex] " : "[Text]",6,vio.row - 2,0,0);
}


ULONG ShowLine (char *text,ULONG len,BOOL hexmode,ULONG offset,BOOL top) {

  char *s = text + offset,*e,revattr = (8 | 6),attr = ((6 | 8) << 4),c,
        fattr = (3 | 8),*ee;

  if(!hexmode) {
    e = s;
    while(e < text + len && e - s < vio.col && *e != '\r' && *e != '\n')
      e++;
    if(findlen) {
      if(foundstr && foundstr >= s && foundstr <= e) {
        VioWrtNAttr(&revattr,e - s,(top) ? 1 : vio.row - 3,0,0);
        VioWrtNAttr(&attr,min(findlen,(vio.col - (foundstr - s))),
                    (top) ? 1 : vio.row - 3,foundstr - s,0);
      }
      else {
        c = *e;
        *e = 0;
        if(memstr(s,find,e - s,findlen,TRUE))
          VioWrtNAttr(&fattr,e - s,(top) ? 1 : vio.row - 3,0,0);
        *e = c;
      }
    }
    VioWrtCharStr(s,e - s,(top) ? 1 : vio.row - 3,0,0);
    if(e < text + len && *e == '\r')
      e++;
    if(e < text + len && *e == '\n')
      e++;
  }
  else {

    char str[80];
    int  x;

    e = str + sprintf(str,"%08lx:  ",s - text);
    for(x = 0;x < 16 && s + x < text + len;x++)
      e += sprintf(e,"%02hx ",s[x]);
    strcpy(e,"  ");
    e += 2;
    for(;x < 16;x++) {
      strcpy(e,"   ");
      e += 3;
    }
    for(x = 0;x < 16 && s + x < text + len;x++)
      *e++ = s[x];
    ee = min(text + len,s + 16);
    if(findlen) {
      if(foundstr && foundstr >= s && foundstr <= s + 16)
        VioWrtNAttr(&revattr,e - str,(top) ? 1 : vio.row - 3,0,0);
      else {
        c = *ee;
        *ee = 0;
        if(memstr(s,find,ee - s,findlen,TRUE))
          VioWrtNAttr(&fattr,e - str,(top) ? 1 : vio.row - 3,0,0);
        *ee = c;
      }
    }
    VioWrtCharStr(str,e - str,(top) ? 1 : vio.row - 3,0,0);
    e = ee;
  }
  return e - text;
}


ULONG ShowPage (char *text,ULONG len,BOOL hexmode,ULONG offset) {

  char *s = text + offset,*e,revattr = (8 | 6),attr = ((6 | 8) << 4),c,
        fattr = (3 | 8),*ee;
  int   lines = 0;

  if(!hexmode) {
    e = s;
    while(s < text + len && lines < vio.row - 3) {
      e = s;
      while(e < text + len && e - s < vio.col && *e != '\r' && *e != '\n')
        e++;
      if(findlen) {
        if(foundstr && foundstr >= s && foundstr <= e) {
          VioWrtNAttr(&revattr,e - s,lines + 1,0,0);
          VioWrtNAttr(&attr,min(findlen,(vio.col - (foundstr - s))),
                      lines + 1,foundstr - s,0);
        }
        else {
          c = *e;
          *e = 0;
          if(memstr(s,find,e - s,findlen,TRUE))
            VioWrtNAttr(&fattr,e - s,lines + 1,0,0);
          *e = c;
        }
      }
      VioWrtCharStr(s,e - s,lines + 1,0,0);
      if(e < text + len && *e == '\r')
        e++;
      if(e < text + len && *e == '\n')
        e++;
      s = e;
      lines++;
    }
  }
  else {

    char str[80];
    int  x;

    while(s < text + len && lines < vio.row - 3) {
      e = str + sprintf(str,"%08lx:  ",s - text);
      for(x = 0;x < 16 && s + x < text + len;x++)
        e += sprintf(e,"%02hx ",s[x]);
      strcpy(e,"  ");
      e += 2;
      for(;x < 16;x++) {
        strcpy(e,"   ");
        e += 3;
      }
      for(x = 0;x < 16 && s + x < text + len;x++)
        *e++ = s[x];
      ee = min(text + len,s + 16);
      if(findlen) {
        if(foundstr && foundstr >= s && foundstr <= s + 16)
          VioWrtNAttr(&revattr,e - str,lines + 1,0,0);
        else {
          c = *ee;
          *ee = 0;
          if(memstr(s,find,ee - s,findlen,TRUE))
            VioWrtNAttr(&fattr,e - str,lines + 1,0,0);
          *ee = c;
        }
      }
      VioWrtCharStr(str,e - str,lines + 1,0,0);
      s = ee;
      lines++;
    }
    e = s;
  }
  return e - text;
}


ULONG NextPage (char *text,ULONG len,BOOL hexmode,ULONG offset) {

  char *s = text + offset,*e;
  int   lines = 0;

  if(!hexmode) {
    e = s;
    while(s < text + len && lines < vio.row - 3) {
      e = s;
      while(e < text + len && e - s < vio.col && *e != '\r' && *e != '\n')
        e++;
      if(e < text + len)
        e++;
      if(e < text + len && *e == '\r')
        e++;
      if(e < text + len && *e == '\n')
        e++;
      s = e;
      lines++;
    }
  }
  else {

    char str[80];
    int  x;

    while(s < text + len && lines < vio.row - 3) {
      e = str + sprintf(str,"%08lx:  ",offset);
      for(x = 0;x < 16 && s + x < text + len;x++)
        e += sprintf(e,"%02hx ",s[x]);
      strcpy(e,"  ");
      e += 2;
      for(;x < 16;x++) {
        strcpy(e,"   ");
        e += 3;
      }
      for(x = 0;x < 16 && s + x < text + len;x++)
        *e++ = s[x];
      s = min(text + len,s + 16);
      lines++;
    }
    e = s;
  }
  return e - text;
}


ULONG PrevLine (char *text,ULONG len,BOOL hexmode,ULONG offset) {

  char *s,*e;

  if(!hexmode) {
    if(offset)
      offset--;
    s = text + offset;
    if(*s == '\n' && s > text)
      s--;
    if(*s == '\r' && s > text)
      s--;
    e = s;
    while(e > text && *e != '\r' && *e != '\n')
      e--;
    if(e != text && (*e == '\r' || *e == '\n'))
      e++;
    if(s - e > vio.col - 1) {
      e = s - ((s - e) % vio.col);
      if(e < text)
        e = text;
    }
  }
  else {
    if(offset % 16)
      offset += (16 - (offset % 16));
    s = text + offset;
    e = max(text,s - 16);
  }
  return e - text;
}


ULONG PrevPage (char *text,ULONG len,BOOL hexmode,ULONG offset) {

  int lines = 0;

  for(lines = 0;lines < vio.row - 3;lines++)
    offset = PrevLine(text,len,hexmode,offset);
  return offset;
}


ULONG NextLine (char *text,ULONG len,BOOL hexmode,ULONG offset) {

  char *s = text + offset,*e;

  if(!hexmode) {
    e = s;
    while(e < text + len && e - s < vio.col && *e != '\r' && *e != '\n')
      e++;
    if(e < text + len)
      e++;
    if(e < text + len && *e == '\r')
      e++;
    if(e < text + len && *e == '\n')
      e++;
  }
  else
    e = min(text + len,s + 16);
  return e - text;
}


ULONG RestartFile (char *filename,char **text,BOOL *hexmode,
                   ULONG *topoffset,ULONG *botoffset) {

  ULONG len = 0;
  int   attr = (7 << 8) | ' ';

  if(*text)
    free(*text);
  *text = NULL;
  foundstr = NULL;
  *find = 0;
  findlen = 0;
  ShowBorders(filename,0,*hexmode);
  VioScrollUp(1,0,vio.row - 3,vio.col - 1,-1,(char *)&attr,0);
  VioWrtCharStr("Reading...",10,vio.row / 2,(vio.col / 2) - 6,0);
  if((len = LoadFile(filename,text,hexmode)) != 0) {
    VioScrollUp(1,0,vio.row - 3,vio.col - 1,-1,(char *)&attr,0);
    ShowBorders(filename,len,*hexmode);
    *botoffset = ShowPage(*text,len,*hexmode,*topoffset);
  }
  return len;
}


int ViewFile (char *filename) {

  int   ret = 1,key,attr = (7 << 8) | ' ';
  ULONG len,topoffset = 0,botoffset,xlen;
  BOOL  okay = TRUE,hexmode = FALSE;
  char *text = NULL,*savescreen = NULL,per[8];

  if(!filename)
    return 0;

  viewing = TRUE;
  ThreadMsg(NULL);

  ret = 0;
  if(!viewonly)
    savescreen = SaveScreen(savescreen);
  len = RestartFile(filename,&text,&hexmode,&topoffset,&botoffset);
  if(len) {
    while(okay) {
      if(!topoffset) {

        char *msg = " [Top]";

        if(botoffset == len)
          msg = " [All]";
        VioWrtCharStr(msg,6,vio.row - 2,vio.col - 6,0);
      }
      else if(botoffset == len)
        VioWrtCharStr(" [End]",6,vio.row - 2,vio.col - 6,0);
      else {
        xlen = sprintf(per," [%02lu%%]",(botoffset * 100) / len);
        VioWrtCharStr(per,xlen,vio.row - 2,vio.col - xlen,0);
      }

      key = get_ch(-1);
ReSwitch:
      switch(key) {
        case 33 | 256:    /* alt+f, or... */
        case 'f':
        case 'F':
          if(hexmode) {
            foundstr = NULL;
            *find = 0;
            findlen = 0;
          }
          ret = EnterLine(find,160,vio.row - 2,"",NULL,0,0);
          ShowBorders(filename,len,hexmode);
          if(ret > 0) {
            findlen = literal(find);
            foundstr = memstr(text,find,len,findlen,TRUE);
ReFind:
            if(foundstr) {

              char *p;

              p = foundstr;
              VioScrollUp(1,0,vio.row - 3,vio.col - 1,-1,(char *)&attr,0);
              if(!hexmode) {
                if(*p == '\n')
                  p--;
                if(*p == '\r')
                  p--;
                while(p > text && *p != '\r' && *p != '\n')
                  p--;
                if(*p == '\r')
                  p++;
                if(*p == '\n')
                  p++;
                topoffset = p - text;
              }
              else {
                topoffset = p - text;
                if(topoffset % 16)
                  topoffset -= (topoffset % 16);
                if(topoffset > len)
                  topoffset = 0;
              }
              botoffset = NextPage(text,len,hexmode,topoffset);
              if(botoffset >= len) {
                key = 79 | 256;
                goto ReSwitch;
              }
              botoffset = ShowPage(text,len,hexmode,topoffset);
            }
            else 
              DosBeep(50,100);
          }
          break;

        case 48 | 256:    /* alt+b, or... */
        case 'b':
        case 'B':
          if(!*find) {
            key = 'f';
            goto ReSwitch;
          }
          {
            char *f,*t,*l = NULL,*o;

            o = foundstr;
            if(!o)
              o = text + topoffset;
            t = text;
            do {
              f = memstr(t,find,len,findlen,TRUE);
              if(f) {
                if(f == o)
                  break;
                t = f + findlen;
                l = f;
              }
            } while(f != NULL);
            if(l) {
              foundstr = l;
              goto ReFind;
            }
            else
              DosBeep(50,100);
          }
          break;

        case 49 | 256:    /* alt+n, or... */
        case 'n':
        case 'N':
          if(!*find) {
            key = 'f';
            goto ReSwitch;
          }
          {
            char *o = foundstr;

            if(foundstr)
              o = foundstr + findlen;
            else
              o = text + topoffset;
            if(o < text + topoffset)
              o = text + topoffset;
            if(o < text + len) {
              if(!hexmode) {
                o = memstr(o,find,len - (o - text),findlen,TRUE);
                if(o) {
                  foundstr = o;
                  goto ReFind;
                }
              }
            }
            else {
              o = memstr(o,find,len - (o - text),findlen,TRUE);
              if(o) {
                foundstr = o;
                goto ReFind;
              }
            }
          }
          DosBeep(50,100);
          break;

        case 25 | 256:    /* alt+p, or... */
        case 'p':
        case 'P':         /* print file */
          if(_beginthread(PrintFile,NULL,65536,strdup(filename)) == -1)
            DosBeep(50,100);
          else
            DosBeep(1000,10);
          break;

        case 59 | 256:    /* F1 -- help */

          break;

        case 75 | 256:    /* left, or... */
        case '\x1b':      /* Escape -- exit viewer */
          okay = FALSE;
          break;

        case 35 | 256:    /* alt+h, or... */
        case 8:           /* ctrl+h */
        case 'h':
        case 'H':         /* toggle hexmode */
          hexmode = (hexmode) ? FALSE : TRUE;
          ShowBorders(filename,len,hexmode);
          VioScrollUp(1,0,vio.row - 3,vio.col - 1,-1,(char *)&attr,0);
          topoffset = 0;
          botoffset = ShowPage(text,len,hexmode,topoffset);
          foundstr = NULL;
          *find = 0;
          findlen = 0;
          break;

        case 72 | 256:    /* up */
          if(topoffset) {
            VioScrollDn(1,0,vio.row - 3,vio.col - 1,1,(char *)&attr,0);
            topoffset = PrevLine(text,len,hexmode,topoffset);
            botoffset = PrevLine(text,len,hexmode,botoffset);
            ShowLine(text,len,hexmode,topoffset,TRUE);
          }
          break;

        case ' ':
        case 80 | 256:    /* down */
          if(botoffset < len) {
            VioScrollUp(1,0,vio.row - 3,vio.col - 1,1,(char *)&attr,0);
            topoffset = NextLine(text,len,hexmode,topoffset);
            botoffset = ShowLine(text,len,hexmode,botoffset,FALSE);
          }
          break;

        case 132 | 256:   /* ctrl+page up */
          if(viewonly || helping)
            break;
        case 118 | 256:   /* ctrl+page down */
          if(!viewonly && !helping && numfiles) {

            ULONG wasfile = file;

            for(;;) {
              switch(key) {
                case 132 | 256:
                  file--;
                  if(file >= numfiles)
                    file = numfiles - 1;
                  break;

                case 118 | 256:
                  file++;
                  if(file >= numfiles)
                    file = 0;
                  break;
              }
              if(file == wasfile)
                break;
              if(IsFile(files[file].filename) == 1) {
                filename = files[file].filename;
                len = RestartFile(filename,&text,&hexmode,
                                  &topoffset,&botoffset);
                if(len)
                  break;
              }
            }
            if(!text) {
              filename = files[wasfile].filename;
              file = wasfile;
            }
          }
          else if(viewonly) {

            FILEFINDBUF3 fb3;
            ULONG        nm = 1,attrib = findattr;
            HDIR         hdir = HDIR_CREATE;
            APIRET       rc;
            char        *p,first[CCHMAXPATH],wasname[CCHMAXPATH];
            BOOL         reloaded = FALSE,foundit = FALSE;

            strcpy(wasname,filename);
            *first = 0;
            attrib &= (~FILE_DIRECTORY);
            p = strrchr(directory,'\\');
            if(p)
              p++;
            else
              p = directory;
            DosError(FERR_DISABLEHARDERR);
            rc = DosFindFirst(lastdir,&hdir,attrib,&fb3,sizeof(fb3),&nm,
                              FIL_STANDARD);
            if(!rc) {
              strcpy(first,directory);
              strcpy(first + (p - directory),fb3.achName);
              while(!rc) {
                if((fb3.attrFile & FILE_DIRECTORY) == 0) {
                  strcpy(p,fb3.achName);
                  if(!stricmp(directory,wasname))
                    foundit = TRUE;
                  else if(foundit && fb3.cbFile) {
                    filename = directory;
                    len = RestartFile(filename,&text,&hexmode,
                                      &topoffset,&botoffset);
                    if(len) {
                      reloaded = TRUE;
                      break;
                    }
                  }
                  else
                    strcpy(directory,wasname);
                }
                nm = 1;
                rc = DosFindNext(hdir,&fb3,sizeof(fb3),&nm);
              }
              DosFindClose(hdir);
            }
            if(!reloaded && *first) {
              strcpy(directory,first);
              filename = directory;
              len = RestartFile(filename,&text,&hexmode,&topoffset,&botoffset);
              if(len)
                break;
            }
            if(!text) {
              strcpy(directory,wasname);
              filename = directory;
            }
          }
          if(!text) {
            len = RestartFile(filename,&text,&hexmode,&topoffset,&botoffset);
            if(!len) {
              key = '\x1b';
              goto ReSwitch;
            }
          }
          break;

        case 73 | 256:    /* page up */
          if(topoffset) {
            VioScrollUp(1,0,vio.row - 3,vio.col - 1,-1,(char *)&attr,0);
            topoffset = PrevPage(text,len,hexmode,topoffset);
            botoffset = ShowPage(text,len,hexmode,topoffset);
          }
          break;

        case '\r':
        case 81 | 256:    /* page down */
          if(botoffset < len) {
            VioScrollUp(1,0,vio.row - 3,vio.col - 1,-1,(char *)&attr,0);
            topoffset = botoffset;
            botoffset = NextPage(text,len,hexmode,topoffset);
            if(botoffset >= len) {
              key = 79 | 256;
              goto ReSwitch;
            }
            botoffset = ShowPage(text,len,hexmode,topoffset);
          }
          break;

        case 71 | 256:    /* home */
          if(topoffset) {
            VioScrollUp(1,0,vio.row - 3,vio.col - 1,-1,(char *)&attr,0);
            topoffset = 0;
            botoffset = ShowPage(text,len,hexmode,topoffset);
          }
          break;

        case 79 | 256:    /* end */
          VioScrollUp(1,0,vio.row - 3,vio.col - 1,-1,(char *)&attr,0);
          if(!hexmode) {
            topoffset = PrevPage(text,len,hexmode,len);
            botoffset = ShowPage(text,len,hexmode,topoffset);
          }
          else {
            topoffset = len - (16 * (vio.row - 3));
            if(topoffset % 16) {
              topoffset -= (topoffset % 16);
              if((len - topoffset) / 16 >= vio.row - 3)
                topoffset += 16;
            }
            if(topoffset > len)
              topoffset = 0;
            botoffset = ShowPage(text,len,hexmode,topoffset);
          }
          break;

        case 45 | 256:    /* alt-x or ... */
        case 61 | 256:    /* f3 -- exit program */
          okay = FALSE;
          ret = -1;
          break;
      }
    }
    if(text)
      free(text);
  }
  else {
    if(filename) {

      char s[CCHMAXPATH + 80];

      sprintf(s,"Can't get anything out of \"%s\"",filename);
      SimpleInput("Dang!",s,100,50,2000,NULL);
    }
    else
      DosBeep(50,100);
  }

  if(!viewonly) {
    if(ret != -1)
      RestoreScreen(savescreen);
    free(savescreen);
  }

  viewing = FALSE;
  ThreadMsg(NULL);
  SetupConsole();

  return ret;
}

