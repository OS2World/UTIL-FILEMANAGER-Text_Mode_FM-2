#define     C_COPY        1
#define     C_MOVE        2
#define     C_DELETE      3
#define     C_PRINT       4
#define     C_TOUCH       5
#define     C_DELETEPERM  6

#define     S_NAME        0
#define     S_SIZE        1
#define     S_DATE        2
#define     S_FULLNAME    3

#define     I_DATE        0x00000001
#define     I_SIZE        0x00000002
#define     I_ATTR        0x00000004

#define     F_MARKED      0x0001

#define     T_EXPANDED    0x0001
#define     T_NEEDSCAN    0x0002

#pragma pack(1)

typedef struct FILES {
  char  *filename;
  ULONG  cbFile;
  ULONG  cbList;
  FDATE  date;
  FTIME  time;
  USHORT attrFile;
  USHORT flags;
} FILES;

typedef struct TREE {
  char        *name;
  struct TREE *parent;
  struct TREE *sibling;
  struct TREE *child;
  USHORT       flags;
} TREE;

#pragma pack()

typedef struct ASSOC {
  char         *cmd;
  char         *mask;
  struct ASSOC *next;
} ASSOC;

#pragma data_seg(DATA1)

#ifdef DEFINE_GLOBALS
  #define DATADEF
#else
  #define DATADEF extern
#endif

DATADEF FILES       *files;
DATADEF char         directory[CCHMAXPATH];
DATADEF char         target[CCHMAXPATH];
DATADEF char         homedir[CCHMAXPATH];
DATADEF char         filter[CCHMAXPATH];
DATADEF char         lastdir[CCHMAXPATH];
DATADEF char         volume[13];
DATADEF char        *restorescreen,*myscreen;
DATADEF USHORT       rx,ry;
DATADEF VIOINTENSITY vioint;
DATADEF KBDINFO      kbdorg;
DATADEF ULONG        freespace;
DATADEF ULONG        numfiles,nummarked;
DATADEF ULONG        sizefiles,sizemarked;
DATADEF ULONG        topfile,file;
DATADEF ULONG        infolevel;
DATADEF ULONG        sorttype;
DATADEF char        *editor;
DATADEF char        *viewer;
DATADEF char        *savedirs[11];
DATADEF BOOL         autosavecfg;
DATADEF BOOL         invertsort;
DATADEF BOOL         dirstop;
DATADEF BOOL         shelled;
DATADEF ULONG        findattr;
DATADEF ULONG        threads;
DATADEF ULONG        mustcompletes;
DATADEF int          primarythread;
DATADEF char        *usercmds[13];
DATADEF BOOL         cdonexit,startintree;
DATADEF BOOL         recurse;
DATADEF BOOL         viewing,treeing,viewonly,helping;
DATADEF BOOL         includedirsinalllists;
DATADEF BOOL         retaintree;
DATADEF BOOL         idlethreads;
DATADEF char        *sayanykey;
DATADEF ASSOC       *assoc;
DATADEF char        *lastfilter[25];
DATADEF int          lastf;
DATADEF char        *lastcmdline[25];
DATADEF int          lastcmd;

#pragma data_seg(DATA2)

/* fmt.c */
void DisplayCurrFile (FILES *f);
void DisplayNumSizeFiles (void);
void DisplayFreeSpace (char curdrive);
APIRET BuildList (char *dir,BOOL first,BOOL multdirs);
void DisplayFiles (void);
void DisplayTarget (void);
void AdjustLastDir (void);

/* viewer.c */
char *memstr (register char *t,char *s,ULONG lent,ULONG lens,BOOL ins);
int ViewFile (char *filename);

/* seldom.c */
void Info (void);
void ShowFKeys (void);
void About (void);

/* misc.c */
void ShowDrives (USHORT y);
void WriteCenterString (char *line,USHORT y,USHORT x,int len);
size_t CommaFormat (char *buf,int bufsize,ULONG N);
BOOL IsExecutable (char *filename);
int  IsFile (char *filename);
BOOL IsRoot (char *filename);
BOOL IsValidDrive (char drive);
char *MakeFullName (char *s);
APIRET MakeCurrDir (char *s);
APIRET CurrDir (char *curdir);
ULONG FreeSpace (char drive);
int ThreadID (void);
void SetupConsole (void);
USHORT CheckShift (void);
int  Shell (char *cmdline,BOOL pause);
BOOL Associate (char *filename);
BOOL MultiWildCard (char *fstra,char *fcarda);
BOOL WildCard (char *fstra,char *fcarda,BOOL notfile);
char *stristr (register char *t, char *s);
char * strip_lead_char (char *strip,char *a);
char * strip_trail_char (char *strip,char *a);
 #define lstrip(s)  strip_lead_char(" \t",(s))
 #define rstrip(s)  strip_trail_char(" \t",(s))
 #define stripcr(s) strip_trail_char("\r\n",(s))

/* init.c */
void Init (int argc,char **argv);
void DeInit (void);
void WriteFMTCFG (void);

/* edit.c */
int EnterLine (char *buf,ULONG len,USHORT y,char *filename,
               char **choices,int numc,int start);
int EditAttributes (FILES *f);

/* thread.c */
BOOL StartThread (ULONG cmd);
void PrintFile (void *args);
BOOL WriteLongName (char *filename,char *longname);

/* tree.c */
char *TreeView (char *drives);

#include "window.h"
