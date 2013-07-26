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
#include "fmt.h"

#pragma alloc_text(TREE,FreeTree,StubTree,BuildTree,CreateRoot,FullTreeName)
#pragma alloc_text(TREE,TreeName,CountTree,BuildTreeArray,ShowTreeItem)
#pragma alloc_text(TREE,ShowTree,TreeView)

extern void ThreadMsg (char *str);


void FreeTree (TREE *head) {

  /* recursively free a tree */

  TREE *info,*next;

  info = head;
  while(info) {
    next = info->sibling;
    if(info->name)
      free(info->name);
    if(info->child)
      FreeTree(info->child);
    free(info);
    info = next;
  }
}


APIRET StubTree (TREE *info,char *dir) {

  FILEFINDBUF3 *fb3,*pffb;
  BYTE         *pb;
  HDIR          hdir = HDIR_CREATE;
  ULONG         nm,x,maxnum;
  APIRET        rc = ERROR_NOT_ENOUGH_MEMORY;

  maxnum = 8;

  fb3  = malloc(sizeof(FILEFINDBUF3) * maxnum);
  if(!fb3)
    return rc;

  nm = maxnum;
  DosError(FERR_DISABLEHARDERR);
  rc = DosFindFirst(dir,
                    &hdir,
                    FILE_DIRECTORY | FILE_SYSTEM | FILE_HIDDEN |
                    FILE_READONLY  | FILE_NORMAL | FILE_ARCHIVED |
                    MUST_HAVE_DIRECTORY,
                    fb3,
                    sizeof(FILEFINDBUF3) * maxnum,
                    &nm,
                    FIL_STANDARD);
  if(!rc) {
    pb = (PBYTE)fb3;
    for(x = 0;x < nm && rc != ERROR_NOT_ENOUGH_MEMORY;x++) {
      pffb = (FILEFINDBUF3 *)pb;
      if((pffb->attrFile & FILE_DIRECTORY) != 0 &&
         (*pffb->achName != '.' ||
         (pffb->achName[1] != '.' && pffb->achName[1]))) {
        info->flags |= T_NEEDSCAN;
        break;
      }
      pb += pffb->oNextEntryOffset;
    }
    DosFindClose(hdir);
  }

  free(fb3);

  return rc;
}


APIRET BuildTree (TREE *head,char *dir) {

  char               *mask;
  FILEFINDBUF3       *fb3,*pffb;
  BYTE               *pb;
  HDIR                hdir = HDIR_CREATE;
  ULONG               nm,x,maxnum;
  APIRET              rc = ERROR_NOT_ENOUGH_MEMORY;
  TREE               *info,*last = NULL;

  maxnum = 32;

  /* allocate required memory */
  mask = malloc(CCHMAXPATH + 4);
  if(!mask)
    return rc;
  fb3  = malloc(sizeof(FILEFINDBUF3) * maxnum);
  if(!fb3) {
    free(mask);
    return rc;
  }

  nm = maxnum;
  sprintf(mask,"%s%s*",dir,(dir[strlen(dir) - 1] != '\\') ? "\\" : "");
  DosError(FERR_DISABLEHARDERR);
  rc = DosFindFirst(mask,
                    &hdir,
                    FILE_DIRECTORY | FILE_SYSTEM | FILE_HIDDEN |
                    FILE_READONLY  | FILE_NORMAL | FILE_ARCHIVED |
                    MUST_HAVE_DIRECTORY,
                    fb3,
                    sizeof(FILEFINDBUF3) * maxnum,
                    &nm,
                    FIL_STANDARD);
  if(!rc) {
    while(!rc) {
      pb = (PBYTE)fb3;
      for(x = 0;x < nm && rc != ERROR_NOT_ENOUGH_MEMORY;x++) {
        pffb = (FILEFINDBUF3 *)pb;
        if((pffb->attrFile & FILE_DIRECTORY) != 0 &&
           (*pffb->achName != '.' ||
           (pffb->achName[1] != '.' && pffb->achName[1]))) {
          info = malloc(sizeof(TREE));
          if(info) {
            memset(info,0,sizeof(TREE));
            info->name = strdup(pffb->achName);
            if(info->name) {
              if(!head->child)
                head->child = info;
              info->parent = head;
              sprintf(mask,"%s%s%s\\*",dir,
                      (dir[strlen(dir) - 1] != '\\') ? "\\" : "",
                      pffb->achName);
              rc = StubTree(info,mask);
              if(last)
                last->sibling = info;
              last = info;
              if(rc == ERROR_NOT_ENOUGH_MEMORY)
                break;
            }
            else {
              free(info);
              info = NULL;
            }
          }
          if(!info) {
            rc = ERROR_NOT_ENOUGH_MEMORY;
            break;
          }
        }
        pb += pffb->oNextEntryOffset;
      }
      nm = maxnum;
      DosError(FERR_DISABLEHARDERR);
      rc = DosFindNext(hdir,
                       fb3,
                       sizeof(FILEFINDBUF3) * maxnum,
                       &nm);
    }
    DosFindClose(hdir);
  }

  free(mask);
  free(fb3);

  return rc;
}


TREE *CreateRoot (char *name) {

  TREE *info;

  info = malloc(sizeof(TREE));
  if(info) {
    memset(info,0,sizeof(TREE));
    info->name = strdup(name);
    if(!info->name) {
      free(info);
      info = NULL;
    }
  }
  return info;
}


char *FullTreeName (TREE *info) {

  TREE       *prev,*parent;
  static char str[CCHMAXPATH + 4];
  char        revme[CCHMAXPATHCOMP];

  *str = 0;
  prev = info->parent;
  while(prev) {
    *revme = 0;
    parent = prev->parent;
    if(parent && parent->name[strlen(parent->name) - 1] != '\\')
      strcpy(revme,"\\");
    strcat(revme,prev->name);
    prev = parent;
    strrev(revme);
    strcat(str,revme);
  }
  if(*str) {
    strrev(str);
    if(str[strlen(str) - 1] != '\\')
      strcat(str,"\\");
  }
  strcat(str,info->name);
  return str;
}


char *TreeName (TREE *info) {

  TREE       *prev;
  char       *p;
  static char str[CCHMAXPATH + 780];

  p = str;
  if(info->child || (info->flags & T_NEEDSCAN)) {
    *p++ = ((info->flags & T_EXPANDED) != 0) ? '-' : '+';
    *p = 0;
  }
  else {
    *p++ = ' ';
    *p = 0;
  }
  prev = info->parent;
  while(prev) {
    if(prev->sibling && prev->parent)
      *p++ = '³';
    else
      *p++ = ' ';
    *p = 0;
    prev = prev->parent;
  }
  strrev(str + 1);
  *p = (p != str + 1) ? (info->sibling) ? 'Ã' : 'À' : 0;
  if(*p) {
    p++;
    *p = 0;
  }
  strcpy(p,info->name);
  return str;
}


ULONG CountTree (TREE *head) {

  TREE  *info;
  ULONG  count = 0;

  info = head;
  while(info) {
    count++;
    if((info->flags & T_EXPANDED) && info->child)
      count += CountTree(info->child);
    info = info->sibling;
  }
  return count;
}


void BuildTreeArray (TREE *head,TREE **array,ULONG *count) {

  TREE *info;

  info = head;
  while(info) {
    array[(*count)++] = info;
    if((info->flags & T_EXPANDED) && info->child)
      BuildTreeArray(info->child,array,count);
    info = info->sibling;
  }
}


APIRET CollapseTree (TREE *head,BOOL expand,BOOL nosibs) {

  TREE  *info;
  APIRET rc = 0;

  info = head;
  while(info) {
    if(!expand)
      info->flags &= (~T_EXPANDED);
    else {
      if(info->child)
        info->flags |= T_EXPANDED;
      else if((info->flags & T_NEEDSCAN) != 0) {
        info->flags &= (~T_NEEDSCAN);
        rc = BuildTree(info,FullTreeName(info));
        if(rc == ERROR_NOT_ENOUGH_MEMORY)
          break;
        if(info->child)
          info->flags |= T_EXPANDED;
      }
    }
    if(info->child) {
      rc = CollapseTree(info->child,expand,FALSE);
      if(rc == ERROR_NOT_ENOUGH_MEMORY)
        break;
    }
    if(!nosibs)
      info = info->sibling;
    else
      break;
  }
  return rc;
}


void ShowTreeItem (TREE **array,ULONG topitem,ULONG item,char attr) {

  char *p;

  p = TreeName(array[item]);
  VioWrtCharStrAtt(p,strlen(p),1 + (item - topitem),0,&attr,0);
}


void ShowTree (TREE **array,ULONG count,ULONG topitem,ULONG item) {

  ULONG x;
  int   attr = (7 << 8) | ' ';

  VioScrollUp(1,0,vio.row - 3,vio.col - 1,-1,(char *)&attr,0);
  for(x = topitem;x < count && x - topitem < vio.row - 3;x++)
    ShowTreeItem(array,topitem,x,(x == item) ? (7 << 4) : 7);
}


char *TreeView (char *drives) {

  static TREE *tree = NULL;
  TREE       **array = NULL,*last = NULL,*info;
  APIRET       rc;
  ULONG        count,dc,item = 0,topitem = 0,z,x;
  char         arg[6] = " :\\",*dir,*p;
  BOOL         okay = TRUE;
  int          key,attr;

  if(drives && !*drives) {  /* reset trees */
    if(tree)
      FreeTree(tree);
    tree = NULL;
    return NULL;
  }

  treeing = TRUE;
  dir = NULL;
  attr = (7 << 8) | ' ';
  VioScrollUp(1,0,vio.row - 3,vio.col - 1,-1,(char *)&attr,0);
  attr = ((7 << 4) << 8) | ' ';
  VioWrtNCell((char *)&attr,vio.col,vio.row - 2,0,0);
  attr = (7 << 8) | ' ';
  WriteCenterString("Return = Select, + = Expand, - = "
                    "Collapse, Escape = Cancel",
                    vio.row - 2,0,vio.col);

ReStart:
  ThreadMsg(NULL);
  if(!tree) {
    WriteCenterString("Building tree...",(vio.row - 2) / 2,0,vio.col);
    if(drives) {
      for(dc = 0;drives[dc];dc++) {
        *arg = toupper(drives[dc]);
        info = CreateRoot(arg);
        if(info) {
          if(!tree)
            tree = info;
          else
            last->sibling = info;
          last = info;
          info->flags |= T_EXPANDED;
          rc = BuildTree(info,info->name);
          if(rc == ERROR_NOT_ENOUGH_MEMORY)
            break;
        }
        else {
          rc = ERROR_NOT_ENOUGH_MEMORY;
          break;
        }
      }
    }
    else {

      ULONG ulDriveNum,ulDriveMap;

      DosError(FERR_DISABLEHARDERR);
      rc = DosQCurDisk(&ulDriveNum,&ulDriveMap);
      if(!rc) {
        for(x = 0;x < 26;x++) {
          if((ulDriveMap & (1L << x)) != 0) {
            *arg = toupper(x + 'A');
            info = CreateRoot(arg);
            if(info) {
              arg[3] = '*';
              arg[4] = 0;
              rc = StubTree(info,arg);
              arg[3] = 0;
              if(rc && rc != ERROR_NO_MORE_FILES)
                FreeTree(info);
              else {
                if(!tree)
                  tree = info;
                else
                  last->sibling = info;
                last = info;
              }
            }
            else {
              rc = ERROR_NOT_ENOUGH_MEMORY;
              break;
            }
          }
        }
      }
    }
  }

  if(!tree)
    goto Abort;

  last = NULL;
  count = CountTree(tree);
  array = malloc(sizeof(TREE *) * count);
  if(array) {
    z = 0;
    BuildTreeArray(tree,array,&z);
    ShowTree(array,count,topitem,item);
  }

  while(okay && array && count) {
    if(last != array[item]) {
      p = FullTreeName(array[item]);
      VioWrtNChar(" ",vio.col,0,0,0);
      VioWrtCharStr(p,strlen(p),0,0,0);
      last = array[item];
    }

    key = get_ch(-1);
    switch(key) {
      case 256:
        ThreadMsg(NULL);
        break;

      case '\r':
        okay = FALSE;
        dir = FullTreeName(array[item]);
        break;

      case 26:    /* ctrl+z -- restart tree */
        VioScrollUp(1,0,vio.row - 3,vio.col - 1,-1,(char *)&attr,0);
        free(array);
        array = NULL;
        FreeTree(tree);
        tree = NULL;
        item = 0;
        topitem = 0;
        count = 0;
        last = NULL;
        goto ReStart;

      case 19 | 256:  /* alt+r -- rescan branch */
        if(array[item]->child)
          FreeTree(array[item]->child);
        array[item]->child = NULL;
        array[item]->flags &= (~T_NEEDSCAN);
        rc = BuildTree(array[item],FullTreeName(array[item]));
        if(rc == ERROR_NOT_ENOUGH_MEMORY) {
          okay = FALSE;
          break;
        }
        count = CountTree(tree);
        last = NULL;
        free(array);
        array = malloc(sizeof(TREE *) * count);
        if(array) {
          z = 0;
          BuildTreeArray(tree,array,&z);
          if(item > count - 1)
            item = count - 1;
          ShowTree(array,count,topitem,item);
        }
        else {
          rc = ERROR_NOT_ENOUGH_MEMORY;
          okay = FALSE;
        }
        break;

      case 5:         /* ctrl+e -- expand all branches of current item */
        VioWrtNChar(" ",vio.col,0,0,0);
        WriteCenterString("Expanding branches...",0,0,vio.col);
        rc = CollapseTree(array[item],TRUE,TRUE);
        goto AfterExpand;

      case 18 | 256:  /* alt+e -- expand all branches */
        VioWrtNChar(" ",vio.col,0,0,0);
        WriteCenterString("Expanding all branches...",0,0,vio.col);
        /* intentional fallthru */
      case 46 | 256:  /* alt+c -- collapse all branches*/
        rc = CollapseTree(tree,(key == (18 | 256)),FALSE);
AfterExpand:
        if(rc == ERROR_NOT_ENOUGH_MEMORY) {
          okay = FALSE;
          break;
        }
        if(key == (46 | 256))
          topitem = item = 0;
        last = NULL;
        free(array);
        count = CountTree(tree);
        array = malloc(sizeof(TREE *) * count);
        if(array) {
          z = 0;
          BuildTreeArray(tree,array,&z);
          if(item > count - 1)
            item = count - 1;
          ShowTree(array,count,topitem,item);
        }
        break;

      case 61 | 256:  /* F3 */
      case 45 | 256:  /* alt+x */
      case '\x1b':
        okay = FALSE;
        break;

      case '-':
        if((array[item]->flags & T_EXPANDED) != 0) {
          array[item]->flags &= (~T_EXPANDED);
          free(array);
          count = CountTree(tree);
          array = malloc(sizeof(TREE *) * count);
          if(array) {
            z = 0;
            BuildTreeArray(tree,array,&z);
            if(item > count - 1)
              item = count - 1;
            ShowTree(array,count,topitem,item);
          }
        }
        else
          goto DefKey;
        break;

      case '=':
      case '+':
        if(((array[item]->flags & T_NEEDSCAN) == 0) &&
           (!array[item]->child ||
            ((array[item]->flags & T_EXPANDED) != 0)))
          goto DefKey;
        if((array[item]->flags & T_NEEDSCAN) != 0) {
          array[item]->flags &= (~T_NEEDSCAN);
          rc = BuildTree(array[item],FullTreeName(array[item]));
          if(rc == ERROR_NOT_ENOUGH_MEMORY) {
            okay = FALSE;
            break;
          }
        }
        if(array[item]->child) {
          array[item]->flags |= T_EXPANDED;
          free(array);
          count = CountTree(tree);
          array = malloc(sizeof(TREE *) * count);
          if(array) {
            z = 0;
            BuildTreeArray(tree,array,&z);
            if(item > count - 1)
              item = count - 1;
            topitem = item;
            if(topitem + (vio.row - 4) > count - 1)
              topitem = count - (vio.row - 3);
            if(topitem > count - 1)
              topitem = 0;
            ShowTree(array,count,topitem,item);
          }
        }
        else
          ShowTreeItem(array,topitem,item,(7 << 4));
        break;

      case 72 | 256:  /* up */
        if(item) {
          ShowTreeItem(array,topitem,item,7);
          item--;
          if(item < topitem) {
            topitem--;
            VioScrollDn(1,0,vio.row - 3,vio.col - 1,1,(char *)&attr,0);
          }
          ShowTreeItem(array,topitem,item,(7 << 4));
        }
        break;

      case 80 | 256:  /* down */
        if(item < count - 1) {
          ShowTreeItem(array,topitem,item,7);
          item++;
          if(item > topitem + vio.row - 4) {
            topitem++;
            VioScrollUp(1,0,vio.row - 3,vio.col - 1,1,(char *)&attr,0);
          }
          ShowTreeItem(array,topitem,item,(7 << 4));
        }
        break;

      case 119 | 256:   /* ctrl+home */
      case 71 | 256:    /* home */
        if(key != (119 | 256) && item != topitem) {
          ShowTreeItem(array,topitem,item,7);
          item = topitem;
          ShowTreeItem(array,topitem,item,(7 << 4));
        }
        else if(item || topitem) {
          topitem = item = 0;
          ShowTree(array,count,topitem,item);
        }
        break;

      case 117 | 256:   /* ctrl+end */
      case 79 | 256:    /* end */
        if(count) {
          if(key != (117 | 256) && item != topitem + (vio.row - 4)) {
            ShowTreeItem(array,topitem,item,7);
            item = topitem + (vio.row - 4);
            if(item > count - 1)
              item = count - 1;
            ShowTreeItem(array,topitem,item,(7 << 4));
          }
          else {
            item = count - 1;
            topitem = item - (vio.row - 4);
            if(topitem > count - 1)
              topitem = 0;
            ShowTree(array,count,topitem,item);
          }
        }
        break;

      case 73 | 256:    /* page up */
        if(topitem) {
          topitem -= vio.row - 3;
          if(topitem > count - 1)
            topitem = 0;
          item = topitem + (vio.row - 4);
          if(item > count - 1)
            item = count - 1;
        }
        else
          item = topitem;
        ShowTree(array,count,topitem,item);
        break;

      case 81 | 256:    /* page down */
        topitem += vio.row - 3;
        if(topitem > count - 1)
          topitem = count - (vio.row - 4);
        if(topitem + (vio.row - 4) > count - 1)
          topitem = count - (vio.row - 3);
        if(topitem > count - 1)
          topitem = 0;
        if(item < topitem)
          item = topitem;
        else {
          item = topitem + (vio.row - 4);
          if(item > count - 1)
            item = count - 1;
        }
        ShowTree(array,count,topitem,item);
        break;

      default:
DefKey:
        if(!(key & 256)) {

          ULONG this = (ULONG)-1,wasitem = item,wastop = topitem;

          if(key >= 'A' && key <= 'Z') {
            info = tree;
            while(this == (ULONG)-1 && info) {
              if(toupper(*info->name) == toupper(key)) {
                for(x = 0;x < count;x++) {
                  if(array[x] == info) {
                    this = x;
                    break;
                  }
                }
              }
              info = info->sibling;
            }
          }
          if(this == (ULONG)-1) {
            for(x = item + 1;x < count;x++) {
              if(toupper(*array[x]->name) == toupper(key)) {
                this = x;
                break;
              }
            }
          }
          if(this == (ULONG)-1) {
            for(x = 0;x < item;x++) {
              if(toupper(*array[x]->name) == toupper(key)) {
                this = x;
                break;
              }
            }
          }
          if(this != (ULONG)-1) {
            item = this;
            topitem = item - ((vio.row - 4) / 2);
            if(topitem > count - 1)
              topitem = 0;
            if(count - topitem < vio.row - 3)
              topitem = count - (vio.row - 3);
            if(topitem > count - 1)
              topitem = 0;
            if(item != wasitem) {
              if(topitem != wastop)
                ShowTree(array,count,topitem,item);
              else {
                ShowTreeItem(array,wastop,wasitem,7);
                ShowTreeItem(array,topitem,item,(7 << 4));
              }
            }
          }
        }
        break;
    }
  }

Abort:

  if(array)
    free(array);

  if(rc == ERROR_NOT_ENOUGH_MEMORY || !retaintree) {
    FreeTree(tree);
    tree = NULL;
  }

  if(rc == ERROR_NOT_ENOUGH_MEMORY) {

    int responses[] = {0,0};

    SimpleInput("Warning!",
                " ***FMT ran out of memory!  (Press a key to continue)*** ",
                100,250,3000,responses);
  }

  treeing = FALSE;
  ThreadMsg(NULL);
  return dir;
}

