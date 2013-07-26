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

#pragma alloc_text(EDITLINE,EnterLine)
#pragma alloc_text(EDITATTR,EditAttributes)

extern void ThreadMsg (char *str);


int EnterLine (char *buf,ULONG len,USHORT y,char *filename,
               char **choices,int numc,int start) {

  ULONG         pos = 0,c,nm,numchars = 0,tpos,wl;
  APIRET        rc;
  int           key,attr = ((7 << 4) << 8) | ' ',ret = 0;
  BOOL          okay = TRUE,insert = TRUE,lasttab = FALSE;
  char         *sav,*k = NULL,*p;
  static char   keybuf[1026];
  USHORT        t;
  FILEFINDBUF3  fb3;
  HDIR          hdir = HDIR_CREATE;

  wl = 0;

  sav = malloc(vio.col * 2);
  if(!sav) {
    DosBeep(50,100);
    *buf = 0;
    return -1;
  }

  ThreadMsg(" ");

  t = vio.col * 2;
  VioReadCellStr(sav,&t,y,0,0);
  VioWrtNCell((char *)&attr,vio.col,y,0,0);
  ShowCursor(FALSE);
  VioSetCurPos(y,0,0);

  for(c = 0;c < len - 1;c++) {          /* find end of default string */
    if(!buf[c])
      break;
  }
  pos = c;
  for(;c < len - 1;c++)                 /* space-pad remainder of buf */
    buf[c] = ' ';

  while(okay) {
    /* assure buffer is null terminated (cluck, cluck) */
    buf[len - 1] = 0;

    /* assure pos hasn't gone past our limit */
    if(pos > len - 1)
      pos = len - 1;

    /* set left side of entry field */
    if(pos < wl)
      wl = pos;
    if(pos >= wl + vio.col)
      wl = (pos - vio.col) + 1;

    /* set cursor position */
    VioSetCurPos(y,pos - wl,0);

    /* display buf */
    tpos = min(vio.col,len - wl); /* max length of displayable text */
    VioWrtCharStr(buf + wl,tpos,y,0,0); /* show text */
    if(tpos < vio.col)                  /* space-pad? */
      VioWrtNChar(" ",vio.col - tpos,y,tpos,0);

    if(k && *k)   /* "macros" waiting to be entered? */
      key = *k++; /* yep, skip keyboard input */
    else {        /* nope, go to the keyboard */
      k = NULL;   /* clear macro pointer */
      key = get_ch(-1);
    }

    switch(key) {
      case 256:       /* shiftstate changed -- ignore */
        break;

      case '\r':      /* accept input */
        okay = FALSE;
        break;

      case 45 | 256:
      case 61 | 256:
      case '\x1b':    /* Escape -- exit editor */
        memset(buf,' ',len - 1);
        buf[len - 1] = 0;
        okay = FALSE;
        ret = -1;
        break;

      case '\b':
        if(pos) {
          pos--;
          memmove(buf + pos,buf + (pos + 1),len - (pos + 1));
          buf[len - 2] = ' ';
        }
        lasttab = FALSE;
        break;

      case 25:          /* ctrl+y -- clear input */
        VioWrtNCell((char *)&attr,vio.col,y,0,0);
        memset(buf,' ',len - 1);
        buf[len - 1] = 0;
        wl = pos = 0;
        lasttab = FALSE;
        break;

      case 59 | 256:    /* F1 -- insert directory */
        k = directory;
        lasttab = FALSE;
        break;

      case 60 | 256:    /* F2 -- insert filename */
        if(filename)
          k = filename;
        lasttab = FALSE;
        break;

      case 62 | 256:    /* F4 -- insert target */
        k = target;
        lasttab = FALSE;
        break;

      case 15:          /* shift+tab */
        if(hdir != (HDIR)HDIR_CREATE) {
          DosFindClose(hdir);
          hdir = HDIR_CREATE;
          lasttab = FALSE;
        }
        /* intentional fallthru */
      case 9:           /* tab -- auto-complete */
        if(!pos || pos >= len - 1)
          break;
        if(lasttab && numchars) {
          lasttab = FALSE;
          for(tpos = 0;tpos < numchars;tpos++)
            keybuf[tpos] = '\b';
          keybuf[tpos++] = '\01';
          keybuf[tpos] = 0;
          numchars = 0;
          k = keybuf;
          break;
        }
        else {
          if(hdir != (HDIR)HDIR_CREATE)
            DosFindClose(hdir);
          hdir = HDIR_CREATE;
        }
        /* intentional fallthru */
      case 1:           /* cheat! */
        k = NULL;
        if(!pos || pos >= len - 1)
          break;
        tpos = pos - 1;
        while(tpos && buf[tpos] != ' ')
          tpos--;
        if(buf[tpos] == ' ')
          tpos++;
        strcpy(keybuf,buf + tpos);
        tpos = 0;
        while(keybuf[tpos] && keybuf[tpos] != ' ')
          tpos++;
        keybuf[tpos] = 0;
        lstrip(rstrip(keybuf));
        if(*keybuf) {
          strcat(keybuf,"*");
          nm = 1;
          if(hdir == (HDIR)HDIR_CREATE)
            rc = DosFindFirst(keybuf,
                              &hdir,
                              FILE_NORMAL    | FILE_HIDDEN   | FILE_SYSTEM |
                              FILE_DIRECTORY | FILE_ARCHIVED | FILE_READONLY,
                              &fb3,
                              sizeof(fb3),
                              &nm,
                              FIL_STANDARD);
          else
            rc = DosFindNext(hdir,&fb3,sizeof(fb3),&nm);
          if(!rc) {
            while((fb3.attrFile & FILE_DIRECTORY) &&
                  (*fb3.achName == '.' && (!fb3.achName[1] ||
                                           (fb3.achName[1] == '.' &&
                                            !fb3.achName[2])))) {
              nm = 1;
              if(DosFindNext(hdir,&fb3,sizeof(fb3),&nm)) {
                DosFindClose(hdir);
                hdir = HDIR_CREATE;
                *fb3.achName = 0;
                break;
              }
            }
            if(*fb3.achName) {
              keybuf[strlen(keybuf) - 1] = 0;
              p = strchr(keybuf,'\\');
              if(p)
                p++;
              else
                p = keybuf;
              tpos = 0;
              while(*p && fb3.achName[tpos] &&
                    toupper(*p) == toupper(fb3.achName[tpos])) {
                p++;
                tpos++;
              }
              if(fb3.achName[tpos]) {
                strcpy(keybuf,fb3.achName + tpos);
                numchars = strlen(keybuf);
                lasttab = TRUE;
                k = keybuf;
              }
              else if(hdir != (HDIR)HDIR_CREATE) {
                DosFindClose(hdir);
                hdir = HDIR_CREATE;
              }
            }
          }
          else if(hdir != (HDIR)HDIR_CREATE) {
            DosBeep(50,50);
            DosFindClose(hdir);
            hdir = HDIR_CREATE;
          }
        }
        break;

      case 83 | 256:    /* delete */
        memmove(buf + pos,buf + (pos + 1),len - (pos + 1));
        buf[len - 2] = ' ';
        lasttab = FALSE;
        break;

      case 82 | 256:    /* insert */
        insert = (insert) ? FALSE : TRUE;
        break;

      case 71 | 256:    /* home */
        wl = pos = 0;
        lasttab = FALSE;
        break;

      case 79 | 256:    /* end */
        pos = len - 2;
        while(pos && buf[pos] == ' ')
          pos--;
        if(pos && buf[pos] != ' ')
          pos++;
        lasttab = FALSE;
        break;

      case 75 | 256:    /* left */
        if(pos)
          pos--;
        lasttab = FALSE;
        break;

      case 77 | 256:    /* right */
        if(pos < len - 1)
          pos++;
        lasttab = FALSE;
        break;

      case 72 | 256:    /* up */
        lasttab = FALSE;
        if(choices) {
          if(choices[start]) {
            VioWrtNCell((char *)&attr,vio.col,y,0,0);
            memset(buf,' ',len - 1);
            buf[len - 1] = 0;
            wl = pos = 0;
            k = choices[start];
            start++;
            while(start < numc && !choices[start])
              start++;
            if(start > (numc - 1))
              start = 0;
            if(!choices[start]) {
              while(start < numc && !choices[start])
                start++;
            }
            if(start > (numc - 1))
              start = 0;
          }
        }
        break;

      case 80 | 256:    /* down */
        lasttab = FALSE;
        if(choices) {
          if(choices[start]) {
            VioWrtNCell((char *)&attr,vio.col,y,0,0);
            memset(buf,' ',len - 1);
            buf[len - 1] = 0;
            wl = pos = 0;
            k = choices[start];
            start--;
            while(start >= 0 && !choices[start])
              start--;
            if(start < 0)
              start = numc - 1;
            if(!choices[start]) {
              while(start >= 0 && !choices[start])
                start--;
            }
            if(start < 0)
              start = numc - 1;
          }
        }
        break;

      case 115 | 256:   /* ctrl+left */
        while(pos && buf[pos] == ' ')
          pos--;
        while(pos && buf[pos] != ' ')
          pos--;
        lasttab = FALSE;
        break;

      case 116 | 256:   /* ctrl + right */
        while(pos < len - 1 && buf[pos] == ' ')
          pos++;
        while(pos < len - 1 && buf[pos] != ' ')
          pos++;
        lasttab = FALSE;
        break;

      default:
        if(pos < len - 1 && !(key & 256) && !iscntrl(key)) {
          if(insert) {
            if(pos < len - 2) {
              memmove(buf + (pos + 1),buf + pos,len - (pos + 2));
              buf[len - 2] = ' ';
            }
            buf[pos] = (char)key;
          }
          else
            buf[pos] = (char)key;
          pos++;
        }
        else if(pos >= len - 1)
          DosBeep(250,25);
        break;
    }
  }

  if(hdir != (HDIR)HDIR_CREATE)
    DosFindClose(hdir);

  ShowCursor(TRUE);
  VioWrtCellStr(sav,vio.col * 2,y,0,0);
  free(sav);
  SetupConsole();

  ThreadMsg(NULL);

  buf[len - 1] = 0;
  lstrip(rstrip(buf));
  return (ret) ? ret : strlen(buf);
}


int EditAttributes (FILES *f) {

  char  *buf;
  USHORT len = (vio.col * 7) * 2,curpos = 0;
  char   nattr = (1 << 4) | (7 | 8);
  char   sattr = 8;
  int    ret = 0,x,key;
  BOOL   okay = TRUE;
  char   attrs[5] = "----";

  if(!f)
    return -1;
  buf = malloc(len);
  if(!buf)
    return -1;

  ThreadMsg(" ");

  VioReadCellStr(buf,&len,(vio.row / 2) - 1,0,0);

  VioWrtCharStrAtt("ÚÄ Attributes: Ä¿",17,(vio.row / 2) - 1,
                   34,&nattr,0);
  VioWrtCharStrAtt("³ Readonly: [ ] ³",17,vio.row / 2,
                   34,&nattr,0);
  VioWrtCharStrAtt("³ Hidden:   [ ] ³",17,(vio.row / 2) + 1,
                   34,&nattr,0);
  VioWrtCharStrAtt("³ System:   [ ] ³",17,(vio.row / 2) + 2,
                   34,&nattr,0);
  VioWrtCharStrAtt("³ Archived: [ ] ³",17,(vio.row / 2) + 3,
                   34,&nattr,0);
  VioWrtCharStrAtt("ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ",17,(vio.row / 2) + 4,
                   34,&nattr,0);
  VioWrtNAttr(&sattr,1,vio.row / 2,51,0);
  VioWrtNAttr(&sattr,1,(vio.row / 2) + 1,51,0);
  VioWrtNAttr(&sattr,1,(vio.row / 2) + 2,51,0);
  VioWrtNAttr(&sattr,1,(vio.row / 2) + 3,51,0);
  VioWrtNAttr(&sattr,1,(vio.row / 2) + 4,51,0);
  VioWrtNAttr(&sattr,16,(vio.row / 2) + 5,36,0);
  if(f->attrFile & FILE_READONLY)
    attrs[0] = 'x';
  if(f->attrFile & FILE_HIDDEN)
    attrs[1] = 'x';
  if(f->attrFile & FILE_SYSTEM)
    attrs[2] = 'x';
  if(f->attrFile & FILE_ARCHIVED)
    attrs[3] = 'x';
  for(x = 0;x < 4;x++)
    VioWrtCharStr(attrs + x,1,x + (vio.row / 2),47,0);
  VioSetCurPos(curpos + (vio.row / 2),47,0);
  ShowCursor(FALSE);
  while(okay) {
    VioWrtCharStr(attrs + curpos,1,curpos + (vio.row / 2),47,0);
    VioSetCurPos(curpos + (vio.row / 2),47,0);
    key = get_ch(-1);
    switch(key) {
      case 256:
        break;

      case 45 | 256:
      case 61 | 256:
      case '\x1b':  /* abort */
        okay = FALSE;
        ret = -1;
        break;

      case '\r':      /* process */
        okay = FALSE;
        {
          FILESTATUS3 fs3;

          if(!DosQueryPathInfo(f->filename,FIL_STANDARD,&fs3,sizeof(fs3))) {
            fs3.attrFile &= (~FILE_DIRECTORY);
            if(attrs[0] == 'x')
              fs3.attrFile |= FILE_READONLY;
            else
              fs3.attrFile &= (~FILE_READONLY);
            if(attrs[1] == 'x')
              fs3.attrFile |= FILE_HIDDEN;
            else
              fs3.attrFile &= (~FILE_HIDDEN);
            if(attrs[2] == 'x')
              fs3.attrFile |= FILE_SYSTEM;
            else
              fs3.attrFile &= (~FILE_SYSTEM);
            if(attrs[3] == 'x')
              fs3.attrFile |= FILE_ARCHIVED;
            else
              fs3.attrFile &= (~FILE_ARCHIVED);
            if(DosSetPathInfo(f->filename,FIL_STANDARD,&fs3,sizeof(fs3),
                              DSPI_WRTTHRU))
              DosBeep(50,100);
            else
              ret = 1;
          }
          else
            DosBeep(50,100);
        }
        break;

      case 72 | 256:  /* up */
        curpos--;
        if(curpos > 3)
          curpos = 3;
        break;

      case 'x':
      case 'X':
      case '+':
        attrs[curpos] = 'x';
        break;

      case '-':
        attrs[curpos] = '-';
        break;

      case ' ':       /* toggle */
        attrs[curpos] = (attrs[curpos] == 'x') ? '-' : 'x';
        VioWrtCharStr(attrs + curpos,1,curpos + (vio.row / 2),47,0);
        /* intentional fallthru */
      case 80 | 256:  /* down */
        curpos++;
        if(curpos > 3)
          curpos = 0;
        break;
    }
  }

  ShowCursor(TRUE);
  VioWrtCellStr(buf,len,(vio.row / 2) - 1,0,0);
  free(buf);
  SetupConsole();
  ThreadMsg(NULL);
  return ret;
}

