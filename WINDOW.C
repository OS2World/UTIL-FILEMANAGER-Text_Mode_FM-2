#define DEFINE_WINDOWGLOBALS

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
#include "window.h"

#pragma alloc_text(COMMON,get_ch,SaveScreen,RestoreScreen,ShowCursor)
#pragma alloc_text(WINDOW,SimpleInput,ShowFullWindow)

extern void ThreadMsg (char *str);


int get_ch (clock_t timer) {

  KBDKEYINFO kbd;
  USHORT     wait = IO_WAIT;
  clock_t    t;
  int        key = 0;

  if(timer != (clock_t)-1) {    /* we're on a timer */
    wait = IO_NOWAIT;
    t = clock() + ((timer * CLOCKS_PER_SEC) / 1000);
    do {
      if(!KbdCharIn(&kbd,wait,0)) {
        if(kbd.fbStatus & 0x40) {
          if(kbd.chChar == 0 || kbd.chChar == 0xe0)
            key = kbd.chScan | 256;
          else
            key = kbd.chChar;
          break;
        }
        else if(kbd.fbStatus & 0x01)
          return 256;
      }
      DosSleep(33L);    /* snooze a bit... */
    } while(t > clock());
  }
  else {  /* we're NOT on a timer */
    while(!KbdCharIn(&kbd,wait,0)) {
      if(kbd.fbStatus & 0x40) {
        if(kbd.chChar == 0 || kbd.chChar == 0xe0)
          key = kbd.chScan | 256;
        else
          key = kbd.chChar;
        break;
      }
      else if(kbd.fbStatus & 0x01)
        return 256;
    }
  }
  return key;
}


char *SaveScreen (char *buf) {

  USHORT size;

  if(buf)
    free(buf);
  size = ((vio.col + 1) * (vio.row + 1)) * 2;
  buf = malloc(size);
  if(buf)
    VioReadCellStr(buf,&size,0,0,0);
  return buf;
}


void RestoreScreen (char *buf) {

  VioSetMode(&vio,0);
  if(buf) 
    VioWrtCellStr(buf,((vio.col + 1) * (vio.row + 1)) * 2,0,0,0);
}


void ShowCursor (BOOL hide) {

  VIOCURSORINFO viocurs;

  memset(&viocurs,0,sizeof(viocurs));
  VioGetCurType(&viocurs,0);
  viocurs.attr = (hide) ? -1 : 0;
  VioSetCurType(&viocurs,0);
}


int SimpleInput (char *title,char *text,ULONG beep,ULONG dur,ULONG wait,
                 int *responses) {

  char  *buf;
  int    xlen,key = 0,x;
  USHORT len = (vio.col * 7) * 2,start,mlen,cell;
  char   sattr = 8;
  BOOL   okay = TRUE;

  if(!title)
    title = "";
  if(!text)
    return key;
  xlen = min(max(strlen(title),strlen(text)),vio.col - 6);
  mlen = xlen + 6;
  start = (vio.col - xlen) / 2;

  buf = malloc(len);
  if(buf) {
    /* save screen under */
    VioReadCellStr(buf,&len,(vio.row / 2) - 2,0,0);
    /* draw borders */
    /* first, bright white left and top */
    cell = ((((7 | 8) << 4) | 7) << 8) | ' ';
    VioWrtNCell((char *)&cell,mlen,(vio.row / 2) - 2,start,0);
    VioWrtNCell((char *)&cell,1,(vio.row / 2) - 1,start,0);
    VioWrtNCell((char *)&cell,1,(vio.row / 2),start,0);
    VioWrtNCell((char *)&cell,1,(vio.row / 2) + 1,start,0);
    VioWrtNCell((char *)&cell,1,(vio.row / 2) + 2,start,0);
    VioWrtNCell((char *)&cell,1,(vio.row / 2) + 3,start,0);
    /* now dark grey right and bottom */
    cell = (((8 << 4) | 7) << 8) | ' ';
    VioWrtNCell((char *)&cell,1,(vio.row / 2) - 1,start + (mlen - 1),0);
    VioWrtNCell((char *)&cell,1,(vio.row / 2),start + (mlen - 1),0);
    VioWrtNCell((char *)&cell,1,(vio.row / 2) + 1,start + (mlen - 1),0);
    VioWrtNCell((char *)&cell,1,(vio.row / 2) + 2,start + (mlen - 1),0);
    VioWrtNCell((char *)&cell,mlen - 1,(vio.row / 2) + 3,start + 1,0);
    /* fill title area with light grey foreground, red background */
    cell = (((7 << 4) | 4) << 8) | ' ';
    VioWrtNCell((char *)&cell,mlen - 2,(vio.row / 2) - 1,start + 1,0);
    /* fill text area with light grey foreground, black background */
    cell = (((7 << 4) | 0) << 8) | ' ';
    VioWrtNCell((char *)&cell,mlen - 4,(vio.row / 2) + 1,start + 2,0);
    /* make interior border -- first white on light grey */
    cell = (((7 << 4) | (7 | 8)) << 8) | 'Ú';
    VioWrtNCell((char *)&cell,1,(vio.row / 2),start + 1,0);
    cell = (((7 << 4) | (7 | 8)) << 8) | '¿';
    VioWrtNCell((char *)&cell,1,(vio.row / 2),start + (mlen - 2),0);
    cell = (((7 << 4) | (7 | 8)) << 8) | 'Ä';
    VioWrtNCell((char *)&cell,mlen - 4,(vio.row / 2),start + 2,0);
    cell = (((7 << 4) | (7 | 8)) << 8) | '³';
    VioWrtNCell((char *)&cell,1,(vio.row / 2) + 1,start + 1,0);
    cell = (((7 << 4) | (7 | 8)) << 8) | 'À';
    VioWrtNCell((char *)&cell,1,(vio.row / 2) + 2,start + 1,0);
    /* now dark grey on light grey */
    cell = (((7 << 4) | 8) << 8) | 'Ä';
    VioWrtNCell((char *)&cell,mlen - 4,(vio.row / 2) + 2,start + 2,0);
    cell = (((7 << 4) | 8) << 8) | 'Ù';
    VioWrtNCell((char *)&cell,1,(vio.row / 2) + 2,start + (mlen - 2),0);
    cell = (((7 << 4) | 8) << 8) | '³';
    VioWrtNCell((char *)&cell,1,(vio.row / 2) + 1,start + (mlen - 2),0);

    /* insert title */
    VioWrtCharStr(title,
                  min(xlen,strlen(title)),
                  (vio.row / 2) - 1,
                  (start + (mlen / 2)) - (min(xlen,strlen(title)) / 2),
                  0);
    /* insert text */
    VioWrtCharStr(text,
                  min(xlen,strlen(text)),
                  (vio.row / 2) + 1,
                  (start + (mlen / 2)) - (min(xlen,strlen(text)) / 2),
                  0);
    /* draw shadow */
    VioWrtNAttr(&sattr,1,(vio.row / 2) - 1,start + mlen,0);
    VioWrtNAttr(&sattr,1,(vio.row / 2),start + mlen,0);
    VioWrtNAttr(&sattr,1,(vio.row / 2) + 1,start + mlen,0);
    VioWrtNAttr(&sattr,1,(vio.row / 2) + 2,start + mlen,0);
    VioWrtNAttr(&sattr,1,(vio.row / 2) + 3,start + mlen,0);
    VioWrtNAttr(&sattr,mlen,(vio.row / 2) + 4,start + 1,0);
    VioSetCurPos((vio.row / 2) + 1,start + (mlen - 3),0);
    if(responses)
      ShowCursor(FALSE);
    if(beep)
      DosBeep(beep,dur);
    if(!responses)
      DosSleep(wait);
    else {
      KbdFlushBuffer(0);
      while(okay) {
        key = get_ch(-1);
        if(!*responses)
          break;
        for(x = 0;responses[x];x++) {
          if(key == responses[x]) {
            okay = FALSE;
            break;
          }
        }
      }
    }
    ShowCursor(TRUE);
    VioWrtCellStr(buf,len,(vio.row / 2) - 2,0,0);
    free(buf);
    KbdFlushBuffer(0);
    ThreadMsg(NULL);
  }
  return key;
}


void ShowFullWindow (char *title) {

  int    attr = ((7 << 4) << 8) | ' ';
  USHORT line;

  /* display empty "window" */
  VioScrollUp(3,0,vio.row - 5,vio.col - 1,-1,(char *)&attr,0);
  attr = (((7 << 4) | 4) << 8) | ' ';
  VioWrtNCell((char *)&attr,vio.col,2,0,0);
  attr = (((7 << 4) | (7 | 8)) << 8) | 'Ä';
  VioWrtNCell((char *)&attr,vio.col - 4,3,1,0);
  attr = (((7 << 4) | 8) << 8) | 'Ä';
  VioWrtNCell((char *)&attr,vio.col - 4,vio.row - 6,1,0);
  attr = (((7 << 4) | (7 | 8)) << 8) | 'Ú';
  VioWrtNCell((char *)&attr,1,3,1,0);
  attr = (((7 << 4) | (7 | 8)) << 8) | '¿';
  VioWrtNCell((char *)&attr,1,3,vio.col - 3,0);
  attr = (((7 << 4) | (7 | 8)) << 8) | 'À';
  VioWrtNCell((char *)&attr,1,vio.row - 6,1,0);
  attr = (((7 << 4) | 8) << 8) | 'Ù';
  VioWrtNCell((char *)&attr,1,vio.row - 6,vio.col - 3,0);
  attr = (((7 << 4) | (7 | 8)) << 8) | '³';
  for(line = 4;line < vio.row - 6;line++)
    VioWrtNCell((char *)&attr,1,line,1,0);
  attr = (((7 << 4) | 8) << 8) | '³';
  for(line = 4;line < vio.row - 6;line++)
    VioWrtNCell((char *)&attr,1,line,vio.col - 3,0);
  attr = ' ';
  VioWrtNCell((char *)&attr,vio.col - 1,vio.row - 4,1,0);
  for(line = 3;line < vio.row - 4;line++)
    VioWrtNCell((char *)&attr,1,line,vio.col - 1,0);
  attr = ((8 << 4) << 8) | ' ';
  VioWrtNCell((char *)&attr,1,2,vio.col - 1,0);
  VioWrtNCell((char *)&attr,1,vio.row - 4,0,0);
  /* display title */
  VioWrtCharStr(title,min(strlen(title),vio.col - 2),2,
                ((vio.col - 2) - min(strlen(title),vio.col - 2)) / 2,0);
}

