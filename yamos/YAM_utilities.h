#ifndef YAM_UTILITIES_H
#define YAM_UTILITIES_H

/***************************************************************************

 YAM - Yet Another Mailer
 Copyright (C) 1995-2000 by Marcel Beck <mbeck@yam.ch>
 Copyright (C) 2000-2005 by YAM Open Source Team

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

 YAM Official Support Site :  http://www.yam.ch
 YAM OpenSource project    :  http://sourceforge.net/projects/yamos/

 $Id$

***************************************************************************/

#include <stdio.h>

#include <dos/dos.h>
#include <intuition/classusr.h>
#include <mui/Toolbar_mcc.h>
#include <proto/intuition.h>

#include "SDI_compiler.h"
#include "YAM_folderconfig.h"
#include "YAM_stringsizes.h"

#define STR(x)  STR2(x)
#define STR2(x) #x

// some forward declarations
#ifndef YAM_FOLDERCONFIG_H
struct Folder;
#endif

#ifndef YAM_MIME_H
struct TranslationTable;
#endif

#ifndef YAM_READ_H
struct ReadMailData;
#endif

enum DateStampType { DSS_DATE, DSS_TIME, DSS_WEEKDAY, DSS_DATETIME,
  DSS_USDATETIME, DSS_UNIXDATE, DSS_BEAT, DSS_DATEBEAT };

enum TZConvert { TZC_NONE, TZC_UTC, TZC_LOCAL };

enum ReqFileType { ASL_ABOOK=0, ASL_CONFIG, ASL_DETACH, ASL_ATTACH,
  ASL_REXX, ASL_PHOTO, ASL_IMPORT, ASL_FOLDER };

struct Person
{       
   char Address[SIZE_ADDRESS];
   char RealName[SIZE_REALNAME];
};

struct ExpandTextData
{
   char *            OS_Name;
   char *            OS_Address;
   char *            OM_Subject;
   struct DateStamp *OM_Date;
   int               OM_TimeZone;
   char *            OM_MessageID;
   char *            R_Name;
   char *            R_Address;
   char *            HeaderFile;
};

struct MailInfo
{
   int   Pos;
   BOOL  Display;
   char *FName;
};

struct TempFile
{
   FILE *FP;
   char  Filename[SIZE_PATH+SIZE_MFILE];
};

struct BodyChunkData
{
   ULONG * Colors;
   UBYTE * Body;
   int     Width;
   int     Height;
   int     Depth;
   int     Compression;
   int     Masking;
   char    File[SIZE_NAME];
};

struct NewToolbarEntry
{
   APTR label;
   APTR help;
};

// Library open/close macros
#if defined(__amigaos4__)
#define INITLIB(iface, func)      ((iface)=func)
#define CLOSELIB(lib, iface)      { if((iface) && (lib)) { DropInterface((APTR)(iface)); iface = NULL; CloseLibrary((struct Library *)lib); lib = NULL; } }
#define GETINTERFACE(iface, base) (iface = (APTR)GetInterface((struct Library *)(base), "main", 1L, NULL))
#define DROPINTERFACE(iface)      { DropInterface((APTR)(iface)); iface = NULL; }
#else
#define INITLIB(iface, func)      func
#define CLOSELIB(lib, iface)      { if((lib)) { CloseLibrary((struct Library *)lib); lib = NULL; } }
#define GETINTERFACE(iface, base) TRUE
#define DROPINTERFACE(iface)
#endif

// misc defines
#define PGPLOGFILE          "T:PGP.log"
#define NOERRORS            (1<<4)
#define KEEPLOG             (1<<5)
#define hasNoErrorsFlag(v)  (isFlagSet((v), NOERRORS))
#define hasKeepLogFlag(v)   (isFlagSet((v), KEEPLOG))

// RequesterMode flags & macros
#define REQF_NONE             0
#define REQF_SAVEMODE         (1<<0)
#define REQF_MULTISELECT      (1<<1)
#define REQF_DRAWERSONLY      (1<<2)
#define hasSaveModeFlag(v)    (isFlagSet((v), REQF_SAVEMODE))
#define hasMultiSelectFlag(v) (isFlagSet((v), REQF_MULTISELECT))
#define hasDrawersOnlyFlag(v) (isFlagSet((v), REQF_DRAWERSONLY))

// special Macros for the Busy Handling of the InfoBar.
#define BUSYLEVEL             5
#define BusyEnd()             Busy("", NULL, 0, 0)
#define BusySet(c)            Busy(NULL, NULL, c, 0)
#define BusyText(t, p)        Busy(t, p, 0, 0)
#define BusyGauge(t, p, max)  Busy(t, p, 0, max)

#define OUT_DOS       ((BPTR)0)
#define OUT_NIL       ((BPTR)1)

// attachment requester flags & macros
#define ATTREQ_DISP       (1<<0)
#define ATTREQ_SAVE       (1<<1)
#define ATTREQ_PRINT      (1<<2)
#define ATTREQ_MULTI      (1<<3)
#define isDisplayReq(v)   (isFlagSet((v), ATTREQ_DISP))
#define isSaveReq(v)      (isFlagSet((v), ATTREQ_SAVE))
#define isPrintReq(v)     (isFlagSet((v), ATTREQ_PRINT))
#define isMultiReq(v)     (isFlagSet((v), ATTREQ_MULTI))

#define ARRAY_SIZE(x)     (sizeof(x[0]) ? sizeof(x)/sizeof(x[0]) : 0)

// special flagging macros
#define isFlagSet(v,f)      (((v) & (f)) == (f))  // return TRUE if the flag is set
#define hasFlag(v,f)        (((v) & (f)) != 0)    // return TRUE if one of the flags in f is set in v
#define isFlagClear(v,f)    (((v) & (f)) == 0)    // return TRUE if flag f is not set in v
#define SET_FLAG(v,f)       ((v) |= (f))          // set the flag f in v
#define CLEAR_FLAG(v,f)     ((v) &= ~(f))         // clear the flag f in v
#define MASK_FLAG(v,f)      ((v) &= (f))          // mask the variable v with flag f bitwise

// some filetype handling macros
#define isFile(etype)     (etype < 0)
#define isDrawer(etype)   (etype >= 0 && etype != ST_SOFTLINK && etype != ST_LINKDIR)

/* ReturnID collecting macros
** every COLLECT_ have to be finished with a REISSUE_
**
** Example:
**
** COLLECT_RETURNIDS;
**
** while(running)
** {
**    ULONG signals;
**    switch (DoMethod(G->App, MUIM_Application_Input, &signals))
**    {
**        case ID_PLAY:
**           PlaySound();
**           break;
**
**        case ID_CANCEL:
**        case MUIV_Application_ReturnID_Quit:
**           running = FALSE;
**           break;
**    }
**
**    if (running && signals) Wait(signals);
** }
**
** REISSUE_RETURNIDS;
*/
#define COLLECT_SIZE 32
#define COLLECT_RETURNIDS { \
                            ULONG returnID[COLLECT_SIZE], csize = COLLECT_SIZE, rpos = COLLECT_SIZE, userData, userSigs = 0; \
                            while(csize && userSigs == 0 && (userData = DoMethod(G->App, MUIM_Application_NewInput, &userSigs))) \
                              returnID[--csize] = userData

#define REISSUE_RETURNIDS   while(rpos > csize) \
                              DoMethod(G->App, MUIM_Application_ReturnID, returnID[--rpos]); \
                          }

// Wrapper define to be able to use the standard call of MUI_Request
#ifdef MUI_Request
#undef MUI_Request
#endif
#define MUI_Request YAMMUIRequest

// function macros
#define ISpace(ch)            ((BOOL)((ch) == ' ' || ((ch) >= 9 && (ch) <= 13)))
#define BuildAddrName2(p)     BuildAddrName((p)->Address, (p)->RealName)
#define SetHelp(o,str)        set(o, MUIA_ShortHelp, GetStr(str))
#define DisposeModulePush(m)  DoMethod(G->App, MUIM_Application_PushMethod, G->App, 3, MUIM_CallHook, &DisposeModuleHook, m)
#define MyStrCpy(a,b)         strncpy((a),(b), sizeof(a)), (a)[sizeof(a)-1] = 0
#define FreeStrBuf(str)       ((str) ? free(((char *)(str))-sizeof(size_t)) : (void)0)
#define isSpace(c)            ((BOOL)(G->Locale ? (IsSpace(G->Locale, (ULONG)(c)) != 0) : (ISpace((c)) != 0)))
#define isGraph(c)            ((BOOL)(G->Locale ? (IsGraph(G->Locale, (ULONG)(c)) != 0) : (isgraph((c)) != 0)))
#define isAlNum(c)            ((BOOL)(G->Locale ? (IsAlNum(G->Locale, (ULONG)(c)) != 0) : (isalnum((c)) != 0)))
#define isValidMailFile(file) (!(strlen(file) < 17 || file[12] != '.' || file[16] != ',' || !isdigit(file[13])))
#define Bool2Txt(b)           ((b) ? "Y" : "N")
#define Txt2Bool(t)           (BOOL)(toupper((int)*(t)) == 'Y' || (int)*(t) == '1')

#if !defined(IsMinListEmpty)
#define IsMinListEmpty(x)     (((x)->mlh_TailPred) == (struct MinNode *)(x))
#endif

extern int            BusyLevel;
extern struct Hook    GeneralDesHook;
extern struct Hook    DisposeModuleHook;
extern long           PNum;
extern unsigned char  *PPtr[16];

// only prototypes needed for AmigaOS
#if !defined(__MORPHOS__)
Object * STDARGS VARARGS68K DoSuperNew(struct IClass *cl, Object *obj, ...);
#endif

// all the utility prototypes
struct Mail *AddMailToList(struct Mail *mail, struct Folder *folder);
APTR     AllocCopy(APTR source, int size);
char *   AllocReqText(char *s);
char *   AllocStrBuf(size_t initlen);
void     AppendLog(int id, char *text, void *a1, void *a2, void *a3, void *a4);
void     AppendLogNormal(int id, char *text, void *a1, void *a2, void *a3, void *a4);
void     AppendLogVerbose(int id, char *text, void *a1, void *a2, void *a3, void *a4);
struct Part *AttachRequest(char *title, char *body, char *yestext, char *notext, int mode, struct ReadMailData *rmData);
char *   BuildAddrName(char *address, char *name);
void     Busy(char *text, char *parameter, int cur, int max);
BOOL     CheckPrinter(void);
void     ClearMailList(struct Folder *folder, BOOL resetstats);
void     CloseTempFile(struct TempFile *tf);
ULONG    CRC32(void *buffer, unsigned int count, ULONG crc);
ULONG    CompressMsgID(char *msgid);
BOOL     ConvertCRLF(char *in, char *out, BOOL to);
ULONG    ConvertKey(struct IntuiMessage *imsg);
BOOL     isChildOfGroup(Object *group, Object *child);
BOOL     CopyFile(char *dest, FILE *destfh, char *sour, FILE *sourfh);
BOOL     MoveFile(char *oldname, char *newname);
char *   CreateFilename(const char * const file);
BOOL     CreateDirectory(char *dir);
long     DateStamp2Long(struct DateStamp *date);
int      TZtoMinutes(char *tzone);
void     DateStampUTC(struct DateStamp *ds);
void     GetSysTimeUTC(struct timeval *tv);
void     TimeValTZConvert(struct timeval *tv, enum TZConvert tzc);
void     DateStampTZConvert(struct DateStamp *ds, enum TZConvert tzc);
void     TimeVal2DateStamp(const struct timeval *tv, struct DateStamp *ds, enum TZConvert tzc);
void     DateStamp2TimeVal(const struct DateStamp *ds, struct timeval *tv, enum TZConvert tzc);
BOOL     TimeVal2String(char *dst, const struct timeval *tv, enum DateStampType mode, enum TZConvert tzc);
BOOL     DateStamp2String(char *dst, struct DateStamp *date, enum DateStampType mode, enum TZConvert tzc);
BOOL     DateStamp2RFCString(char *dst, struct DateStamp *date, int timeZone, BOOL convert);
char *   Decrypt(char *source);
BOOL     DeleteMailDir(char *dir, BOOL isroot);
char *   DescribeCT(char *ct);
void     DisplayMailList(struct Folder *fo, APTR lv);
void     DisplayAppIconStatistics(void);
void     DisplayStatistics(struct Folder *fo, BOOL updateAppIcon);
void     DisposeModule(void *modptr);
BOOL     DoPack(char *file, char *newfile, struct Folder *folder);
BOOL     DumpClipboard(FILE *out);
BOOL     EditorToFile(Object *editor, char *file, struct TranslationTable *tt);
char *   Encrypt(char *source);
char *   GetRealPath(char *path);
BOOL     ExecuteCommand(char *cmd, BOOL asynch, BPTR outdef);
char *   ExpandText(char *src, struct ExpandTextData *etd);
void     ExtractAddress(char *line, struct Person *pe);
BOOL     FileExists(char *filename);
int      FileSize(char *filename);
long     FileProtection(const char *filename);
BOOL     FileToEditor(char *file, Object *editor);
int      FileType(char *filename);
char *   FileComment(char *filename);
struct DateStamp *FileDate(char *filename);
long     FileTime(const char *filename);
long     FileCount(char *directory);
void     FinishUnpack(char *file);
struct Folder *FolderRequest(char *title, char *body, char *yestext, char *notext,
         struct Folder *exclude, APTR parent);
void     FormatSize(LONG size, char *buffer);
void     FreeBCImage(struct BodyChunkData *bcd);
struct BodyChunkData *GetBCImage(char *fname);
time_t   GetDateStamp(void);
char *   GetFolderDir(struct Folder *fo);
char *   GetLine(FILE *fh, char *buffer, int bufsize);
char *   GetMailFile(char *string, struct Folder *folder, struct Mail *mail);
struct MailInfo *GetMailInfo(struct Mail *smail);
BOOL     GetMUICheck(Object *obj);
int      GetMUICycle(Object *obj);
int      GetMUIInteger(Object *obj);
int      GetMUINumer(Object *obj);
int      GetMUIRadio(Object *obj);
void     GetMUIString(char *a, Object *obj);
void     GetMUIText(char *a, Object *obj);
char *   GetNextLine(char *p1);
struct Person *GetReturnAddress(struct Mail *mail);
int      GetSimpleID(void);
void     GotoURL(char *url);
char *   IdentifyFile(char *fname);
void     InfoWindow(char *title, char *body, char *oktext, APTR parent);
void     InsertAddresses(APTR obj, char **addr, BOOL add);
char *   itoa(int val);
struct BodyChunkData *LoadBCImage(char *fname);
void     LoadLayout(void);
BOOL     LoadTranslationTable(struct TranslationTable **tt, char *file);
BOOL     MailExists(struct Mail *mailptr, struct Folder *folder);
Object * MakeButton(char *txt);
Object * MakeCheck(char *label);
Object * MakeCheckGroup(Object **check, char *label);
Object * MakeCycle(char **labels, char *label);
Object * MakeInteger(int maxlen, char *label);
Object * MakeMenuitem(const UBYTE *str, ULONG ud);
Object * MakeNumeric(int min, int max, BOOL percent);
Object * MakePassString(char *label);
Object * MakePGPKeyList(Object **st, BOOL secret, char *label);
Object * MakeStatusFlag(char *fname);
Object * MakeFolderImage(char *fname);
Object * MakeString(int maxlen, char *label);
Object * MakeAddressField(Object **string, char *label, APTR help, int abmode, int winnum, BOOL allowmulti);
BOOL     MatchNoCase(const char *string, const char *match);
void     MyAddTail(struct Mail **list, struct Mail *new);
char *   MyStrChr(const char *s, int c);
struct TempFile *OpenTempFile(char *mode);
BOOL     AllFolderLoaded(void);
BOOL     PFExists(char *path, char *file);
void     PGPClearPassPhrase(BOOL force);
int      PGPCommand(char *progname, char *options, int flags);
void     PGPGetPassPhrase(void);
void     PlaySound(char *filename);
void     Quote_Text(FILE *out, char *src, int len, int line_max, char *prefix);
void     RemoveMailFromList(struct Mail *mail);
BOOL     RenameFile(char *oldname, char *newname);
BOOL     RepackMailFile(struct Mail *mail, enum FolderMode dstMode, char *passwd);
int      ReqFile(enum ReqFileType num, Object *win, char *title, int mode, char *drawer, char *file);
BOOL     SafeOpenWindow(Object *obj);
void     SaveLayout(BOOL permanent);
int      SelectMessage(struct Mail *mail);
void     SetupToolbar(struct MUIP_Toolbar_Description *tb, char *label, char *help, ULONG flags);
char     ShortCut(char *label);
void     SimpleWordWrap(char *filename, int wrapsize);
void STDARGS VARARGS68K SPrintF(char *outstr, char *fmtstr, ...);
char *   StartUnpack(char *file, char *newfile, struct Folder *folder);
char *   StrBufCat(char *strbuf, const char *source);
char *   StrBufCpy(char *strbuf, const char *source);
char *   AppendToBuffer(char *buf, int *wptr, int *len, char *add);
int      StringRequest(char *string, int size, char *title, char *body,
                       char *yestext, char *alttext, char *notext, BOOL secret, APTR parent);
char *   StripUnderscore(char *label);
char *   stristr(const char *a, const char *b);
char *   StrTok_R(char **s, char *sep);
char *   SWSSearch(char *str1, char*str2);
int      TransferMailFile(BOOL copyit, struct Mail *mail, struct Folder *dstfolder);
char *   Trim(char *s);
char *   TrimEnd(char *s);
char *   TrimStart(char *s);
BOOL     LoadParsers(void);
void     SParse(char *);
LONG STDARGS YAMMUIRequest(APTR app, APTR win, LONG flags, char *title, char *gadgets, char *format, ...);
APTR     WhichLV(struct Folder *folder);

// Here we define inline functions that should be inlined by
// the compiler, if possible.

/// xget()
//  Gets an attribute value from a MUI object
INLINE ULONG xget(Object *obj, const ULONG attr)
{
  ULONG b = 0;

  // please note that we do not evaluate the return value of GetAttr()
  // as some attributes (e.g. MUIA_Selected) always return FALSE, even
  // when they are supported by the object. But setting b=0 right before
  // the GetAttr() should catch the case when attr doesn't exist at all
  GetAttr(attr, obj, &b);

  return b;
}
///

#endif /* YAM_UTILITIES_H */
