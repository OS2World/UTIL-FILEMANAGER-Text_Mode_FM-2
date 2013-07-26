typedef struct WINDOW {
  USHORT x,y,cx,cy;
  char   colorborder,colortitle,colorbody;
  char  *title,*text;
  char  *savbuf;
} WINDOW;

int    SimpleInput    (char *title,char *text,ULONG beep,ULONG dur,ULONG wait,
                       int *responses);
int    get_ch         (clock_t timer);
void   ShowCursor     (BOOL hide);
char * SaveScreen     (char *buf);
void   RestoreScreen  (char *buf);
void   ShowFullWindow (char *title);

#ifdef DEFINE_WINDOWGLOBALS
  #define WDATADEF
#else
  #define WDATADEF extern
#endif

WDATADEF VIOMODEINFO vio;
