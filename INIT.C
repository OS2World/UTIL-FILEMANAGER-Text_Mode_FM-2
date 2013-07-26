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

#pragma alloc_text(INIT,Init,DeInit,WriteFMTCFG)

extern void ThreadMsg (char *str);


void Init (int argc,char **argv) {

  FILE       *fp;
  static char str[1006],*p,*pp;
  int         attr = (7 << 8) | ' ',x,c;
  ASSOC      *info,*last = NULL;

  sayanykey = "   <* Press any key to continue *>";
  primarythread = ThreadID();
  infolevel = 7;    /* all information on by default */
  dirstop = TRUE;   /* directories shown on top of list by default */
  findattr = FILE_NORMAL   | FILE_HIDDEN | FILE_SYSTEM   |
             FILE_ARCHIVED | FILE_SYSTEM | FILE_READONLY |
             FILE_DIRECTORY;
  strcpy(filter,"*");

  kbdorg.cb = sizeof(kbdorg);
  KbdGetStatus(&kbdorg,0);
  vioint.cb = sizeof(vioint);
  vioint.type = 2;
  VioGetState(&vioint,0);
  vio.cb = sizeof(vio);
  VioGetMode(&vio,0);
  VioGetCurPos(&ry,&rx,0);
  restorescreen = SaveScreen(restorescreen);
  SetupConsole();
  VioScrollUp(0,0,-1,-1,-1,(char *)&attr,0);

  /* set homedir to point to the directory in which the .EXE resides */
  {
    TIB  *pTib;
    PIB  *pPib;

    if(!DosGetInfoBlocks(&pTib,&pPib)) {
      if(!DosQueryModuleName(pPib->pib_hmte,sizeof(str),str)) {
        p = strrchr(str,'\\');
        if(p) {
          if(p == str + 2)
            p++;
          *p = 0;
          strcpy(homedir,str);
        }
      }
    }
  }

  CurrDir(directory);         /* what is current directory? */
  strcpy(target,directory);   /* initially set target == current */
  if(!*homedir)               /* in case the above failed... */
    strcpy(homedir,directory);

  sprintf(str,"%s%sFMT.cfg",homedir,
          (homedir[strlen(homedir) - 1] != '\\') ? "\\" : "");
  fp = fopen(str,"r");
  if(fp) {
    c = x = 0;
    while(!feof(fp)) {
      if(!fgets(str,1002,fp))
        break;
      str[1001] = 0;
      lstrip(rstrip(stripcr(str)));
      if(*str && *str != ';') {
        if(!editor && !strnicmp(str,"EDITOR ",7))
          editor = strdup(lstrip(str + 7));
        else if(!viewer && !strnicmp(str,"VIEWER ",7))
          viewer = strdup(lstrip(str + 7));
        else if(!strnicmp(str,"TARGET ",7))
          strcpy(target,MakeFullName(lstrip(str + 7)));
        else if(!stricmp(str,"AUTOSAVECFG"))
          autosavecfg = TRUE;
        else if(!stricmp(str,"RECURSE"))
          recurse = TRUE;
        else if(!strnicmp(str,"FILTER ",7))
          strcpy(filter,lstrip(str + 7));
        else if(!strnicmp(str,"RETAINTREE",10))
          retaintree = TRUE;
        else if(!stricmp(str,"INVERTSORT"))
          invertsort = TRUE;
        else if(!stricmp(str,"DIRSBOTTOM"))
          dirstop = FALSE;
        else if(!stricmp(str,"IDLETHREADS"))
          idlethreads = TRUE;
        else if(!stricmp(str,"INCLUDEDIRSINALLLISTS"))
          includedirsinalllists = TRUE;
        else if(!stricmp(str,"NOHIDDEN"))
          findattr &= (~(FILE_HIDDEN | FILE_SYSTEM));
        else if(!strnicmp(str,"SORTTYPE ",9)) {
          sorttype = atol(lstrip(str + 9));
          if(sorttype > S_FULLNAME)
            sorttype = S_NAME;
        }
        else if(!strnicmp(str,"INFOLEVEL ",10)) {
          infolevel = atol(lstrip(str + 10));
          if(infolevel > 7)
            infolevel = 7;
        }
        else if(x < 11 && !strnicmp(str,"DIRECTORY ",10)) {
          savedirs[x] = strdup(MakeFullName(lstrip(str + 10)));
          if(savedirs[x])
            x++;
        }
        else if(c < 13 && !strnicmp(str,"COMMAND ",8)) {
          usercmds[c] = strdup(lstrip(str + 8));
          if(usercmds[c])
            c++;
        }
        else if(!strnicmp(str,"ASSOC ",6)) {
          p = lstrip(str + 6);
          pp = p;
          while(*pp && *pp != ' ')
            pp++;
          if(*pp == ' ') {
            *pp = 0;
            pp++;
            while(*pp && *pp == ' ')
              pp++;
            if(*pp) {
              info = malloc(sizeof(ASSOC));
              if(info) {
                info->next = NULL;
                info->mask = strdup(p);
                if(info->mask) {
                  info->cmd = strdup(pp);
                  if(info->cmd) {
                    if(!assoc)
                      assoc = info;
                    else
                      last->next = info;
                    last = info;
                  }
                  else {
                    free(info->mask);
                    free(info);
                  }
                }
                else
                  free(info);
              }
            }
          }
        }
        else
          printf("\nUnknown FMT.CFG command \"%s\".\n",str);
      }
    }
    fclose(fp);
  }
  if(!editor)
    editor = strdup("T2.EXE");

  for(x = 1;x < argc;x++) {
    switch(*argv[x]) {
      case '/': /* argument switch */
      case '-':
        switch(toupper(argv[x][1])) {
          case '-':
            memmove(argv[x],&argv[x][1],strlen(&argv[x][1]));
            goto InterruptusD;

          case 'A':
            if(!viewonly)
              autosavecfg = (autosavecfg) ? FALSE : TRUE;
            break;

          case 'T':
            strcpy(target,MakeFullName(&argv[x][2]));
            break;

          case 'V':
            viewonly = TRUE;
            autosavecfg = FALSE;
            break;

          case '$':
            if(!viewonly)
              cdonexit = TRUE;
            break;

          case 'R':
            if(!viewonly)
              startintree = TRUE;
            break;

          case '?':
            goto InterruptusH;

          default:
            printf("Unknown command line switch \"%s\"\n",
                   argv[x]);
            break;
        }
        break;

      case '?':
InterruptusH:
        VioSetCurPos(0,0,0);
        printf("\nFMT is free software from Mark Kimes.\n"
               "\nFMT is FM/2 Tiny Text, a tiny text-mode file manager intended for use"
               "\nin situations where you must boot from diskettes.\n"
               "\nUsage:  FMT [/switches] [startupdir]\n"
               "\nSwitches:"
               "\n /A            = Autosave FMT.CFG at exit toggle (default off)"
               "\n /R            = Start in tree mode"
               "\n /T<targetdir> = Set target directory"
               "\n /V            = Viewer mode"
               "\n /$            = Write $FMTCD.CMD on exit\n"
               "\nExamples:"
               "\n FMT /A /TC:\\MYFILES\n FMT C:\\OS2\n"
               "\nCompiled:  %s  %s\n",__DATE__,__TIME__);
        exit(0);
        break;

      default:  /* default directory */
InterruptusD:
        strcpy(directory,MakeFullName(argv[x]));
        break;
    }
  }

  if(viewonly) {
    if(!IsFile(directory)) {
      if(directory[strlen(directory) - 1] != '\\')
        strcat(directory,"\\");
      strcat(directory,"*");
    }
  }

  if(strchr(directory,'*') || strchr(directory,'?')) {

    /* resolve any wildcards */
    FILEFINDBUF3 fb3;
    ULONG        nm = 1,attrib = findattr;
    HDIR         hdir = HDIR_CREATE;
    APIRET       rc;

    if(viewonly) {
      attrib &= (~FILE_DIRECTORY);
      strcpy(lastdir,directory);
    }
    p = strrchr(directory,'\\');
    if(p)
      p++;
    else
      p = directory;
    DosError(FERR_DISABLEHARDERR);
    rc = DosFindFirst(directory,&hdir,attrib,&fb3,sizeof(fb3),&nm,
                      FIL_STANDARD);
    if(!rc) {
      while(!rc) {
        if(viewonly && ((fb3.attrFile & FILE_DIRECTORY) == 0)) {
          strcpy(p,fb3.achName);
          break;
        }
        else if(!viewonly && ((fb3.attrFile & FILE_DIRECTORY) != 0) &&
                (*fb3.achName != '.' ||
                 ((fb3.achName[1] && fb3.achName[1] != '.') ||
                  (fb3.achName[2])))) {
          strcpy(p,fb3.achName);
          break;
        }
        nm = 1;
        rc = DosFindNext(hdir,&fb3,sizeof(fb3),&nm);
      }
      DosFindClose(hdir);
    }
  }

  if(viewonly) {
    ViewFile(directory);
    DeInit();
    exit(0);
  }
  else {
    ThreadMsg(NULL);

    MakeCurrDir(directory);
    CurrDir(directory);         /* what is current directory? */

    if(startintree) {

      char *dir;

      dir = TreeView(NULL);
      if(dir && IsFile(dir) == 0) {
        MakeCurrDir(dir);
        CurrDir(directory);
        if(cdonexit) {
          DeInit();
          exit(0);
        }
      }
    }

    AdjustLastDir();
    DisplayTarget();
    freespace = FreeSpace(*target);
    DisplayFreeSpace(*target);

    BuildList("",TRUE,FALSE);
    DisplayNumSizeFiles();
    if(numfiles)
      DisplayCurrFile(&files[file]);
    DisplayFiles();
  }
}


void DeInit (void) {

  RestoreScreen(restorescreen);
  free(restorescreen);
  VioSetState(&vioint,0); /* put states back */
  KbdSetStatus(&kbdorg,0);
  VioSetCurPos(ry,rx,0);
  ShowCursor(FALSE);
  if(autosavecfg)
    WriteFMTCFG();
  if(cdonexit) {

    FILE *fp;
    char  str[CCHMAXPATH];

    sprintf(str,"%s%s$FMTCD.CMD",homedir,
            (homedir[strlen(homedir) - 1] != '\\') ? "\\" : "");
    fp = fopen(str,"w");
    if(fp) {
      fprintf(fp,"@echo off\n%c:\ncd %s\n",*directory,directory);
      fclose(fp);
    }
  }
}


void WriteFMTCFG (void) {

  FILE       *fp;
  int         x;
  BOOL        once = FALSE;
  static char str[CCHMAXPATH];
  ASSOC      *info;

  sprintf(str,"%s%sFMT.cfg",homedir,
          (homedir[strlen(homedir) - 1] != '\\') ? "\\" : "");
  fp = fopen(str,"w");
  if(fp) {
    fprintf(fp,"; FMT.CFG, FMT configuration file\n"
               "; FMT is free software.\n;\n");
    fprintf(fp,"; Lines should not exceed 1000 bytes in length, "
               "including comments.\n");
    fprintf(fp,"; Any line beginning with a semi-colon is a comment.\n;\n");
    fprintf(fp,"%sEDITOR %s\n",
            (editor) ? "" : "; ",
            (editor) ? editor : "T2.EXE");
    fprintf(fp,"%sVIEWER %s\n;\n",
            (viewer) ? "" : "; ",
            (viewer) ? viewer : "HV.EXE");
    fprintf(fp,"TARGET %s\n;\n",target);
    fprintf(fp,"%sFILTER %s\n",
            (*filter) ? "" : "; ",
            (*filter) ? filter : "*");
    fprintf(fp,"%sNOHIDDEN\n;\n",
            ((findattr & FILE_HIDDEN) != 0) ? "; " : "");
    fprintf(fp,"%sRECURSE\n;\n",
            (recurse) ? "" : "; ");
    fprintf(fp,"%sAUTOSAVECFG\n;\n",
            (autosavecfg) ? "" : "; ");
    fprintf(fp,"; Info levels (bitmapped field):\n"
               ";  0 = name only\n"
               ";  1 = name + date\n"
               ";  2 = name + size\n"
               ";  3 = name + date + size\n"
               ";  4 = name + attr\n"
               ";  5 = name + attr + date\n"
               ";  6 = name + attr + size\n"
               ";  7 = name + attr + date + size\n");
    fprintf(fp,"INFOLEVEL %lu\n;\n",
            infolevel);
    fprintf(fp,"; Sort types:\n"
               ";  0 = name\n"
               ";  1 = size\n"
               ";  2 = date\n"
               ";  3 = full name\n");
    fprintf(fp,"SORTTYPE %lu\n",
            sorttype);
    fprintf(fp,"%sINVERTSORT\n",
            (invertsort) ? "" : "; ");
    fprintf(fp,"%sDIRSBOTTOM\n;\n",
            (dirstop) ? "; " : "");
    fprintf(fp,"; Saved directories (up to 11 used, one entry per line):\n;\n");
    for(x = 0;x < 11;x++) {
      if(savedirs[x]) {
        once = TRUE;
        fprintf(fp,"DIRECTORY %s\n",savedirs[x]);
      }
    }
    if(!once)
      fprintf(fp,"; DIRECTORY C:\\OS2\n");
    fprintf(fp,";\n;\n; Goodies not documented elsewhere!\n;\n;\n");
    fprintf(fp,"%sIDLETHREADS\n;\n",
            (idlethreads) ? "" : "; ");
    fprintf(fp,"%sINCLUDEDIRSINALLLISTS\n;\n",
            (includedirsinalllists) ? "" : "; ");
    fprintf(fp,";\n; Use Ctrl+Z to reset tree if using the following command:\n"
               "%sRETAINTREE\n;\n",
               (retaintree) ? "" : "; ");
    once = FALSE;
    fprintf(fp,";\n; User commands (up to 13 used, one entry per line, Alt+` - Alt+=):\n;\n");
    for(x = 0;x < 13;x++) {
      if(usercmds[x]) {
        once = TRUE;
        fprintf(fp,"COMMAND %s\n",usercmds[x]);
      }
    }
    if(!once) {
      fprintf(fp,"; COMMAND WIPE.EXE\n");
      fprintf(fp,"; COMMAND ZIP.EXE -9\n");
      fprintf(fp,"; COMMAND UNZIP.EXE -t\n");
    }
    once = FALSE;
    fprintf(fp,";\n;\n; Associations (one entry per line, mask<space>command, as many as req'd)"
               "\n;  Place an * at the front of command to pause after execution:\n;\n");
    info = assoc;
    while(info) {
      fprintf(fp,"ASSOC %s %s\n",info->mask,info->cmd);
      once = TRUE;
      info = info->next;
    }
    if(!once) {
      fprintf(fp,"; ASSOC *.WAV PLAY FILE=\n");
      fprintf(fp,"; ASSOC *.ZIP *UNZIP -v\n");
      fprintf(fp,"; ASSOC *.LZH *LH l\n");
    }
    fclose(fp);
  }
}

