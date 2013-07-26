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

#pragma alloc_text(COMMON,SetupConsole,ThreadID,strip_trail_char)
#pragma alloc_text(COMMON,strip_lead_char,WriteCenterString,CommaFormat)
#pragma alloc_text(COMMON,MakeFullName,CurrDir,MakeCurrDir,IsFile)
#pragma alloc_text(COMMON,IsValidDrive)
#pragma alloc_text(FMT,Shell,FreeSpace,WildCard,MultiWildCard,CheckShift)
#pragma alloc_text(FMT,IsExecutable,stristr,Associate,ShowDrives)

extern void ThreadMsg (char *str);


void SetupConsole (void) {

  /* set keyboard into binary mode and turn on shift reporting */
  KBDINFO kbdinfo;

  memset(&kbdinfo,0,sizeof(kbdinfo));
  kbdinfo.cb = sizeof(kbdinfo);
  if(!KbdGetStatus(&kbdinfo,0)) {
    kbdinfo.fsMask &= (~KEYBOARD_ASCII_MODE);
    kbdinfo.fsMask |= (KEYBOARD_BINARY_MODE | KEYBOARD_SHIFT_REPORT);
    KbdSetStatus(&kbdinfo,0);
  }
  /* also reset some video attributes */
  VioSetMode(&vio,0);
  {
    VIOINTENSITY vii;

    vii.cb = sizeof(vii);
    vii.type = 2;
    vii.fs = 1;
    VioSetState(&vii,0);
  }
  ShowCursor(TRUE);
}


USHORT CheckShift (void) {

  KBDINFO kbdi;

  kbdi.cb = sizeof(kbdi);
  if(!KbdGetStatus(&kbdi,0))
    return kbdi.fsState;
  return 0;
}


int Shell (char *cmdline,BOOL pause) {

  int         attr = (7 << 8) | ' ';
  char       *savescreen = NULL;
  int         ret = -1;
  static char cmdline2[1024];

  shelled = TRUE;
  savescreen = SaveScreen(savescreen);
  VioScrollUp(0,0,-1,-1,-1,(char *)&attr,0);
  VioSetCurPos(0,0,0);
  VioSetState(&vioint,0); /* put states back */
  KbdSetStatus(&kbdorg,0);
  ShowCursor(FALSE);
  if(!cmdline) {
    cmdline = getenv("COMSPEC");
    if(!cmdline)
      cmdline = getenv("OS2_SHELL");
    if(!cmdline)
      cmdline = "CMD.EXE";
  }
  if((CheckShift() & KBDSTF_SCROLLLOCK_ON) != 0) {  /* is scroll-lock on? */
    strcpy(cmdline2,"START ");
    strcat(cmdline2,cmdline);
    ret = system(cmdline2);
  }
  else {
    ret = system(cmdline);
    if(pause) {
      printf("\n\x1b[0m\r%s  ",sayanykey);
      fflush(stdout);
      while(get_ch(-1) == 256)
      ;
    }
  }
  KbdFlushBuffer(0);
  ShowCursor(TRUE);
  RestoreScreen(savescreen);
  free(savescreen);
  SetupConsole();
  VioWrtNChar(" ",vio.col,vio.row - 1,0,0);
  shelled = FALSE;
  ThreadMsg(NULL);
  return ret;
}


int ThreadID (void) {

  TIB *pTib = NULL;
  PIB *pPib = NULL;

  if(!DosGetInfoBlocks(&pTib,&pPib))
    return (int)pTib->tib_ptib2->tib2_ultid;
  return -1;
}


void ShowDrives (USHORT y) {

  char   str[162],*p;
  APIRET rc;
  ULONG  ulDriveNum,ulDriveMap,x;
  BOOL   lastblank = FALSE;

  rc = DosQCurDisk(&ulDriveNum,&ulDriveMap);
  strcpy(str,"Available drives: ");
  p = str + strlen(str);
  if(!rc) {
    for(x = 0;x < 26;x++) {
      if((ulDriveMap & (1L << x)) != 0) {
        *p = (char)(x + 'A');
        p++;
        *p = 0;
        lastblank = FALSE;
      }
      else if(!lastblank) {
        lastblank = TRUE;
        *p = ' ';
        p++;
        *p = 0;
      }
    }
    VioWrtNChar(" ",vio.col,y,0,0);
    VioWrtCharStr(str,min(strlen(str),vio.col),y,0,0);
  }
}


char * strip_trail_char (char *strip,char *a) {

  register char *p;

  if(a && *a && strip && *strip) {
    p = &a[strlen(a) - 1];
    while (*a && strchr(strip,*p) != NULL) {
      *p = 0;
      p--;
    }
  }
  return a;
}

char * strip_lead_char (char *strip,char *a) {

  register char *p = a;

  if(a && *a && strip && *strip) {
    while(*p && strchr(strip,*p) != NULL)
      p++;
    if(p != a)
      memmove(a,p,strlen(p) + 1);
  }
  return a;
}


void WriteCenterString (char *line,USHORT y,USHORT x,int len) {

  int xlen = min(strlen(line),len);

  VioWrtCharStr(line,xlen,y,x + (len / 2) - (xlen / 2),0);
}


size_t CommaFormat(char   *buf,            /* Buffer for formatted string  */
                   int     bufsize,        /* Size of buffer               */
                   ULONG   N) {            /* Number to convert            */

  /* From C_ECHO via Bob Stout's Snippets */

  int   len = 1, posn = 1;
  char *ptr = buf + bufsize - 1;

  if(2 > bufsize) {
ABORT:
    *buf = 0;
    return 0;
  }

  *ptr-- = 0;
  --bufsize;

  for(;len <= bufsize;++len,++posn) {
    *ptr-- = (char)((N % 10L) + '0');
    if(0L == (N /= 10L))
      break;
    if(0 == (posn % 3)) {
      *ptr-- = ',';
      ++len;
    }
    if(len >= bufsize)
      goto ABORT;
  }

  strcpy(buf, ++ptr);
  return (size_t)len;
}


BOOL IsExecutable (char *filename) {

  /*
   * if filename can be executed, return TRUE
   * else return FALSE
   */

  register char *p;
  APIRET         rc;
  ULONG          apptype;

  if(filename) {
    DosError(FERR_DISABLEHARDERR);
    rc = DosQAppType(filename,&apptype);
    p = strrchr(filename,'.');
    if((!rc && (!apptype ||
                (apptype &
                 (FAPPTYP_NOTWINDOWCOMPAT |
                  FAPPTYP_WINDOWCOMPAT |
                  FAPPTYP_WINDOWAPI |
                  FAPPTYP_BOUND |
                  FAPPTYP_DOS |
                  FAPPTYP_WINDOWSREAL |
                  FAPPTYP_WINDOWSPROT |
                  FAPPTYP_32BIT |
                  0x1000)))) ||
       (p && (!stricmp(p,".CMD") ||
              !stricmp(p,".BAT"))))
      return TRUE;
  }
  return FALSE;
}


BOOL IsValidDrive (char drive) {

  /*
   * if drive is valid, return TRUE
   * else return FALSE
   */

  char   Path[] = " :",Buffer[256];
  APIRET Status;
  ULONG  Size;
  ULONG  ulDriveNum,ulDriveMap;

  if(!isalpha(drive))
    return FALSE;
  DosError(FERR_DISABLEHARDERR);
  Status = DosQCurDisk(&ulDriveNum,&ulDriveMap);
  if(!Status) {
    if(!(ulDriveMap & (1L << (ULONG)(toupper(drive) - 'A'))))
      return FALSE;
    Path[0] = toupper(drive);
    Size = sizeof(Buffer);
    DosError(FERR_DISABLEHARDERR);
    Status = DosQueryFSAttach(Path, 0, FSAIL_QUERYNAME,
                              (PFSQBUFFER2)Buffer, &Size);
  }
  return (Status == 0);
}


BOOL IsRoot (char *filename) {

  /*
   * if filename refers to a root directory, return TRUE
   * else return FALSE
   */

  return (filename && isalpha(*filename) && filename[1] == ':' &&
          filename[2] == '\\' && !filename[3]);
}


int IsFile (char *filename) {

  /*
   * returns:  -1 (error), 0 (is a directory), or 1 (is a file)
   */

  FILEFINDBUF3 fb3;
  HDIR         hdir = HDIR_CREATE;
  ULONG        nm = 1;
  APIRET       rc;

  if(filename) {
    DosError(FERR_DISABLEHARDERR);
    rc = DosFindFirst(filename,
                      &hdir,
                      FILE_NORMAL    | FILE_HIDDEN   | FILE_SYSTEM |
                      FILE_DIRECTORY | FILE_ARCHIVED | FILE_READONLY,
                      &fb3,
                      sizeof(fb3),
                      &nm,
                      FIL_STANDARD);
    if(!rc) {
      DosFindClose(hdir);
      return ((fb3.attrFile & FILE_DIRECTORY) == 0);
    }
    else {  /* special-case root drives -- FAT "feature" */
      if(IsRoot(filename) && IsValidDrive(*filename))
        return 0;
    }
  }
  return -1;  /* error; doesn't exist or null filename */
}


char *MakeFullName (char *s) {

  /*
   * warning!  not reentrant!
   */

  static char    t[CCHMAXPATH];
  register char *p;

  if(s) {
    if(IsRoot(s)) {
      strcpy(t,s);
      return t;
    }
    DosError(FERR_DISABLEHARDERR);
    if(!DosQueryPathInfo(s,FIL_QUERYFULLNAME,t,sizeof(t)) && *t) {
      p = t;
      while(*p) {
        if(*p == '/')
          *p = '\\';
        p++;
      }
      return t;
    }
    else {
      if(s[strlen(s) - 1] != '\\')
        strcat(s,"\\");
      DosError(FERR_DISABLEHARDERR);
      if(!DosQueryPathInfo(s,FIL_QUERYFULLNAME,t,sizeof(t))) {
        p = t;
        while(*p) {
          if(*p == '/')
            *p = '\\';
          p++;
        }
        return t;
      }
    }
  }
  return "";
}


APIRET MakeCurrDir (char *s) {

  /*
   * make directory named in s the current directory
   * including drive
   */

  APIRET      ret;
  FILESTATUS3 fsa;
  char        path[CCHMAXPATH + 1],*p;

  if(s) {
    strcpy(path,s);
    while(*path) {
      DosError(FERR_DISABLEHARDERR);
      ret = DosQueryPathInfo(path,FIL_STANDARD,&fsa,
                             (ULONG)sizeof(FILESTATUS3));
      if(!IsRoot(path) && (ret || !(fsa.attrFile & FILE_DIRECTORY))) {
        p = strrchr(path,'\\');
        if(p)
          *p = 0;
        else {
          strcpy(path,s);
          break;
        }
      }
      else
        break;
    }
    DosError(FERR_DISABLEHARDERR);
    if(isalpha(*path) && path[1] == ':') {
      ret = DosSelectDisk(toupper(*path) - '@');
      return (ret) ? ret : DosChDir(path);
    }
    return DosChDir(path);
  }
  else
    return ERROR_PATH_NOT_FOUND;
}


APIRET CurrDir (char *curdir) {

  /*
   * return current pathname in curdir
   */

  APIRET  ret;
  ULONG   curdirlen,curdrive,drivemap;

  *curdir = 0;
  DosError(FERR_DISABLEHARDERR);
  ret = DosQCurDisk (&curdrive, &drivemap);
  curdirlen = CCHMAXPATH - 4;   /* NOTE!!!!!!!!! */
  DosError(FERR_DISABLEHARDERR);
  ret += DosQCurDir (curdrive, &curdir[3], &curdirlen);
  *curdir = (char)('@' + (int)curdrive);
  curdir[1] = ':';
  curdir[2] = '\\';
  return ret;
}


ULONG FreeSpace (char drive) {

  /*
   * return bytes available on drive
   * (0 on failure)
   */

  FSALLOCATE fsa;

  memset(&fsa,0,sizeof(fsa));
  DosError(FERR_DISABLEHARDERR);
  if(!DosQueryFSInfo(toupper(drive) - '@',
                     FSIL_ALLOC,
                     &fsa,
                     sizeof(fsa))) {
    return fsa.cUnitAvail * (fsa.cSectorUnit * fsa.cbSector);
  }
  return 0;
}


char *stristr (register char *t, char *s) {

  /* case-insensitive strstr() */

  register char *t1,*s1;

  while (*t) {
    t1 = t;
    s1 = s;
    while (*s1) {
      if (toupper (*s1) != toupper (*t))
        break;
      else {
        s1++;
        t++;
      }
    }
    if (!*s1)
      return t1;
    t = t1 + 1;
  }
  return NULL;
}


BOOL MultiWildCard (char *fstra,char *fcarda) {

  register char *p,*pp;
  static char    tempcard[CCHMAXPATH];

  p = fcarda;
  while(p) {
    strcpy(tempcard,p);
    pp = strchr(tempcard,';');
    if(pp)
      *pp = 0;
    if(WildCard(fstra,tempcard,FALSE))
      return TRUE;
    p = strchr(p,';');
    if(p)
      p++;
  }
  return FALSE;
}


BOOL WildCard (char *fstra,char *fcarda,BOOL notfile) {

  register char *fstr = fstra,*fcard = fcarda;
  char          *p;
  register BOOL  wmatch = TRUE;

  while(wmatch && *fcard && *fstr) {
    switch(*fcard) {
      case '?' :                        /* character substitution */
         fcard++;
         if(notfile || (*fstr != '.' && *fstr != '/' && *fstr != '\\'))
           fstr++;                      /* skip (match) next character */
         break;

      case '*' :
         /* find next non-wild character in wildcard */
         while(*fcard && (*fcard == '?' || *fcard == '*'))
           fcard++;
         if(!*fcard)   /* if last char of wildcard is *, it matches */
           return TRUE;
          if((p = stristr(fstr,fcard)) != NULL) {
            fstr = p;
            break;
          }
         /* skip until partition, match, or eos */
         while(*fstr && *fstr != *fcard && (notfile || (*fstr != '\\' &&
               *fstr != '/' && *fstr != '.')))
           fstr++;
         if(!notfile && !*fstr)                   /* implicit '.' */
           if(*fcard == '.')
             fcard++;
         break;

      default  :
         if(!notfile && ((*fstr == '/' || *fstr == '\\') &&
            (*fcard == '/' || *fcard == '\\')))
           wmatch = TRUE;
         else
           wmatch = (toupper(*fstr) == toupper(*fcard));
         fstr++;
         fcard++;
         break;
    }
  }

  if ((*fcard && *fcard != '*') || *fstr)
    return FALSE;
  else
    return wmatch;
}


BOOL Associate (char *filename) {

  static char rest[1002];
  char  *p,*pipe,c = 0;
  ASSOC *info;
  BOOL   ret = FALSE;

  if(assoc) {
    p = strrchr(filename,'\\');
    if(p)
      p++;
    else
      p = filename;
    info = assoc;
    while(info) {
      if(WildCard(p,info->mask,FALSE)) {

        static char str[1022 + CCHMAXPATH];
        char       *q = "\"";

        if(*info->cmd == '*')
          p = info->cmd + 1;
        else
          p = info->cmd;
        if(!strchr(filename,' '))
          q = "";
        pipe = strchr(info->cmd,'|');
        if(!pipe || pipe > strchr(info->cmd,'<'))
          pipe = strchr(info->cmd,'<');
        if(!pipe || pipe > strchr(info->cmd,'>'))
          pipe = strchr(info->cmd,'>');
        if(pipe && pipe > info->cmd &&
           (*(pipe - 1) == '1' || *(pipe - 2) == '2') &&
           (*pipe == '>' || *pipe == '<'))
          pipe--;
        if(pipe) {
          strcpy(rest,pipe);
          c = *pipe;
          *pipe = 0;
        }
        else
          *rest = 0;
        sprintf(str,"%s%s%s%s%s",
                p,
                (info->cmd[strlen(info->cmd) - 1] != '=') ? " " : "",
                q,
                filename,
                q,
                (*rest) ? " " : "",
                rest);
        if(Shell(str,(*info->cmd == '*')) != -1)
          ret = TRUE;
        if(c)
          *pipe = c;
        break;
      }
      info = info->next;
    }
  }
  return ret;
}

