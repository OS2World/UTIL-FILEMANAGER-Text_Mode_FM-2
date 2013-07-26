/****************************************************************************
    fmt.c - main line control

    FMT is a text-mode file maintenance utility program
    Copyright (c) 1997, 2001 by Mark Kimes

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

Revisions	19 Aug 01 MK - Baseline

*****************************************************************************/

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
#include <process.h>
#include <time.h>
#define DEFINE_GLOBALS
#include "fmt.h"

#pragma alloc_text(FMT,DisplayTarget,DisplayCurrFile,DisplayNumSizeFiles)
#pragma alloc_text(FMT,DisplayFreeSpace,BuildFiles,DisplayFiles)
#pragma alloc_text(FMT,DrawFile,DisableTimer,AdjustLastDir)
#pragma alloc_text(SORT,SortFiles,CompareNames,CompareSizes,CompareDates)

extern void ThreadMsg (char *str);

static clock_t timer = (clock_t)-1;
static char    input[CCHMAXPATH + 2];


void DisplayTarget (void) {

  int attr = (((8 << 4) | (7 | 8)) << 8) | ' ',xlen;

  xlen = strlen(target);
  VioWrtNCell((char *)&attr,vio.col,vio.row - 3,0,0);
  VioWrtCharStr("Target: ",8,vio.row - 3,0,0);
  VioWrtCharStr(target,min(xlen,vio.col - 8),vio.row - 3,8,0);
  if(xlen > vio.col - 8)
    VioWrtCharStr("...",3,vio.row - 3,vio.col - 3,0);
}


void DisplayCurrFile (FILES *f) {

  int         attr = (((3 << 4) | (7 | 8)) << 8) | ' ';
  char        hattr = ((3 << 4) | (8 << 4));
  int         xlen,x,y;
  static char str[CCHMAXPATH];
  char        attrstr[] = "RHS\0DA";

  VioWrtNCell((char *)&attr,vio.col * 2,0,0,0);
  VioWrtNAttr(&hattr,22,1,0,0);
  VioWrtNAttr(&hattr,19,1,44,0);
  if(f) {
    if(!strchr(f->filename,'\\')) {
      strcpy(str,directory);
      xlen = strlen(str);
      if(str[xlen - 1] != '\\')
        strcpy(&str[xlen++],"\\");
      strcpy(&str[xlen],f->filename);
      xlen += strlen(f->filename);
    }
    else {
      strcpy(str,f->filename);
      xlen = strlen(str);
    }
    VioWrtCharStr(str,min(vio.col,xlen),0,0,0);
    if(xlen > vio.col)
      VioWrtCharStr("...",3,0,vio.col - 3,0);

    sprintf(str," %02u:%02u:%02u  %04u/%02u/%02u ",
            f->time.hours,
            f->time.minutes,
            f->time.twosecs * 2,
            f->date.year + 1980,
            f->date.month,
            f->date.day);
    VioWrtCharStr(str,22,1,0,0);
    xlen = CommaFormat(str,sizeof(str),f->cbFile);
    strcpy(str + xlen," bytes");
    WriteCenterString(str,1,22,22);
    if(f->cbList > 4) {
      xlen = CommaFormat(str,sizeof(str),f->cbList / 2);
      strcpy(str + xlen," bytes EAs");
    }
    else
      strcpy(str,"<No EAs>");
    WriteCenterString(str,1,44,20);
    strcpy(str,"Attr: ");
    y = 6;
    for(x = 0;x < 6;x++)
      if(attrstr[x])
        str[y++] = ((f->attrFile & (1 << x)) ?
                     attrstr[x] : '-');
    str[y]  = 0;
    WriteCenterString(str,1,63,17);
  }
  else {
    if(*directory && !recurse) {
      VioWrtCharStr("Dir: ",5,0,0,0);
      VioWrtCharStr(directory,min(strlen(directory),vio.col - 5),0,5,0);
      if(strlen(directory) > vio.col - 5)
        VioWrtCharStr("...",3,0,vio.col - 3,0);
    }
    else
      VioWrtCharStr("<No current item>",17,0,0,0);
  }
}


void DisplayNumSizeFiles (void) {

  int  attr = (((0 << 4) | (8 << 4) | (7 | 8)) << 8) | ' ';
  char str[42];
  int  xlen;
  BOOL amfiltering = FALSE;

  if((*filter && strcmp(filter,"*")) ||
     ((findattr & FILE_HIDDEN) == 0))
    amfiltering = TRUE;

  VioWrtNCell((char *)&attr,22,vio.row - 2,23,0);
  if(vio.col > 80)
    VioWrtNCell((char *)&attr,vio.col - 80,vio.row - 2,80,0);
  xlen = CommaFormat(str,38,numfiles);
  str[xlen++] = '/';
  xlen += CommaFormat(str + xlen,38 - xlen,nummarked);
  strcpy(str + xlen," files");
  WriteCenterString(str,vio.row - 2,23,22);

  attr = ((7 << 4) << 8) | ' ';
  VioWrtNCell((char *)&attr,35 - (amfiltering != FALSE),vio.row - 2,45,0);
  xlen = CommaFormat(str,38 - (amfiltering != FALSE),sizefiles);
  str[xlen++] = '/';
  xlen += CommaFormat(str + xlen,(38 - (amfiltering != FALSE)) - xlen,
                      sizemarked);
  strcpy(str + xlen," bytes");
  WriteCenterString(str,vio.row - 2,45,35 - (amfiltering != FALSE));

  if(amfiltering) {
    attr = (((0 << 4) | (8 << 4) | (7 | 8)) << 8) | 'F';
    VioWrtNCell((char *)&attr,1,vio.row - 2,vio.col - 1,0);
  }
}


void DisplayFreeSpace (char curdrive) {

  int  attr = ((7 << 4) << 8) | ' ';
  int  xlen;
  char str[24];

  VioWrtNCell((char *)&attr,23,vio.row - 2,0,0);
  xlen = CommaFormat(str,sizeof(str),freespace);
  strcpy(str + xlen," free");
  WriteCenterString(str,vio.row - 2,3,20);
  strcpy(str," : ");
  *str = toupper(curdrive);
  VioWrtCharStr(str,3,vio.row - 2,0,0);
}


static int CompareDates (const void *v1,const void *v2);
static int CompareSizes (const void *v1,const void *v2);
static int CompareNames (const void *v1,const void *v2);

static int CompareDates (const void *v1,const void *v2) {

  FILES *d1 = (FILES *)v1;
  FILES *d2 = (FILES *)v2;
  int    ret;
  union {
    USHORT x;
    FDATE  date;
  } dc1,dc2;
  union {
    USHORT x;
    FTIME  time;
  } tc1,tc2;

  if((d1->attrFile & FILE_DIRECTORY) != (d2->attrFile & FILE_DIRECTORY)) {
    ret = ((d1->attrFile & FILE_DIRECTORY) == 0) ? 1 : -1;
    if(!dirstop)
      ret = -ret;
    return ret;
  }

  dc1.date = d1->date;
  dc2.date = d2->date;
  ret = (dc1.x > dc2.x) ? 1 : (dc1.x < dc2.x) ? -1 : 0;
  if(!ret) {
    tc1.time = d1->time;
    tc2.time = d2->time;
    ret = (tc1.x > tc2.x) ? 1 : (tc1.x < tc2.x) ? -1 : 0;
    if(!ret) {
      ret = CompareNames(v1,v2);
      return ret;
    }
  }
  if(invertsort)
    ret = (ret < 1) ? 1 : (ret > 1) ? -1 : 0;
  return ret;
}
                

static int CompareSizes (const void *v1,const void *v2) {

  FILES *d1 = (FILES *)v1;
  FILES *d2 = (FILES *)v2;
  int    ret;

  if((d1->attrFile & FILE_DIRECTORY) != (d2->attrFile & FILE_DIRECTORY)) {
    ret = ((d1->attrFile & FILE_DIRECTORY) == 0) ? 1 : -1;
    if(!dirstop)
      ret = -ret;
    return ret;
  }

  ret = (d1->cbFile + d1->cbList > d2->cbFile + d2->cbList) ?
         1 : (d1->cbFile + d1->cbList < d2->cbFile + d2->cbList) ?
          -1 : 0;
  if(!ret) {
    ret = CompareNames(v1,v2);
    return ret;
  }
  if(invertsort)
    ret = (ret < 1) ? 1 : (ret > 1) ? -1 : 0;
  return ret;
}
                

static int CompareNames (const void *v1,const void *v2) {

  FILES *d1 = (FILES *)v1;
  FILES *d2 = (FILES *)v2;
  int    ret;

  if((d1->attrFile & FILE_DIRECTORY) != (d2->attrFile & FILE_DIRECTORY)) {
    ret = ((d1->attrFile & FILE_DIRECTORY) == 0) ? 1 : -1;
    if(!dirstop)
      ret = -ret;
    return ret;
  }

  if(sorttype == S_FULLNAME ||
     (d1->attrFile & FILE_DIRECTORY) || !strchr(d1->filename,'\\'))
    ret = stricmp(d1->filename,d2->filename);
  else {

    char *p1,*p2;

    p1 = strrchr(d1->filename,'\\');
    p2 = strrchr(d2->filename,'\\');
    if(p1)
      p1++;
    else
      p1 = d1->filename;
    if(p2)
      p2++;
    else
      p2 = d2->filename;
    ret = stricmp(p1,p2);
    if(!ret)
      ret = stricmp(d1->filename,d2->filename);
  }
  if(invertsort)
    ret = (ret < 1) ? 1 : (ret > 1) ? -1 : 0;
  return ret;
}
                

void SortFiles (void) {

  if(numfiles)
    qsort(files,numfiles,sizeof(FILES),
          ((sorttype == S_SIZE) ? CompareSizes : (sorttype == S_DATE) ?
           CompareDates : CompareNames));
}


APIRET BuildList (char *dir,BOOL first,BOOL multdirs) {

  char               *mask;
  FILEFINDBUF4       *fb4,*pffb;
  BYTE               *pb;
  HDIR                hdir = HDIR_CREATE;
  ULONG               nm,x,maxnum;
  APIRET              rc = ERROR_NOT_ENOUGH_MEMORY;
  FILES              *test;

  maxnum = (recurse) ? 32 : 128;

  if(first) {
    /* clear what's already in the list, display "scanning" text */
    {
      int  iattr = (7 << 8) | ' ';

      VioScrollUp(2,0,vio.row - 4,vio.col - 1,-1,(char *)&iattr,0);
      VioWrtCharStr("Scanning...",11,vio.row / 2,(vio.col / 2) - 6,0);
    }
    if(files) {
      for(nm = 0;nm < numfiles;nm++)
        if(files[nm].filename)
          free(files[nm].filename);
      free(files);
    }
    files = NULL;
    numfiles = nummarked = sizefiles = sizemarked = topfile = file = 0;
  }
  else {

    char str[38];

    sprintf(str,"%lu",numfiles);
    VioWrtCharStr(str,strlen(str),vio.row / 2,vio.col - strlen(str),0);
  }

  /* allocate required memory */
  mask = malloc(CCHMAXPATH + 4);
  if(!mask)
    goto Abort;
  fb4  = malloc(sizeof(FILEFINDBUF4) * maxnum);
  if(!fb4) {
    free(mask);
    goto Abort;
  }

  nm = maxnum;
  sprintf(mask,"%s%s%s%s*",
          directory,
          (directory[strlen(directory) - 1] != '\\') ? "\\" : "",
          (dir && *dir) ? dir : "",
          (dir && *dir && dir[strlen(dir) - 1] != '\\') ? "\\" : "");
  DosError(FERR_DISABLEHARDERR);
  rc = DosFindFirst(mask,
                    &hdir,
                    findattr,
                    fb4,
                    sizeof(FILEFINDBUF4) * maxnum,
                    &nm,
                    FIL_QUERYEASIZE);
  if(!rc) {
    while(!rc) {
      pb = (PBYTE)fb4;
      for(x = 0;x < nm && rc != ERROR_NOT_ENOUGH_MEMORY;x++) {
        pffb = (FILEFINDBUF4 *)pb;
        if(((pffb->attrFile & FILE_DIRECTORY) != 0) ||
           (!*filter || MultiWildCard(pffb->achName,filter))) {
          if(((pffb->attrFile & FILE_DIRECTORY) == 0) ||
             (*pffb->achName != '.' ||
              (pffb->achName[1] != '.' && pffb->achName[1]))) {
            if(recurse && ((pffb->attrFile & FILE_DIRECTORY) != 0)) {
              sprintf(mask,"%s%s%s",
                      (dir) ? dir : "",
                      (dir && *dir && dir[strlen(dir) - 1] != '\\') ? "\\" : "",
                      pffb->achName);
              rc = BuildList(mask,FALSE,multdirs);
              if(rc == ERROR_NOT_ENOUGH_MEMORY)
                break;
              if(includedirsinalllists)
                goto IncludeTheDarnDir;
            }
            else {
IncludeTheDarnDir:
              test = realloc(files,(numfiles + 1) * sizeof(FILES));
              if(test) {
                files = test;
                memset(&files[numfiles],0,sizeof(FILES));
                files[numfiles].cbFile   = pffb->cbFile;
                files[numfiles].cbList   = pffb->cbList;
                files[numfiles].attrFile = pffb->attrFile;
                files[numfiles].date     = pffb->fdateLastWrite;
                files[numfiles].time     = pffb->ftimeLastWrite;
                if(!multdirs && (first || (!dir || !*dir)))
                  files[numfiles].filename = strdup(pffb->achName);
                else {
                  if(!multdirs)
                    sprintf(mask,"%s%s%s",
                            (dir) ? dir : "",
                            (dir && *dir && dir[strlen(dir) - 1] == '\\') ? "" : "\\",
                            pffb->achName);
                  else
                    sprintf(mask,"%s%s%s%s%s",
                            directory,
                            (directory[strlen(directory) - 1] != '\\') ? "\\" : "",
                            (dir) ? dir : "",
                            (dir && *dir && dir[strlen(dir) - 1] == '\\') ? "" : "\\",
                            pffb->achName);
                  files[numfiles].filename = strdup(mask);
                }
                if(!files[numfiles].filename) {
                  if(numfiles)
                    files = realloc(files,numfiles * sizeof(FILES));
                  else {
                    free(files);
                    files = NULL;
                  }
                  rc = ERROR_NOT_ENOUGH_MEMORY;
                  break;
                }
                else {
                  numfiles++;
                  sizefiles += (pffb->cbFile +
                                ((pffb->cbList > 4) ? pffb->cbList / 2 : 0));
                }
              }
              else {
                rc = ERROR_NOT_ENOUGH_MEMORY;
                break;
              }
            }
          }
        }
        pb += pffb->oNextEntryOffset;
      }
      nm = maxnum;
      DosError(FERR_DISABLEHARDERR);
      rc = DosFindNext(hdir,
                       fb4,
                       sizeof(FILEFINDBUF4) * maxnum,
                       &nm);
    }
    DosFindClose(hdir);
  }

  free(mask);
  free(fb4);

  if(numfiles && first && !multdirs) {

    char str[38];

    sprintf(str,"%lu",numfiles);
    VioWrtCharStr(str,strlen(str),vio.row / 2,vio.col - strlen(str),0);
    VioWrtCharStr("Sorting... ",11,vio.row / 2,(vio.col / 2) - 6,0);
    SortFiles();
  }


Abort:
  if(first && rc == ERROR_NOT_ENOUGH_MEMORY) {

    int responses[] = {0,0};

    SimpleInput("Warning!",
                " ***FMT ran out of memory!  (Press a key to continue)*** ",
                100,250,3000,responses);
  }

  return rc;
}


void DisplayFiles (void) {

  int   x = 0,xlen;
  char  attr,color;
  ULONG filelen,sizepos,attrpos;

  {
    int  iattr = (7 << 8) | ' ';

    VioScrollUp(2,0,vio.row - 4,vio.col - 1,-1,(char *)&iattr,0);
  }

  if(!numfiles)
    return;

  filelen = vio.col;
  if(infolevel & I_DATE)
    filelen -= 21;
  if(infolevel & I_SIZE) {
    filelen -= 16;
    sizepos = vio.col - 14;
    if(infolevel & I_DATE)
      sizepos -= 21;
    if(infolevel & I_ATTR)
      sizepos -= 7;
  }
  if(infolevel & I_ATTR) {
    filelen -= 7;
    attrpos = vio.col - 5;
    if(infolevel & I_DATE)
      attrpos -= 21;
  }

  while(x + topfile < numfiles && x < vio.row - 5) {
    color = ((files[x + topfile].attrFile & FILE_DIRECTORY) != 0) ?
             (((files[x + topfile].flags & F_MARKED) != 0) ? 3 | 8 : 2 | 8) :
             (((files[x + topfile].flags & F_MARKED) != 0) ? 6 | 8 : 7);
    attr = (x + topfile == file) ? (color << 4) : color;
    xlen = strlen(files[x + topfile].filename);
    VioWrtCharStrAtt(files[x + topfile].filename,
                     min(filelen,xlen),
                     x + 2,
                     0,
                     &attr,
                     0);
// VioWrtNAttr(&attr,filelen,x + 2,0,0);
    if(xlen > filelen)
      VioWrtCharStrAtt("...",
                       3,
                       x + 2,
                       filelen - 3,
                       &attr,
                       0);

    if(infolevel & I_SIZE) {

      char str[20];

      xlen = CommaFormat(str,sizeof(str),files[x + topfile].cbFile +
                                         ((files[x + topfile].cbList > 4) ?
                                          files[x + topfile].cbList / 2 : 0));
      VioWrtCharStrAtt(str,
                       xlen,
                       x + 2,
                       sizepos + (14 - xlen),
                       &attr,
                       0);
// VioWrtNAttr(&attr,14,x + 2,sizepos,0);
    }

    if(infolevel & I_DATE) {

      char str[22];

      sprintf(str,"%04u/%02u/%02u %02u:%02u:%02u",
              files[x + topfile].date.year + 1980,
              files[x + topfile].date.month,
              files[x + topfile].date.day,
              files[x + topfile].time.hours,
              files[x + topfile].time.minutes,
              files[x + topfile].time.twosecs * 2);
      VioWrtCharStrAtt(str,
                       19,
                       x + 2,
                       vio.col - 19,
                       &attr,
                       0);
// VioWrtNAttr(&attr,19,x + 2,vio.col - 19,0);
    }

    if(infolevel & I_ATTR) {

      char attrstr[] = "RHS\0DA",str[8];
      int  c,y;

      y = 0;
      for(c = 0;c < 6;c++)
        if(attrstr[c])
          str[y++] = ((files[x + topfile].attrFile & (1 << c)) ?
                       attrstr[c] : '-');
      str[y] = 0;
      VioWrtCharStrAtt(str,
                       y,
                       x + 2,
                       attrpos,
                       &attr,
                       0);
// VioWrtNAttr(&attr,5,x + 2,attrpos,0);
    }

    x++;
  }
}


void DrawFile (ULONG num) {

  int   xlen;
  char  attr,color;
  ULONG filelen,sizepos,attrpos;

  filelen = vio.col;
  if(infolevel & I_DATE)
    filelen -= 21;
  if(infolevel & I_SIZE) {
    filelen -= 16;
    sizepos = vio.col - 14;
    if(infolevel & I_DATE)
      sizepos -= 21;
    if(infolevel & I_ATTR)
      sizepos -= 7;
  }
  if(infolevel & I_ATTR) {
    filelen -= 7;
    attrpos = vio.col - 5;
    if(infolevel & I_DATE)
      attrpos -= 21;
  }

  if(num >= topfile && num < numfiles && num < ((vio.row - 5) + topfile)) {
    color = ((files[num].attrFile & FILE_DIRECTORY) != 0) ?
             (((files[num].flags & F_MARKED) != 0) ? 3 | 8 : 2 | 8) :
             (((files[num].flags & F_MARKED) != 0) ? 6 | 8 : 7);
    attr = (num == file) ? (color << 4) : color;
    xlen = strlen(files[num].filename);
    VioWrtCharStrAtt(files[num].filename,
                     min(filelen,xlen),
                     2 + (num - topfile),
                     0,
                     &attr,
                     0);
    if(xlen > filelen)
      VioWrtCharStrAtt("...",
                       3,
                       2 + (num - topfile),
                       filelen - 3,
                       &attr,
                       0);

    if(infolevel & I_SIZE) {

      char str[20];

      xlen = CommaFormat(str,sizeof(str),files[num].cbFile +
                                         ((files[num].cbList > 4) ?
                                          files[num].cbList / 2 : 0));
      VioWrtCharStrAtt(str,
                       xlen,
                       2 + (num - topfile),
                       sizepos + (14 - xlen),
                       &attr,
                       0);
    }

    if(infolevel & I_DATE) {

      char str[22];

      sprintf(str,"%04u/%02u/%02u %02u:%02u:%02u",
              files[num].date.year + 1980,
              files[num].date.month,
              files[num].date.day,
              files[num].time.hours,
              files[num].time.minutes,
              files[num].time.twosecs * 2);
      VioWrtCharStrAtt(str,
                       19,
                       2 + (num - topfile),
                       vio.col - 19,
                       &attr,
                       0);
    }

    if(infolevel & I_ATTR) {

      char attrstr[] = "RHS\0DA",str[8];
      int  c,y;

      y = 0;
      for(c = 0;c < 6;c++)
        if(attrstr[c])
          str[y++] = ((files[num].attrFile & (1 << c)) ?
                       attrstr[c] : '-');
      str[y] = 0;
      VioWrtCharStrAtt(str,
                       y,
                       2 + (num - topfile),
                       attrpos,
                       &attr,
                       0);
    }
  }
}


void DisableTimer (void) {

  timer = (clock_t)-1;
  *input = 0;
  DisplayTarget();
  ShowCursor(TRUE);
}


void AdjustLastDir (void) {

  char *p;

  p = strrchr(directory,'\\');
  if(p) {
    p++;
    strcpy(lastdir,p);
  }
  else
    *lastdir = 0;
}


int main (int argc,char *argv[]) {

  int         key,wasfile,attr = (7 << 8) | ' ';
  BOOL        okay = TRUE;

  Init(argc,argv);
  if((CheckShift() & KBDSTF_SCROLLLOCK_ON) != 0)  /* is scroll-lock on? */
    SimpleInput("Reminder:","Scroll Lock is on.",0,0,1000,NULL);

  while(okay) {
    key = get_ch(timer);
    if(timer != (clock_t)-1 && (key != 256 && ((key & 256) != 0)))
      DisableTimer();
ReSwitch:
    switch(key) {
      case 0:     /* timer was set and expired */
        DisableTimer();
        break;

      case 256:   /* shiftstate only */
        ThreadMsg(NULL);
        break;

      case 1:     /* ctrl+a -- About */
        DisableTimer();
        About();
        break;

      case 26:        /* ctrl+z -- reset tree */
        DisableTimer();
        TreeView("");
        SimpleInput("Okay","Tree reset.",0,0,1500,NULL);
        break;

      case 20:        /* ctrl+t -- set target via tree */
        DisableTimer();
        {
          char *p;

          p = TreeView(NULL);
          if(p && IsFile(p) == 0) {
            strcpy(target,p);
            freespace = FreeSpace(*target);
          }
          DisplayFreeSpace(*target);
          DisplayTarget();
          DisplayNumSizeFiles();
          DisplayCurrFile((numfiles) ? &files[file] : NULL);
          DisplayFiles();
        }
        break;

      case 07:        /* ctrl+g -- set directory via tree */
        DisableTimer();
        {
          char *p;

          p = TreeView(NULL);
          if(p && IsFile(p) == 0) {
            MakeCurrDir(p);
            CurrDir(directory);
            AdjustLastDir();
            BuildList("",TRUE,FALSE);
          }
          DisplayFreeSpace(*target);
          DisplayTarget();
          DisplayNumSizeFiles();
          DisplayCurrFile((numfiles) ? &files[file] : NULL);
          DisplayFiles();
        }
        break;

      case 44 | 256:  /* alt+z -- scan many drives */
        {
          BOOL        temprecurse = recurse,first = TRUE;
          char        tempdir[CCHMAXPATH],*p,c;
          static char drives[28] = "";
          int         ret;

          strcpy(tempdir,directory);
          recurse = TRUE;
          ShowDrives(vio.row - 3);
          VioWrtCharStr("Enter drives to scan below:",27,
                        vio.row - 3,vio.col - 27,0);
          if(!*drives) {
            p = drives;
            for(c = 'C';c < 'Z' + 1;c++) {
              if(IsValidDrive(c)) {
                *p = c;
                p++;
              }
            }
            *p = 0;
          }
          ret = EnterLine(drives,26,vio.row - 2,"",NULL,0,0);
          DisplayTarget();
          if(ret > 0) {
            p = drives;
            while(*p) {
              if(isalpha(*p) && IsValidDrive(toupper(*p))) {
                *directory = *p;
                directory[1] = ':';
                directory[2] = '\\';
                directory[3] = 0;
                BuildList("",first,TRUE);
                first = FALSE;
              }
              p++;
            }
            {
              char str[38];

              sprintf(str,"%lu",numfiles);
              VioWrtCharStr(str,strlen(str),vio.row / 2,
                            vio.col - strlen(str),0);
              VioWrtCharStr("Sorting... ",11,vio.row / 2,
                            (vio.col / 2) - 6,0);
              SortFiles();
            }
            DisplayNumSizeFiles();
            DisplayCurrFile((numfiles) ? &files[file] : NULL);
            DisplayFiles();
          }
          recurse = temprecurse;
          strcpy(directory,tempdir);
        }
        break;

      case 31 | 256:  /* alt+s -- recurse toggle */
        recurse = (recurse) ? FALSE : TRUE;
        BuildList("",TRUE,FALSE);
        DisplayNumSizeFiles();
        DisplayCurrFile((numfiles) ? &files[file] : NULL);
        DisplayFiles();
        break;

      case 35 | 256:  /* alt+h -- hide marked files */
        if(numfiles) {

          ULONG x;
          BOOL  once = FALSE;

          x = 0;
          while(x < numfiles) {
            if((files[x].flags & F_MARKED) != 0) {
              free(files[x].filename);
              if(x < numfiles - 1)
                memmove(&files[x],&files[x + 1],
                        sizeof(FILES) * numfiles - (x + 1));
              numfiles--;
              sizefiles -= files[x].cbFile + ((files[x].cbList > 4) ?
                                              files[x].cbList / 2 : 0);
              sizemarked -= files[x].cbFile + ((files[x].cbList > 4) ?
                                               files[x].cbList / 2 : 0);
              nummarked--;
              once = TRUE;
            }
            else
              x++;
          }
          if(!once) {
            free(files[file].filename);
            if(file < numfiles - 1)
              memmove(&files[file],&files[file + 1],
                      sizeof(FILES) * numfiles - (file + 1));
            sizefiles -= files[file].cbFile + ((files[file].cbList > 4) ?
                                               files[file].cbList / 2 : 0);
            numfiles--;
          }
          if(numfiles) {
            files = realloc(files,numfiles * sizeof(FILES));
            if(file > numfiles - 1)
              file = numfiles - 1;
            topfile = file - ((vio.row - 5) / 2);
            if((numfiles - 1) - topfile < vio.row - 5)
              topfile = numfiles - (vio.row - 5);
            if(topfile > file)
              topfile = 0;
          }
          else {
            free(files);
            files = NULL;
            topfile = file = 0;
          }
          DisplayNumSizeFiles();
          DisplayCurrFile((numfiles) ? &files[file] : NULL);
          DisplayFiles();
        }
        break;

      case 34 | 256:  /* alt+g -- goto another directory */
        {
          char   str[CCHMAXPATH + 2];
          char  *filename = NULL;
          int    ret;
          ULONG  len;

          if(numfiles)
            filename = files[file].filename;
          strcpy(str,directory);
          VioWrtCharStr("Enter directory to go to below:",31,vio.row - 3,0,0);
          ret = EnterLine(str,CCHMAXPATH,vio.row - 2,filename,
                          savedirs,11,0);
          DisplayTarget();
          if(ret > 0) {
            if(isalpha(*str) && str[1] == ':' && !str[2]) {
              len = CCHMAXPATH - 4;
              if(!DosQCurDir((ULONG)(toupper(*str) - '@'),str + 3,&len))
                str[2] = '\\';
            }
            strcpy(str,MakeFullName(str));
            if(*str && IsFile(str) == 0) {
              MakeCurrDir(str);
              CurrDir(directory);
              AdjustLastDir();
              BuildList("",TRUE,FALSE);
              DisplayNumSizeFiles();
              DisplayCurrFile((numfiles) ? &files[file] : NULL);
              DisplayFiles();
            }
            else if(*str) {

              char s[CCHMAXPATH + 80];

              sprintf(s,"Can't switch to \"%s\"",str);
              SimpleInput("Hmmph!",s,100,50,2000,NULL);
            }
          }
        }
        break;

      case 23 | 256:  /* alt+i -- drive information */
        Info();
        break;

      case 49 | 256:  /* alt+n -- create directory */
        {
          char   str[CCHMAXPATH + 2];
          char  *filename = NULL;
          int    ret;
          APIRET rc;

          if(numfiles)
            filename = files[file].filename;
          strcpy(str,directory);
          VioWrtCharStr("Enter directory name to make below:",35,vio.row - 3,0,0);
          ret = EnterLine(str,CCHMAXPATH,vio.row - 2,filename,NULL,0,0);
          DisplayTarget();
          if(ret > 0) {
            strcpy(str,MakeFullName(str));
            if(IsFile(str) == -1) {
              rc = DosCreateDir(str,NULL);
              if(rc) {

                char s[CCHMAXPATH + 80];

                sprintf(s,"SYS%04lu: Can't create \"%s\"",rc,str);
                SimpleInput("Argh!",s,100,50,2500,NULL);
              }
            }
            else
              SimpleInput("Ahem","That name's already taken.",
                          100,50,1500,NULL);
          }
        }
        break;

      case 30 | 256:  /* alt+a, or... */
      case 133 | 256: /* F11 -- attribute edit */
        if(numfiles) {
          if(EditAttributes(&files[file]) > 0) {
            DisplayCurrFile(&files[file]);
            DrawFile(file);
          }
        }
        break;

      case 134 | 256: /* F12 -- touch filedates */
        StartThread(C_TOUCH);
        ThreadMsg(NULL);
        break;

      case 20 | 256:  /* alt+t -- type in target directory */
        {
          char   str[CCHMAXPATH + 2];
          char  *filename = NULL;
          int    ret;
          ULONG  len;

          if(numfiles)
            filename = files[file].filename;
          strcpy(str,target);
          VioWrtCharStr("Enter new target directory below:",33,vio.row - 3,0,0);
          ret = EnterLine(str,CCHMAXPATH,vio.row - 2,filename,
                          savedirs,11,0);
          if(ret > 0) {
            if(isalpha(*str) && str[1] == ':' && !str[2]) {
              len = CCHMAXPATH - 4;
              if(!DosQCurDir((ULONG)(toupper(*str) - '@'),str + 3,&len))
                str[2] = '\\';
            }
            strcpy(str,MakeFullName(str));
            if(!IsFile(str) || (IsRoot(str) && IsValidDrive(*str)) ||
               !DosCreateDir(str,NULL)) {
              strcpy(target,str);
              freespace = FreeSpace(*target);
              DisplayFreeSpace(*target);
            }
            else {

              char s[CCHMAXPATH + 80];

              sprintf(s,"Couldn't set directory \"%s\"",str);
              SimpleInput("Oops",s,100,50,1500,NULL);
            }
          }
          DisplayTarget();
        }
        break;

      case '<':       /* unmark by mask */
      case '>':       /* mark by mask */
        DisableTimer();
        if(numfiles) {

          ULONG x;
          char  str[CCHMAXPATH + 2];
          char *filename = NULL,*p;
          int   ret;

          filename = files[file].filename;
          p = strrchr(filename,'.');
          sprintf(str,"*%s",(p) ? p : ".");
          VioWrtCharStr("Enter marking mask below:",25,vio.row - 3,0,0);
          ret = EnterLine(str,CCHMAXPATH,vio.row - 2,filename,NULL,0,0);
          DisplayTarget();
          if(ret > 0) {
            for(x = 0;x < numfiles;x++) {
              if(MultiWildCard(files[x].filename,str)) {
                if(key == '>') {
                  if((files[x].flags & F_MARKED) == 0) {
                    sizemarked += files[x].cbFile + ((files[x].cbList > 4) ?
                                                     files[x].cbList / 2 : 0);
                    nummarked++;
                    files[x].flags |= F_MARKED;
                  }
                }
                else {
                  if((files[x].flags & F_MARKED) != 0) {
                    sizemarked -= files[x].cbFile + ((files[x].cbList > 4) ?
                                                     files[x].cbList / 2 : 0);
                    nummarked--;
                    files[x].flags &= (~F_MARKED);
                  }
                }
              }
            }
            DisplayFiles();
          }
          DisplayNumSizeFiles();
        }
        break;

      case '|':       /* invert marking */
      case '?':       /* unmark all */
      case '/':       /* mark all */
        DisableTimer();
        if(numfiles) {

          ULONG x;

          for(x = 0;x < numfiles;x++) {
            switch(key) {
              case '/':
                if((files[x].attrFile & FILE_DIRECTORY) ==
                   (files[file].attrFile & FILE_DIRECTORY)) {
                  if((files[x].flags & F_MARKED) == 0) {
                    sizemarked += files[x].cbFile + ((files[x].cbList > 4) ?
                                                     files[x].cbList / 2 : 0);
                    nummarked++;
                    files[x].flags |= F_MARKED;
                  }
                }
                break;
              case '?':
                if((files[x].flags & F_MARKED) != 0) {
                  sizemarked -= files[x].cbFile + ((files[x].cbList > 4) ?
                                                   files[x].cbList / 2 : 0);
                  nummarked--;
                  files[x].flags &= (~F_MARKED);
                }
                break;
              case '|':
                if(files[x].flags & F_MARKED) {
                  files[x].flags &= (~F_MARKED);
                  sizemarked -= files[x].cbFile + ((files[x].cbList > 4) ?
                                                   files[x].cbList / 2 : 0);
                  nummarked--;
                }
                else {
                  files[x].flags |= F_MARKED;
                  sizemarked += files[x].cbFile + ((files[x].cbList > 4) ?
                                                   files[x].cbList / 2 : 0);
                  nummarked++;
                }
                break;
            }
          }
          DisplayNumSizeFiles();
          DisplayFiles();
        }
        break;

      case 89 | 256:  /* sF6 -- sort filename */
        if(sorttype == S_NAME)
          sorttype = S_FULLNAME;
        else
          sorttype = S_NAME;
ReSort:
        if(numfiles) {

          ULONG x;
          char  filename[CCHMAXPATH];

          strcpy(filename,files[file].filename);
          {
            char str[81];

            sprintf(str,"Sorting by %s",
                    (sorttype == S_SIZE) ? "size" :
                     (sorttype == S_DATE) ? "date" :
                      (sorttype == S_FULLNAME) ? "full name" :
                       "name");
            ThreadMsg(str);
          }

          file = 0;
          SortFiles();
          for(x = 0;x < numfiles;x++) {
            if(!stricmp(files[x].filename,filename)) {
              file = x;
              break;
            }
          }
          topfile = file - ((vio.row - 5) / 2);
          if((numfiles - 1) - topfile < vio.row - 5)
            topfile = numfiles - (vio.row - 5);
          if(topfile > file)
            topfile = 0;
          DisplayFiles();
          DisplayCurrFile(&files[file]);

          ThreadMsg(NULL);
        }
        break;

      case 90 | 256:  /* sF7 -- sort size */
        sorttype = S_SIZE;
        goto ReSort;

      case 91 | 256:  /* sF8 -- sort date */
        sorttype = S_DATE;
        goto ReSort;

      case 93 | 256:  /* sF10 -- toggle info level */
        infolevel++;
        if(infolevel > 7)
          infolevel = 0;
        DisplayFiles();
        break;

      case 135 | 256: /* sF11 -- toggle hidden/system find */
        if((findattr & FILE_HIDDEN) != 0)
          findattr &= (~(FILE_HIDDEN | FILE_SYSTEM));
        else
          findattr |= (FILE_HIDDEN | FILE_SYSTEM);
        key = 60 | 256;
        goto ReSwitch;

      case 136 | 256: /* sF12 -- toggle directories at top */
        dirstop = (dirstop) ? FALSE : TRUE;
        goto ReSort;

      case 94 | 256:  /* ctrl+F1 -- set target directory */
        strcpy(target,directory);
        freespace = FreeSpace(*target);
        DisplayFreeSpace(*target);
        DisplayTarget();
        DisplayNumSizeFiles();
        break;

      case 95 | 256:    /* ctrl+F2 - ctrl+F12 -- save directories */
      case 96 | 256:
      case 97 | 256:
      case 98 | 256:
      case 99 | 256:
      case 100 | 256:
      case 101 | 256:
      case 102 | 256:
      case 103 | 256:
      case 137 | 256:
      case 138 | 256:
        {
          int x;

          x = (key < (137 | 256)) ? key - (95 | 256) :
                                    10 + (key - (137 | 256));
          if(savedirs[x])
            free(savedirs[x]);
          savedirs[x] = strdup(directory);
          SimpleInput("Okay","Saved directory",0,0,750,NULL);
        }
        break;

      case 104 | 256:   /* alt+F1 -- list saved directories */
        ShowFKeys();
        break;

      case 105 | 256:   /* alt+F2 - alt+F12 -- recall directories */
      case 106 | 256:
      case 107 | 256:
      case 108 | 256:
      case 109 | 256:
      case 110 | 256:
      case 112 | 256:
      case 113 | 256:
      case 139 | 256:
      case 140 | 256:
        {
          int x;

          x = (key < (139 < 256)) ? key - (105 | 256) :
                                    10 + (key - (139 | 256));
          if(savedirs[x]) {
            MakeCurrDir(savedirs[x]);
            CurrDir(directory);
            AdjustLastDir();
            BuildList("",TRUE,FALSE);
            DisplayNumSizeFiles();
            DisplayCurrFile((numfiles) ? &files[file] : NULL);
            DisplayFiles();
          }
          else
            DosBeep(50,100);
        }
        break;

      case 46 | 256:  /* alt+c, or... */
      case 64 | 256:  /* F6 -- copy files */
        StartThread(C_COPY);
        ThreadMsg(NULL);
        break;

      case 50 | 256:  /* alt+m, or... */
      case 65 | 256:  /* F7 -- move files */
        StartThread(C_MOVE);
        ThreadMsg(NULL);
        break;

      case 25 | 256:  /* alt+p -- print files */
        StartThread(C_PRINT);
        ThreadMsg(NULL);
        break;

      case 19 | 256:  /* alt+r, or... */
      case 66 | 256:  /* F8, rename file */
        {
          char   str[CCHMAXPATH + 2],*p;
          char  *filename = NULL;
          int    ret;
          APIRET rc;

          if(numfiles) {
            filename = files[file].filename;
            *str = 0;
            p = strrchr(files[file].filename,'\\');
            if(p) {
              strcpy(str,files[file].filename);
              p = strrchr(str,'\\');
              p++;
              *p = 0;
            }
            VioWrtCharStr("Enter new filename below:",25,vio.row - 3,0,0);
            ret = EnterLine(str,CCHMAXPATH,vio.row - 2,filename,NULL,0,0);
            DisplayTarget();
            if(ret > 0 && strcmp(str,files[file].filename)) {
              rc = DosMove(files[file].filename,str);
              if(rc) {
                if(!strchr(str,'\\') &&
                   WriteLongName(files[file].filename,str)) {
                  SimpleInput("Notice","Rename failed, but .LONGNAME was set.",
                              400,10,1000,NULL);
                  DisplayCurrFile((numfiles) ? &files[file] : NULL);
                  DrawFile(file);
                }
                else {
                  sprintf(str,"SYS%04lu: Rename failed.",rc);
                  SimpleInput("Notice",str,100,50,2000,NULL);
                }
              }
              else
                DosBeep(1000,10);
            }
          }
        }
        break;

      case 84 | 256:  /* sF1, swap target/current directories */
        {
          char temp[CCHMAXPATH];

          strcpy(temp,target);
          strcpy(target,directory);
          strcpy(directory,temp);
          freespace = FreeSpace(*target);
          DisplayFreeSpace(*target);
          DisplayTarget();
          DisplayNumSizeFiles();
          MakeCurrDir(directory);
          CurrDir(directory);
          AdjustLastDir();
          BuildList("",TRUE,FALSE);
          DisplayNumSizeFiles();
          DisplayCurrFile((numfiles) ? &files[file] : NULL);
          DisplayFiles();
        }
        break;

      case 4:         /* ctrl+d -- permanently delete files */
        DisableTimer();
        StartThread(C_DELETEPERM);
        ThreadMsg(NULL);
        break;

      case 83 | 256:  /* delete -- delete files */
        StartThread(C_DELETE);
        ThreadMsg(NULL);
        break;

      case 36 | 256:  /* alt+j */
      case 92 | 256:  /* sF9, shell to OS */
        Shell(NULL,FALSE);
        break;

      case 6:         /* ctrl+f, or... */
      case 33 | 256:  /* alt+f, set filter */
        DisableTimer();
        {
          char         str[CCHMAXPATH + 2];
          char        *filename = NULL;
          int          ret,x;

          if(numfiles)
            filename = files[file].filename;
          strcpy(str,filter);
          VioWrtCharStr("Enter filter below:",19,vio.row - 3,0,0);
          ret = EnterLine(str,CCHMAXPATH,vio.row - 2,filename,
                          lastfilter,25,lastf);
          DisplayTarget();
          if(ret >= 0 && stricmp(str,filter)) {
            if(!stricmp(str,"*")) {
              for(x = 0;x < 25;x++) {
                if(lastfilter[x] && !stricmp(str,lastfilter[x]))
                  break;
              }
              if(x == 25) {
                if(lastfilter[lastf]) {
                  lastf++;
                  if(lastf > 24)
                    lastf = 0;
                }
                if(lastfilter[lastf])
                  free(lastfilter[lastf]);
                lastfilter[lastf] = strdup(str);
                if(!lastfilter[lastf]) {
                  lastf--;
                  if(lastf < 0)
                    lastf = 24;
                }
              }
            }
            strcpy(filter,(ret > 0 && *str) ? str : "*");
            key = 60 | 256;
            goto ReSwitch;
          }
        }
        break;

      case 41 | 256:  /* a` */
      case 120 | 256: /* a1 */
      case 121 | 256: /* a2 */
      case 122 | 256: /* a3 */
      case 123 | 256: /* a4 */
      case 124 | 256: /* a5 */
      case 125 | 256: /* a6 */
      case 126 | 256: /* a7 */
      case 127 | 256: /* a8 */
      case 128 | 256: /* a9 */
      case 129 | 256: /* a0 */
      case 130 | 256: /* a- */
      case 131 | 256: /* a= */
      case '\t':
      case 67 | 256:  /* F9, command line */
        DisableTimer();
        {
          static char  str[1002];
          char        *filename = NULL;
          int          ret,x;

          if(key == '\t')
            key = (67 | 256);
          if(numfiles)
            filename = files[file].filename;
          *str = 0;
          if(key == (41 | 256) && usercmds[0]) {
            strcpy(str,usercmds[0]);
            strcat(str," ");
          }
          else if(key != (67 | 256) && usercmds[key - (119 | 256)]) {
            strcpy(str,usercmds[key - (119 | 256)]);
            strcat(str," ");
          }
          if(!*str && lastcmdline[lastcmd])
            strcpy(str,lastcmdline[lastcmd]);
          str[1000] = 0;
          VioWrtCharStr("Enter command line below:",25,vio.row - 3,0,0);
          ret = EnterLine(str,1000,vio.row - 2,filename,lastcmdline,25,lastcmd);
          DisplayTarget();
          if(ret > 0) {
            if(key == (67 | 256)) {
              for(x = 0;x < 25;x++) {
                if(lastcmdline[x] && !stricmp(str,lastcmdline[x]))
                  break;
              }
              if(x == 25) {
                if(lastcmdline[lastcmd]) {
                  lastcmd++;
                  if(lastcmd > 24)
                    lastcmd = 0;
                }
                if(lastcmdline[lastcmd])
                  free(lastcmdline[lastcmd]);
                lastcmdline[lastcmd] = strdup(str);
                if(!lastcmdline[lastcmd]) {
                  lastcmd--;
                  if(lastcmd < 0)
                    lastcmd = 24;
                }
              }
            }
            Shell(str,TRUE);
          }
        }
        break;

      case 32 | 256:  /* alt+d, or... */
      case 68 | 256:  /* F10, change drive */
        {
          int   responses[] = {'a','A','b','B','c','C','d','D','e','E',
                               'f','F','g','G','h','H','i','I','j','J',
                               'k','K','l','L','m','M','n','N','o','O',
                               'p','P','q','Q','r','R','s','S','t','T',
                               'u','U','v','V','w','W','x','X','y','Y',
                               'z','Z','\x1b',0};

          ShowDrives(vio.row - 3);
          key = SimpleInput("Change drive","Enter a drive letter:",
                            0,0,0,responses);
          DisplayTarget();
          if(key == '\x1b')
            break;
          DosError(FERR_DISABLEHARDERR);
          if(IsValidDrive(key)) {
            DosSelectDisk(toupper(key) - '@');
            CurrDir(directory);
            AdjustLastDir();
            BuildList("",TRUE,FALSE);
            DisplayNumSizeFiles();
            DisplayCurrFile((numfiles) ? &files[file] : NULL);
            DisplayFiles();
          }
          else
            SimpleInput("<Cough>","That's not a valid drive letter...",
                        100,50,2000,NULL);
        }
        break;

      case '\\':      /* Go to root directory */
        if(timer != (clock_t)-1 && numfiles)
          goto Default;

        directory[3] = 0;
        MakeCurrDir(directory);
        *lastdir = 0;
        BuildList("",TRUE,FALSE);
        DisplayNumSizeFiles();
        DisplayCurrFile((numfiles) ? &files[file] : NULL);
        DisplayFiles();
        break;

      case '\r':    /* Return key -- switch to directory or exec/view file */
        DisableTimer();
        if(numfiles) {

          char *p;

          if(files[file].attrFile & FILE_DIRECTORY) {
            p = strrchr(files[file].filename,'\\');
            if(p)
              strcpy(directory,p);
            else {
              if(directory[strlen(directory) - 1] != '\\')
                strcat(directory,"\\");
              strcat(directory,files[file].filename);
              strcpy(lastdir,files[file].filename);
            }
            MakeCurrDir(directory);
            BuildList("",TRUE,FALSE);
            DisplayNumSizeFiles();
            DisplayCurrFile((numfiles) ? &files[file] : NULL);
            DisplayFiles();
          }
          else {

            char str[CCHMAXPATH + 2];

            if(!strchr(files[file].filename,':'))
              sprintf(str,"%s%s%s",
                      directory,
                      (directory[strlen(directory) - 1] != '\\') ? "\\" : "",
                      files[file].filename);
            else
              strcpy(str,files[file].filename);
            if(!Associate(str)) {
              p = strrchr(str,'\\');
              if(p)
                p++;
              else
                p = str;
              if(!strchr(p,'.'))
                strcat(str,".");
              if(IsExecutable(str)) {
                p = (strchr(files[file].filename,' ')) ? "\"" : "";
                sprintf(str,"%s%s%s",p,files[file].filename,p);
                Shell(str,TRUE);
              }
              else {
                key = 62 | 256;
                goto ReSwitch;
              }
            }
          }
        }
        break;

      case ' ':   /* toggle marking on file object */
        if(timer != (clock_t)-1 && numfiles)
          goto Default;

        if(numfiles) {
          if(files[file].flags & F_MARKED) {
            files[file].flags &= (~F_MARKED);
            nummarked--;
            sizemarked -= (files[file].cbFile +
                           ((files[file].cbList > 4) ? files[file].cbList / 2 :
                                                       0));
          }
          else {
            files[file].flags |= F_MARKED;
            nummarked++;
            sizemarked += (files[file].cbFile +
                           ((files[file].cbList > 4) ? files[file].cbList / 2 :
                                                       0));
          }
          DrawFile(file);
          DisplayNumSizeFiles();
          key = 80 | 256;
          goto ReSwitch;
        }
        break;

      case 59 | 256:    /* F1 -- help */
        {
          char helpname[CCHMAXPATH];

          helping = TRUE;
          sprintf(helpname,"%s%sFMTHLP.TXT",homedir,
                  (homedir[strlen(homedir) - 1] != '\\') ? "\\" : "");
          if(ViewFile(helpname) == -1) {
            helping = FALSE;
            ThreadMsg(NULL);
            key = 45 | 256;
            goto ReSwitch;
          }
          helping = FALSE;
          ThreadMsg(NULL);
        }
        break;

      case 22:        /* ctrl+v, or... */
      case 87 | 256:  /* sF4, set viewer */
        DisableTimer();
        {
          char  str[CCHMAXPATH + 2];
          char *filename = NULL;
          int   ret;

          if(numfiles)
            filename = files[file].filename;
          strcpy(str,(viewer) ? viewer : "");
          VioWrtCharStr("Enter viewer below:",19,vio.row - 3,0,0);
          ret = EnterLine(str,CCHMAXPATH,vio.row - 2,filename,NULL,0,0);
          DisplayTarget();
          if(ret >= 0) {
            if(!*str) {
              if(viewer)
                free(viewer);
              viewer = NULL;
            }
            else {
              if(viewer)
                free(viewer);
              viewer = strdup(str);
            }
          }
        }
        break;

      case 5:         /* ctrl+e, or... */
      case 88 | 256:  /* sF5, set editor */
        DisableTimer();
        {
          char  str[CCHMAXPATH + 2];
          char *filename = NULL;
          int   ret;

          if(numfiles)
            filename = files[file].filename;
          strcpy(str,(editor) ? editor : "");
          VioWrtCharStr("Enter editor below:",19,vio.row - 3,0,0);
          ret = EnterLine(str,CCHMAXPATH,vio.row - 2,filename,NULL,0,0);
          DisplayTarget();
          if(ret >= 0) {
            if(!*str) {
              if(editor)
                free(editor);
              editor = NULL;
            }
            else {
              if(editor)
                free(editor);
              editor = strdup(str);
            }
          }
        }
        break;

      case 85 | 256:    /* sF2 -- rewrite FMT.CFG */
        WriteFMTCFG();
        SimpleInput("Okay","FMT.CFG written.",0,0,1000,NULL);
        break;

      case 18:          /* ctrl+r, or... */
      case 60 | 256:    /* F2 -- rescan */
        DisableTimer();
        {
          char   filename[CCHMAXPATH];
          ULONG  size,z;
          USHORT date,Time;
          ULONG x;
          int   ret;
          union {
            USHORT x;
            FDATE  d;
          } d;
          union {
            USHORT x;
            FTIME  t;
          } t;
          ULONG attrFile;

          *filename = 0;
          if(numfiles) {  /* save info on current pos */
            strcpy(filename,files[file].filename);
            size = files[file].cbFile + ((files[file].cbList > 4) ?
                     files[file].cbList / 2 : 0);
            d.d = files[file].date;
            t.t = files[file].time;
            date = d.x;
            Time = t.x;
            attrFile = files[file].attrFile;
          }
          BuildList("",TRUE,FALSE);
          /* attempt to find same or close pos in list */
          if(*filename && numfiles) {
            for(x = 0;x < numfiles;x++) {
              if((files[x].attrFile & FILE_DIRECTORY) ==
                 (attrFile & FILE_DIRECTORY)) {
                if(sorttype == S_NAME) {
                  ret = stricmp(files[x].filename,filename);
                  if(!ret ||
                     ((invertsort == FALSE) ? ret > 0 : ret < 0)) {
                    file = x;
                    break;
                  }
                }
                else if(sorttype == S_SIZE) {
                  z = files[x].cbFile + ((files[x].cbList > 4) ?
                        files[x].cbList / 2 : 0);
                  if(size == z || ((invertsort == FALSE) ? z > size : z < size)) {
                    file = x;
                    break;
                  }
                }
                else if(sorttype == S_DATE) {
                  d.d = files[x].date;
                  t.t = files[x].time;
                  if(d.x == date && t.x == Time) {
                    file = x;
                    break;
                  }
                  if(!invertsort) {
                    if(d.x > date || (d.x == date && t.x > Time)) {
                      file = x;
                      break;
                    }
                  }
                  else {
                    if(d.x < date || (d.x == date && t.x < Time)) {
                      file = x;
                      break;
                    }
                  }
                }
              }
            }
            topfile = file - ((vio.row - 5) / 2);
            if((numfiles - 1) - topfile < vio.row - 5)
              topfile = numfiles - (vio.row - 5);
            if(topfile > file)
              topfile = 0;
          }
          DisplayNumSizeFiles();
          DisplayCurrFile((numfiles) ? &files[file] : NULL);
          DisplayFiles();
        }
        break;

      case 45 | 256:    /* alt+x, or... */
        if(threads) {

          int responses[] = {'Y','y','N','n','\r','\x1b',0};

          key = SimpleInput("Confirm exit",
                            "Background threads are still running.  Exit anyway?",
                            100,500,0,responses);
          if(key == 'N' || key == 'n' || key == '\x1b')
            break;
        }
        /* else intentional fallthru */
      case 61 | 256:    /* F3 -- exit program */
        okay = FALSE;
        break;

      case 47 | 256:    /* alt+v, or... */
      case 62 | 256:    /* F4 -- view file */
        if(numfiles) {
          if(viewer) {

            static char str[1024];
            char       *p;

            p = (strchr(files[file].filename,' ')) ? "\"" : "";
            sprintf(str,"%s %s%s%s",viewer,p,files[file].filename,p);
            Shell(str,FALSE);
          }
          else if(ViewFile(files[file].filename) == -1) {
            key = 45 | 256;
            goto ReSwitch;
          }
        }
        break;

      case 18 | 256:    /* alt+e, or... */
      case 63 | 256:    /* F5 -- edit file */
        if(numfiles) {

          static char str[1024];
          char       *p;

          p = (strchr(files[file].filename,' ')) ? "\"" : "";
          sprintf(str,"%s %s%s%s",editor,p,files[file].filename,p);
          Shell(str,FALSE);
        }
        break;

      case 72 | 256:    /* up -- move in list */
        if(numfiles && file) {
          file--;
          DrawFile(file + 1);
          if(file < topfile) {
            VioScrollDn(2,0,vio.row - 4,vio.col - 1,1,(char *)&attr,0);
            topfile--;
          }
          DrawFile(file);
          DisplayCurrFile(&files[file]);
        }
        break;

      case 80 | 256:    /* down -- move in list*/
        if(numfiles && file < (numfiles - 1)) {
          file++;
          DrawFile(file - 1);
          if(file > topfile + (vio.row - 6)) {
            VioScrollUp(2,0,vio.row - 4,vio.col - 1,1,(char *)&attr,0);
            topfile++;
          }
          DrawFile(file);
          DisplayCurrFile(&files[file]);
        }
        break;

      case 77 | 256:    /* right -- go up one directory level */
        if(numfiles) {
          if(files[file].attrFile & FILE_DIRECTORY) {
            key = '\r';
            goto ReSwitch;
          }
          else
            DosBeep(50,100);
        }
        break;

      case '\x1b':      /* Escape, or... */
        if(timer != (clock_t)-1 && numfiles) {
          key = 25;  /* ctrl+y */
          goto Default;
        }
        /* else intentional fallthru */
      case 75 | 256:    /* left -- back one directory level */
        DisableTimer();
        {
          char *p;
          ULONG x;

          p = strrchr(directory,'\\');
          if(p && *(p + 1)) {
            if(p - directory == 2)
              p++;
            *p = 0;
            MakeCurrDir(directory);
            BuildList("",TRUE,FALSE);
            DisplayNumSizeFiles();
            if(numfiles && *lastdir) {
              for(x = 0;x < numfiles;x++) {
                p = strrchr(files[x].filename,'\\');
                if(p)
                  p++;
                else
                  p = files[x].filename;
                if(!stricmp(lastdir,p)) {
                  file = x;
                  break;
                }
              }
              if(file) {
                topfile = file - ((vio.row - 5) / 2);
                if((numfiles - 1) - topfile < vio.row - 5)
                  topfile = numfiles - (vio.row - 5);
                if(topfile > file)
                  topfile = 0;
              }
            }
            DisplayCurrFile((numfiles) ? &files[file] : NULL);
            DisplayFiles();
          }
          AdjustLastDir();
        }
        break;

      case 81 | 256:    /* page down -- move in list */
        if(numfiles) {
          file += (vio.row - 5);
          if(file > (numfiles - 1))
            file = numfiles - 1;
          topfile = file;
          if((numfiles - 1) - file < vio.row - 5)
            topfile = numfiles - (vio.row - 5);
          if(topfile > file)
            topfile = file;
          DisplayFiles();
          DisplayCurrFile(&files[file]);
        }
        break;

      case 73 | 256:    /* page up -- move in list */
        if(numfiles) {
          file -= (vio.row - 5);
          if(file > numfiles)
            file = 0;
          topfile -= (vio.row - 5);
          if(topfile > file)
            topfile = 0;
          DisplayFiles();
          DisplayCurrFile(&files[file]);
        }
        break;

      case 119 | 256:   /* ctrl+home -- top of list */
        if(numfiles) {
          file = topfile = 0;
          DisplayFiles();
          DisplayCurrFile(&files[file]);
        }
        break;

      case 71 | 256:    /* home -- move in list */
        if(numfiles) {
          if(file == topfile) {
            key = 119 | 256;
            goto ReSwitch;
          }
          wasfile = file;
          file = topfile;
          DrawFile(wasfile);
          DrawFile(file);
          DisplayCurrFile(&files[file]);
        }
        break;

      case 117 | 256:   /* ctrl+end -- bottom of list */
        if(numfiles) {
          topfile = numfiles - (vio.row - 5);
          file = numfiles - 1;
          DisplayFiles();
          DisplayCurrFile(&files[file]);
        }
        break;

      case 79 | 256:    /* end -- move in list */
        if(numfiles) {
          wasfile = file;
          file = topfile + (vio.row - 6);
          if(file > numfiles - 1)
            file = numfiles - 1;
          if(wasfile == file) {
            key = 117 | 256;
            goto ReSwitch;
          }
          DrawFile(wasfile);
          DrawFile(file);
          DisplayCurrFile(&files[file]);
        }
        break;

      default:  /* find matching start in list? */
Default:
        if(!(key & 256) && numfiles) {

          ULONG  x,wastop = topfile,ilen,wl,tpos;
          char  *first;
          BOOL   full;

          ilen = strlen(input);
          switch(key) {
            case 25:  /* ctrl+y */
              *input = 0;
              ilen = 0;
              break;

            case '\b':
              if(ilen) {
                ilen--;
                input[ilen] = 0;
              }
              break;

            default:
              if(ilen < CCHMAXPATH - 1) {
                input[ilen++] = (char)key;
                input[ilen] = 0;
              }
              else
                DosBeep(50,50);
              break;
          }
          if(!*input) {
            DisableTimer();
            break;
          }
          else {
            if(timer == (clock_t)-1)
              ShowCursor(FALSE);
            wl = 0;
            if(ilen < wl)
              wl = ilen;
            if(ilen >= wl + vio.col)
              wl = (ilen - vio.col) + 1;
            /* set cursor position */
            VioSetCurPos(vio.row - 3,ilen - wl,0);
            tpos = min(vio.col,ilen - wl);
            VioWrtCharStr(input + wl,tpos,vio.row - 3,0,0);
            if(tpos < vio.col)
              VioWrtNChar(" ",vio.col - tpos,vio.row - 3,tpos,0);
          }

          timer = 3000;
          wasfile = file;
          full = (strchr(input,':') != NULL);
          for(x = 0;x < numfiles;x++) {
            if(!full) {
              first = (sorttype == S_FULLNAME ||
                       (files[x].attrFile & FILE_DIRECTORY) != 0) ?
                         NULL : strrchr(files[x].filename,'\\');
              if(first)
                first++;
              else
                first = files[x].filename;
            }
            else
              first = files[x].filename;
            if(!strnicmp(first,input,ilen)) {
              file = x;
              break;
            }
          }
          if(wasfile != file) {
            topfile = file - ((vio.row - 5) / 2);
            if((numfiles - 1) - topfile < vio.row - 5)
              topfile = numfiles - (vio.row - 5);
            if(topfile > file)
              topfile = 0;
            if(topfile == wastop) {
              DrawFile(wasfile);
              DrawFile(file);
            }
            else
              DisplayFiles();
            DisplayCurrFile(&files[file]);
          }
          else if(x >= numfiles)
            DosBeep(250,25);
        }
        break;
    }
  }

  shelled = TRUE;
  DeInit();

  return 0;
}

