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

#pragma alloc_text(COMMON,ThreadMsg)
#pragma alloc_text(THREAD,StartThread)
#pragma alloc_text(THREAD1,Thread,FreeThreadArgs)
#pragma alloc_text(THREAD2,TruncName,GetLongName,WriteLongName)
#pragma alloc_text(THREAD2,MakeDeletable,EraseAll,PrintFile)

typedef struct THREADARGS {
  ULONG  cmd;
  ULONG  numfiles;
  BOOL   overwrite;
  char   target[CCHMAXPATH];
  char **filenames;
} THREADARGS;

static char *helpstr[] =
 {":1help :2rescan :3quit :4view :5edit :6copy :7move :8rename :9cmdline :1:0drive :1:1attr :1:2touch :/markall :\\rootdir",
  ":1swap :2savecfg :3invertsort :4viewer :5editor :6name :7size :8date :9os/2 :1:0info :1:1hidn :1:2dirstop :?unmarkall :|invert :>mark w/ mask :<unmark w/ mask",
  ":1set target dir=curr dir, :2:-:1:2save curr dir, :Delete :Editor :Gtreedir :Rescan :Treetarget :Zreset tree",
  ":1show saved dirs, :2:-:1:2recall saved dir, :Copy :Edit :Filter :GotoDir :Hide :Info :Move :NMkdir :Print :Rename :Subdirs :Target :View :Xexit :Zmultdrvs",
  ":= same as :+, :O:t:h:e:r:s search",
  ":A:-:Z finds root directory, :O:t:h:e:r:s search",
  ":Expand branch :ZRebuild Tree",
  ":Collapse all :Expand all :Rescan branch",
  ":C:t:r:l:+:P:a:g:e: :u:p:/:P:a:g:e: :d:o:w:n Previous/next file",
  ":C:t:r:l:+:P:a:g:e: :d:o:w:n Next file",
  "Browse the help file with :a:r:r:o:w: :a:n:d: :p:a:g:e: :k:e:y:s."};


void ThreadMsg (char *str) {

  int         xlen,attr = (7 << 8) | ' ';
  BOOL        primary = (ThreadID() == primarythread);
  static char shiftstatus = 0;
  KBDINFO     kbdinfo;
  char       *p,*e,cattr;
  
  if(shelled)
    return;

  DosEnterMustComplete(&mustcompletes);
  DosEnterCritSec();

   if(!primary || !threads) {
     xlen = (str) ? strlen(str) : 0;
     if(xlen) {
       /* erase message area */
       VioWrtNCell((char *)&attr,vio.col,vio.row - 1,0,0);
       /* display message */
       VioWrtCharStr(str,min(xlen,vio.col),vio.row - 1,0,0);
       if(xlen > vio.col)
         VioWrtCharStr("...",3,vio.row - 1,vio.col - 3,0);
     }
   }
   if(primary) {
     memset(&kbdinfo,0,sizeof(kbdinfo));
     kbdinfo.cb = sizeof(kbdinfo);
     if(!KbdGetStatus(&kbdinfo,0)) {
       shiftstatus = 0;
       if(kbdinfo.fsState & (KBDSTF_RIGHTSHIFT | KBDSTF_LEFTSHIFT))
        shiftstatus = 1;
       else if(kbdinfo.fsState & KBDSTF_CONTROL)
        shiftstatus = 2;
       else if(kbdinfo.fsState & KBDSTF_ALT)
        shiftstatus = 3;
     }
     if(treeing)
       shiftstatus += 4;
     if(helping)
       shiftstatus = 10;
     else if(viewonly)
       shiftstatus = 9;
     else if(viewing)
       shiftstatus = 8;
   }
   if(!xlen && (!primary || !threads)) {
     /* erase message area */
     VioWrtNCell((char *)&attr,vio.col,vio.row - 1,0,0);
     p = helpstr[shiftstatus];
     xlen = 0;
     cattr = ((8 << 4) | 4);
     while(*p && xlen < vio.col) {
       switch(*p) {
         case ':':
           p++;
           if(*p)
             VioWrtCharStrAtt(p,1,vio.row - 1,xlen++,&cattr,0);
           break;

         default:
           e = p;
           while(*e && *e != ':' && xlen + (e - p) < vio.col)
             e++;
           e--;
           VioWrtCharStr(p,((e - p) + 1),vio.row - 1,xlen,0);
           xlen += ((e - p) + 1);
           p = e;
           break;
       }
       if(*p)
         p++;
     }
   }

  DosExitCritSec();
  DosExitMustComplete(&mustcompletes);
}


BOOL MakeDeletable (char *s) {

  /*
   * remove attributes from a file system object
   * that would prevent us deleting it
   */

  FILESTATUS3 fs3;
  BOOL        isdir = FALSE;

  DosError(FERR_DISABLEHARDERR);
  if(!DosQueryPathInfo(s,
                       FIL_STANDARD,
                       &fs3,
                       sizeof(fs3))) {
    if((fs3.attrFile & FILE_DIRECTORY) != 0)
      isdir = TRUE;
    fs3.attrFile &= (~(FILE_DIRECTORY | FILE_HIDDEN |
                       FILE_SYSTEM | FILE_READONLY));
    DosError(FERR_DISABLEHARDERR);
    DosSetPathInfo(s,
                   FIL_STANDARD,
                   &fs3,
                   sizeof(fs3),
                   DSPI_WRTTHRU);
  }
  return isdir;
}


APIRET EraseAll (char *s) {

  FILEFINDBUF3 *fb3;
  ULONG         nm;
  HDIR          hdir;
  char         *str,*p;
  APIRET        rc = ERROR_NOT_ENOUGH_MEMORY;

  if(s && *s) {
    p = strrchr(s,'\\');
    if(p) {
      p++;
      fb3 = malloc(sizeof(FILEFINDBUF3));
      if(fb3) {
        str = malloc(CCHMAXPATH);
        if(str) {
          hdir = HDIR_CREATE;
          nm = 1;
          rc = 0;
          DosError(FERR_DISABLEHARDERR);
          if(!DosFindFirst(s,
                           &hdir,
                           FILE_NORMAL   | FILE_HIDDEN    | FILE_SYSTEM |
                           FILE_READONLY | FILE_DIRECTORY | FILE_ARCHIVED,
                           fb3,
                           sizeof(FILEFINDBUF3),
                           &nm,
                           FIL_STANDARD)) {
            *p = 0;
            if(!IsRoot(s)) {
              do {
                sprintf(str,"%s%s",s,fb3->achName);
                if(fb3->attrFile & FILE_DIRECTORY) {
                  if(strcmp(fb3->achName,".") && strcmp(fb3->achName,"..")) {
                    strcat(str,"\\*");
                    rc = EraseAll(str);
                    if(rc)
                      break;
                    str[strlen(str) - 1] = 0;
                    MakeDeletable(str);
                    DosError(FERR_DISABLEHARDERR);
                    rc = DosDeleteDir(str);
                    if(rc)
                      break;
                  }
                }
                else {
                  MakeDeletable(str);
                  DosError(FERR_DISABLEHARDERR);
                  rc = DosForceDelete(str);
                  if(rc)
                    break;
                }
                nm = 1;
              } while(!DosFindNext(hdir,fb3,sizeof(FILEFINDBUF3),&nm));
            }
            else
              rc = ERROR_BAD_PATHNAME;
            DosFindClose(hdir);
          }
          free(str);
        }
        free(fb3);
      }
    }
  }
  return rc;
}


BOOL GetLongName (char *oldname,char *longname) {

  BOOL ret = FALSE;

  if(!longname)
    return FALSE;
  *longname = 0;
  if(!oldname || !*oldname)
    return FALSE;
  {
    APIRET    rc;
    EAOP2     eaop;
    PGEA2LIST pgealist;
    PFEA2LIST pfealist;
    PGEA2     pgea;
    PFEA2     pfea;
    char      *value,*p;

    strcpy(longname,oldname);
    p = longname;
    while(*p) {
      if(*p == '/')
        *p = '\\';
      p++;
    }
    p = strrchr(longname,'\\');
    if(p)
      p++;
    else
      p = longname;
    pgealist = malloc(sizeof(GEA2LIST) + 128);
    if(pgealist) {
      memset(pgealist,0,sizeof(GEA2LIST) + 128);
      pgea = &pgealist->list[0];
      strcpy(pgea->szName,".LONGNAME");
      pgea->cbName = strlen(pgea->szName);
      pgea->oNextEntryOffset = 0L;
      pgealist->cbList = (sizeof(GEA2LIST) + pgea->cbName);
      pfealist = malloc(1536);
      if(pfealist) {
        memset(pfealist,0,1024);
        pfealist->cbList = 1024;
        eaop.fpGEA2List = pgealist;
        eaop.fpFEA2List = pfealist;
        eaop.oError = 0L;
        DosError(FERR_DISABLEHARDERR);
        rc = DosQueryPathInfo(oldname,FIL_QUERYEASFROMLIST,
                              (PVOID)&eaop,(ULONG)sizeof(EAOP2));
        if(!rc) {
          pfea = &eaop.fpFEA2List->list[0];
          value = pfea->szName + pfea->cbName + 1;
          value[pfea->cbValue] = 0;
          if(*(USHORT *)value == EAT_ASCII) {
            ret = TRUE;
            strncpy(p,value + (sizeof(USHORT) * 2),
                    CCHMAXPATH - strlen(longname));
            longname[CCHMAXPATH - 1] = 0;
          }
        }
        free(pfealist);
      }
      free(pgealist);
    }
  }
  return ret;
}


BOOL WriteLongName (char *filename,char *longname) {

  APIRET    rc;
  EAOP2     eaop;
  PFEA2LIST pfealist = NULL;
  ULONG     ealen;
  USHORT    len;
  char     *eaval,*p;

  if(!filename || !*filename || !longname)
    return FALSE;
  p = longname;
  while(*p) {
    if(*p == '/')
      *p = '\\';
    p++;
  }
  p = strrchr(longname,'\\');
  if(p)
    memmove(longname,p + 1,strlen(p + 1) + 1);
  lstrip(rstrip(longname));
  len = strlen(longname);
  if(len)
    ealen = sizeof(FEA2LIST) + 10 + len + 4;
  else
    ealen = sizeof(FEALIST) + 10;
  if(!DosAllocMem((PPVOID)&pfealist,ealen + 128,
                   OBJ_TILE | PAG_COMMIT | PAG_READ | PAG_WRITE)) {
    memset(pfealist,0,ealen + 1);
    pfealist->cbList = ealen;
    pfealist->list[0].oNextEntryOffset = 0L;
    pfealist->list[0].fEA = 0;
    pfealist->list[0].cbName = 9;
    strcpy(pfealist->list[0].szName,".LONGNAME");
    if(len) {
      eaval = pfealist->list[0].szName + 10;
      *(USHORT *)eaval = (USHORT)EAT_ASCII;
      eaval += sizeof(USHORT);
      *(USHORT *)eaval = (USHORT)len;
      eaval += sizeof(USHORT);
      memcpy(eaval,longname,len);
      pfealist->list[0].cbValue = len + (sizeof(USHORT) * 2);
    }
    else
      pfealist->list[0].cbValue = 0;
    eaop.fpGEA2List = (PGEA2LIST)0;
    eaop.fpFEA2List = pfealist;
    eaop.oError = 0L;
    DosError(FERR_DISABLEHARDERR);
    rc = DosSetPathInfo(filename,FIL_QUERYEASIZE,
                        (PVOID)&eaop,(ULONG)sizeof(EAOP2),
                        DSPI_WRTTHRU);
    DosFreeMem(pfealist);
    if(rc)
      return FALSE;
  }
  return TRUE;
}


char *TruncName (char *oldname,char *buffer) {

  char       *p,*f,*s,*o,*cmp,*cmpl,longname[CCHMAXPATH];
  FILESTATUS3 fs3;
  APIRET      rc;

  if(!buffer || !oldname || !*oldname) {
    if(buffer)
      *buffer = 0;
    return NULL;
  }
  strcpy(buffer,oldname);
  f = strrchr(buffer,'\\');
  if(!f)
    f = strrchr(buffer,'/');
  if(!f)
    f = buffer;
  else
    f++;
  cmp = oldname + (f - buffer); /* for longname comparisons */
  p = f;
  o = p;
  f = oldname + (f - buffer);
  strupr(buffer);
  while(*f == '.')  /* skip leading '.'s */
    f++;
  s = f;
  while(*f && *f != '.' && f < s + 8) { /* skip past rootname */
    *p = toupper(*f);
    p++;
    f++;
  }
  while(*f == '.')
    f++;
  s = f;
  f = strrchr(f,'.');
  if(f) {
    while(*f == '.')
      f++;
  }
  if(f && *(f + 1))
    s = f;
  else
    f = s;
  if(*f) {
    *p = '.';
    p++;
    while(*f && *f != '.' && f < s + 3) {
      *p = toupper(*f);
      p++;
      f++;
    }
  }
  *p = 0;

  p = o;
  while(*p) {
    if(strchr("*?<>\":/\\|+=;,[] ",*p) || *p < 0x20)
      *p = '_';
    if(*p == '.' && *(p + 1) == '.')
      *(p + 1) = '_';
    p++;
  }

  p = o + (strlen(o) - 1);
  for(;;) {
    DosError(FERR_DISABLEHARDERR);
    rc = DosQueryPathInfo(buffer,FIL_STANDARD,&fs3,
                        (ULONG)sizeof(fs3));
    if(rc == ERROR_DISK_CHANGE) {
      DosError(FERR_ENABLEHARDERR);
      rc = DosQueryPathInfo(buffer,FIL_STANDARD,&fs3,
                          (ULONG)sizeof(fs3));
    }
    if(rc)
      break;
    else {
      if(GetLongName(buffer,longname)) {  /* same longname? */
        cmpl = strrchr(longname,'\\');
        if(!cmpl)
          cmpl = strrchr(longname,'/');
        if(!cmpl)
          cmpl = longname;
        else
          cmpl++;
        if(!stricmp(cmp,cmpl))
          break;
      }
    }
Loop:
    if(p < o)
      return NULL;
    if((*p) + 1 < 'Z' + 1) {
      (*p)++;
      while(strchr("*?<>\":/\\|+=;,[]. ",*p))
        (*p)++;
      *p = toupper(*p);
    }
    else {
      p--;
      if(p >= o && *p == '.')
        p--;
      goto Loop;
    }
  }
  return buffer;
}


void FreeThreadArgs (THREADARGS *args) {

  ULONG x;

  if(args) {
    if(args->numfiles && args->filenames) {
      for(x = 0;x < args->numfiles;x++) {
        if(args->filenames[x])
          free(args->filenames[x]);
      }
      free(args->filenames);
    }
    free(args);
  }
}


void Thread (void *vargs) {

  THREADARGS *args = vargs;
  ULONG       x;
  char        str[(CCHMAXPATH * 2) + 80],*p,longname[CCHMAXPATH];
  APIRET      rc;
  BOOL        madeshort,hadlong,writelong;

  DosEnterMustComplete(&mustcompletes);
  DosEnterCritSec();
   threads++;
  DosExitCritSec();
  DosExitMustComplete(&mustcompletes);

  if(idlethreads)
    DosSetPriority(PRTYS_THREAD,PRTYC_IDLETIME,31L,0L);

  if(args) {
    if(args->numfiles && args->filenames) {
      for(x = 0;x < args->numfiles;x++) {
        madeshort = hadlong = writelong = FALSE;
        switch(args->cmd) {
          case C_TOUCH:
            sprintf(str,"Touching \"%s\"",args->filenames[x]);
            ThreadMsg(str);
            { /* set file time/date to current time/date */
              FILESTATUS3 fs3;
              time_t      t;
              struct tm  *tm;

              DosError(FERR_DISABLEHARDERR);
              rc = DosQueryPathInfo(args->filenames[x],
                                    FIL_STANDARD,
                                    &fs3,
                                    sizeof(fs3));
              if(!rc) {
                fs3.attrFile &= (~(FILE_DIRECTORY));
                t = time(NULL);
                tm = localtime(&t);
                fs3.fdateLastWrite.year     = (tm->tm_year + 1900) - 1980;
                fs3.fdateLastWrite.month    = tm->tm_mon + 1;
                fs3.fdateLastWrite.day      = tm->tm_mday;
                fs3.ftimeLastWrite.hours    = tm->tm_hour;
                fs3.ftimeLastWrite.minutes  = tm->tm_min;
                fs3.ftimeLastWrite.twosecs  = tm->tm_sec / 2;
                DosError(FERR_DISABLEHARDERR);
                rc = DosSetPathInfo(args->filenames[x],
                                    FIL_STANDARD,
                                    &fs3,
                                    sizeof(fs3),
                                    DSPI_WRTTHRU);
              }
              if(rc) {
                sprintf(str,"SYS%04lu:  Failed to touch \"%s\"",
                        rc,args->filenames[x]);
                ThreadMsg(str);
                DosBeep(50,25);
                DosSleep(2000L);
              }
            }
            break;

          case C_COPY:
            sprintf(str,"Copying \"%s\" -> \"%s\"",
                    args->filenames[x],args->target);
            ThreadMsg(str);
            if(toupper(*args->target) != toupper(*args->filenames[x])) {
              if(GetLongName(args->filenames[x],longname))
                hadlong = TRUE;
            }
            else
              strcpy(longname,args->filenames[x]);
            p = strrchr(longname,'\\');
            if(!p)
              p = longname;
            else
              p++;
            sprintf(str,"%s%s%s",
                    args->target,
                    (args->target[strlen(args->target) - 1] != '\\') ?
                      "\\" : "",
                    p);
ReCopy:
            DosError(FERR_DISABLEHARDERR);
            rc = DosCopy(args->filenames[x],str,
                         ((args->overwrite) ? DCPY_EXISTING : 0));
            if(rc == ERROR_DISK_CHANGE) {
              DosError(FERR_ENABLEHARDERR);
              rc = DosCopy(args->filenames[x],str,
                           ((args->overwrite) ? DCPY_EXISTING : 0));
            }
            else if(rc && !madeshort) {

              char filename[CCHMAXPATH];

              if(hadlong) {   /* Try current filename */
                sprintf(str,"%s%s%s",
                        args->target,
                        (args->target[strlen(args->target) - 1] != '\\') ?
                          "\\" : "",
                        p);
                hadlong = FALSE;
                writelong = TRUE;
                goto ReCopy;
              }
              madeshort = TRUE;
              TruncName(str,filename);
              strcpy(str,filename);
              writelong = TRUE;
              goto ReCopy;
            }
            if(rc) {
              sprintf(str,"SYS%04lu: Failed to copy \"%s\"",
                      rc,args->filenames[x]);
              ThreadMsg(str);
              DosBeep(50,25);
              DosSleep(2000L);
            }
            else {
              if(writelong)
                WriteLongName(str,p);
              else
                if(hadlong)
                  WriteLongName(str,"");  /* erase longname */
            }
            break;

          case C_MOVE:
            sprintf(str,"Moving \"%s\" -> \"%s\"",
                    args->filenames[x],args->target);
            ThreadMsg(str);
            if(toupper(*args->target) != toupper(*args->filenames[x])) {
              if(GetLongName(args->filenames[x],longname))
                hadlong = TRUE;
            }
            else
              strcpy(longname,args->filenames[x]);
            p = strrchr(longname,'\\');
            if(!p)
              p = longname;
            else
              p++;
            sprintf(str,"%s%s%s",
                    args->target,
                    (args->target[strlen(args->target) - 1] != '\\') ?
                      "\\" : "",
                    p);
            if(toupper(*str) == toupper(*args->filenames[x]) &&
               IsFile(str) == -1) {
              DosError(FERR_DISABLEHARDERR);
              rc = DosMove(args->filenames[x],str);
              if(rc) {
                DosError(FERR_DISABLEHARDERR);
                rc = DosCopy(args->filenames[x],str,
                             ((args->overwrite) ? DCPY_EXISTING : 0));
              }
            }
            else {
ReMove:
              DosError(FERR_DISABLEHARDERR);
              rc = DosCopy(args->filenames[x],str,
                           ((args->overwrite) ? DCPY_EXISTING : 0));
              if(rc == ERROR_DISK_CHANGE) {
                DosError(FERR_ENABLEHARDERR);
                rc = DosCopy(args->filenames[x],str,
                             ((args->overwrite) ? DCPY_EXISTING : 0));
              }
              else if(rc && !madeshort) {

                char filename[CCHMAXPATH];

                if(hadlong) {   /* Try current filename */
                  sprintf(str,"%s%s%s",
                          args->target,
                          (args->target[strlen(args->target) - 1] != '\\') ?
                            "\\" : "",
                          p);
                  hadlong = FALSE;
                  writelong = TRUE;
                  goto ReMove;
                }
                madeshort = TRUE;
                TruncName(str,filename);
                strcpy(str,filename);
                writelong = TRUE;
                goto ReMove;
              }
            }
            if(rc || IsFile(str) == -1) {
              sprintf(str,"SYS%04lu: Failed to %smove \"%s\"",
                      rc,
                      (IsFile(args->filenames[x]) == 0) ? "(fully) " : "",
                      args->filenames[x]);
              ThreadMsg(str);
              DosBeep(50,25);
              DosSleep(2000L);
            }
            else {
              if(writelong)
                WriteLongName(str,p);
              else if(hadlong)
                WriteLongName(str,"");  /* erase longname */
              if(IsFile(args->filenames[x]) != -1)
                goto DeleteAfter;
            }
            break;

          case C_DELETEPERM:
          case C_DELETE:
            sprintf(str,"Deleting \"%s\"",args->filenames[x]);
            ThreadMsg(str);
DeleteAfter:
            {
              BOOL isdir = MakeDeletable(args->filenames[x]);

              DosError(FERR_DISABLEHARDERR);
              if(isdir) {
                sprintf(str,"%s%s*",args->filenames[x],
                        (args->filenames[x][strlen(args->filenames[x]) - 1] != '\\') ? "\\" : "");
                rc = EraseAll(str);
                if(!rc)
                  rc = DosDeleteDir(args->filenames[x]);
              }
              else {
                if(args->cmd == C_DELETEPERM)
                  rc = DosForceDelete(args->filenames[x]);
                else
                  rc = DosDelete(args->filenames[x]);
              }
              if(rc) {
                sprintf(str,"SYS%04lu: Failed to delete %s\"%s\"",
                        rc,
                        (args->cmd == C_MOVE) ? "original " : "",
                        args->filenames[x]);
                ThreadMsg(str);
                DosBeep(50,25);
                DosSleep(2000L);
              }
            }
            break;

          case C_PRINT:
            sprintf(str,"Printing \"%s\"",args->filenames[x]);
            ThreadMsg(str);
            {
              FILE *fpi,*fpo;
              char  strp[16384];

              fpi = fopen(args->filenames[x],"r");
              if(fpi) {
                fpo = fopen("PRN","w");
                if(fpo) {
                  while(!feof(fpi)) {
                    if(!fgets(strp,16384,fpi))
                      break;
                    strp[16383] = 0;
                    fprintf(fpo,"%s",strp);
                  }
                  fflush(fpo);
                  fclose(fpo);
                }
                fclose(fpi);
              }
            }
            break;
        }
      }
      ThreadMsg(NULL);
    }
    FreeThreadArgs(args);
  }

  if(idlethreads)
    DosSetPriority(PRTYS_THREAD,PRTYC_REGULAR,0L,0L);

  DosEnterMustComplete(&mustcompletes);
  DosEnterCritSec();
   threads--;
  DosExitCritSec();
  DosExitMustComplete(&mustcompletes);
}


BOOL StartThread (ULONG cmd) {

  THREADARGS *args;
  int         key,okey;
  ULONG       x;
  char      **test,filename[CCHMAXPATH],*p;
  ULONG       existed = 0,somedirs = 0;
  BOOL        notexisted = FALSE,ret = FALSE,marked = TRUE;


  if(numfiles && files) {
    if((cmd == C_MOVE || cmd == C_COPY) &&
       !stricmp(target,directory) &&
       !strchr(files[0].filename,'\\')) {
      SimpleInput("Yo!",
                  "Target == Source.  Select a new target directory (Alt+T).",
                  100,50,2000,NULL);
      return ret;
    }
    args = malloc(sizeof(THREADARGS));
    if(args) {
      memset(args,0,sizeof(THREADARGS));
      args->cmd = cmd;
      strcpy(args->target,target);
      for(x = 0;x < numfiles;x++) {
        if((files[x].flags & F_MARKED) != 0 &&
           (key = IsFile(files[x].filename)) != -1 &&
           !DosQueryPathInfo(files[x].filename,FIL_QUERYFULLNAME,
                             filename,sizeof(filename))) {
          test = realloc(args->filenames,
                         (args->numfiles + 1) * sizeof(char *));
          if(test) {
            args->filenames = test;
            args->filenames[args->numfiles] = strdup(filename);
            if(args->filenames[args->numfiles]) {
              if(!key)
                somedirs++;
              p = strrchr(args->filenames[args->numfiles],'\\');
              if(!p)
                p = args->filenames[args->numfiles];
              else
                p++;
              sprintf(filename,"%s%s%s",
                      target,
                      (target[strlen(target) - 1] != '\\') ? "\\" : "",
                      p);
              if(IsFile(filename) != -1)
                existed++;
              else
                notexisted = TRUE;
              args->numfiles++;
            }
            else {  /* out of memory */
              args->filenames = realloc(args->filenames,
                                        args->numfiles * sizeof(char *));
              DosBeep(250,100);
              break;
            }
          }
          else {    /* out of memory */
            DosBeep(250,100);
            break;
          }
        }
      }
      if(!args->numfiles) { /* no marked files -- use current file */
        marked = FALSE;
        key = IsFile(files[file].filename);
        if(key != -1 && !DosQueryPathInfo(files[file].filename,
                                          FIL_QUERYFULLNAME,
                                          filename,
                                          sizeof(filename))) {
          test = realloc(args->filenames,
                         (args->numfiles + 1) * sizeof(char *));
          if(test) {
            args->filenames = test;
            args->filenames[args->numfiles] = strdup(filename);
            if(args->filenames[args->numfiles]) {
              if(!key)
                somedirs++;
              p = strrchr(args->filenames[args->numfiles],'\\');
              if(!p)
                p = args->filenames[args->numfiles];
              else
                p++;
              sprintf(filename,"%s%s%s",
                      target,
                      (target[strlen(target) - 1] != '\\') ? "\\" : "",
                      p);
              if(IsFile(filename) != -1)
                existed++;
              else
                notexisted = TRUE;
              args->numfiles++;
            }
            else {  /* out of memory */
              args->filenames = realloc(args->filenames,
                                        args->numfiles * sizeof(char *));
              DosBeep(250,100);
            }
          }
          else      /* out of memory */
            DosBeep(250,100);
        }
      }
      if(args->numfiles && args->filenames) {

        char str[80];
        int  responses[] = {'\r','y','Y','n','N','\x1b',45 | 256,61 | 256,0};

        sprintf(str,"%s %lu %s file object%s?",
                ((cmd == C_COPY)       ? "Copy" :
                 (cmd == C_MOVE)       ? "Move" :
                 (cmd == C_DELETE)     ? "Delete" :
                 (cmd == C_DELETEPERM) ? "Permanently delete" :
                 (cmd == C_TOUCH)      ? "Touch" :
                 (cmd == C_PRINT)      ? "Print" : ""),
                 args->numfiles,
                 (marked) ? "marked" : "current",
                 &"s"[args->numfiles == 1]);
        if(cmd == C_DELETE && somedirs)
          sprintf(str + strlen(str),"  (%lu director%s present!)",
                  somedirs,
                  (somedirs > 1) ? "ies" : "y");
        key = SimpleInput("Confirm",str,0,0,0,responses);
        if(key == '\r' || key == 'y')
          key = 'Y';
        if(key == 'Y' && (cmd == C_MOVE || cmd == C_COPY)) {
          strcpy(str,"Overwrite existing file objects");
          if(existed)
            sprintf(str + strlen(str)," (%lu known to exist)?",
                    existed);
          else
            strcat(str," (none known)?");
          okey = SimpleInput("Just one more confirmation",str,
                             0,0,0,responses);
          if(okey == '\r' || okey == 'y')
            okey = 'Y';
          if(okey == (45 | 256) || okey == (61 | 256)) {
            key = 'N';
            okey = 'N';
          }
          if(okey == 'Y')
            args->overwrite = TRUE;
          else if(!notexisted)
            key = 'N';
        }
        if(key != 'Y' || _beginthread(Thread,NULL,65536,args) == -1) {
Abort:
          if(key == 'Y')
            DosBeep(50,100);
          FreeThreadArgs(args);
        }
        else
          ret = TRUE;
      }
    }
  }
  return ret;
}


void PrintFile (void *args) {

  char *filename = args;
  FILE *fpi,*fpo;
  char  str[16384];


  DosEnterMustComplete(&mustcompletes);
  DosEnterCritSec();
   threads++;
  DosExitCritSec();
  DosExitMustComplete(&mustcompletes);

  sprintf(str,"Printing \"%s\"",filename);
  ThreadMsg(str);

  if(filename) {
    fpi = fopen(filename,"r");
    if(fpi) {
      fpo = fopen("PRN","w");
      if(fpo) {
        while(!feof(fpi)) {
          if(!fgets(str,16384,fpi))
            break;
          str[16383] = 0;
          fprintf(fpo,"%s",str);
        }
        fflush(fpo);
        fclose(fpo);
      }
      fclose(fpi);
    }
    free(filename);
  }

  ThreadMsg(NULL);

  DosEnterMustComplete(&mustcompletes);
  DosEnterCritSec();
   threads--;
  DosExitCritSec();
  DosExitMustComplete(&mustcompletes);
}

