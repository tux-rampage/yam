#ifndef YAM_CONFIG_H
#define YAM_CONFIG_H

/***************************************************************************

 YAM - Yet Another Mailer
 Copyright (C) 1995-2000 by Marcel Beck <mbeck@yam.ch>
 Copyright (C) 2000-2006 by YAM Open Source Team

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

#include <libraries/mui.h>

#include "YAM_find.h"
#include "YAM_transfer.h"

#define FOCOLNUM 5
#define MACOLNUM 8  // the maximum number of columns the MessageListview can have
#define ABCOLNUM 9

struct CO_GUIData
{
   Object *WI;
   Object *BT_SAVE;
   Object *BT_USE;
   Object *BT_CANCEL;
   Object *MI_IMPMIME;
   Object *NLV_PAGE;
   Object *LV_PAGE;
   Object *GR_PAGE;
   Object *GR_SUBPAGE;
   Object *ST_REALNAME;
   Object *ST_EMAIL;
   Object *ST_POPHOST0;
   Object *ST_PASSWD0;
   Object *CY_TZONE;
   Object *CH_DLSAVING;
   Object *ST_SMTPHOST;
   Object *ST_SMTPPORT;
   Object *ST_DOMAIN;
   Object *CH_SMTP8BIT;
   Object *CH_USESMTPAUTH;
   Object *ST_SMTPAUTHUSER;
   Object *ST_SMTPAUTHPASS;
   Object *LV_POP3;
   Object *GR_POP3;
   Object *BT_PADD;
   Object *BT_PDEL;
   Object *ST_POPHOST;
   Object *ST_POPPORT;
   Object *ST_POPUSERID;
   Object *ST_PASSWD;
   Object *CH_DELETE;
   Object *CH_USEAPOP;
   Object *CH_POP3SSL;
   Object *CH_USESTLS;
   Object *CH_POPENABLED;
   Object *CH_AVOIDDUP;
   Object *CY_MSGSELECT;
   Object *CY_TRANSWIN;
   Object *CH_UPDSTAT;
   Object *ST_WARNSIZE;
   Object *NM_INTERVAL;
   Object *CH_DLLARGE;
   Object *CH_NOTIREQ;
   Object *CH_NOTISOUND;
   Object *CH_NOTICMD;
   Object *ST_NOTISOUND;
   Object *ST_NOTICMD;
   Object *LV_RULES;
   Object *BT_RADD;
   Object *BT_RDEL;
   Object *ST_RNAME;
   Object *CH_REMOTE;
   Object *GR_RGROUP;
   Object *GR_SGROUP;
   Object *PO_MOVETO;
   Object *TX_MOVETO;
   Object *LV_MOVETO;
   Object *CH_APPLYNEW;
   Object *CH_APPLYREQ;
   Object *CH_APPLYSENT;
   Object *CH_ABOUNCE;
   Object *ST_ABOUNCE;
   Object *CH_AFORWARD;
   Object *ST_AFORWARD;
   Object *CH_ARESPONSE;
   Object *PO_ARESPONSE;
   Object *ST_ARESPONSE;
   Object *CH_AEXECUTE;
   Object *PO_AEXECUTE;
   Object *ST_AEXECUTE;
   Object *CH_APLAY;
   Object *PO_APLAY;
   Object *ST_APLAY;
   Object *BT_APLAY;
   Object *CH_AMOVE;
   Object *CH_ADELETE;
   Object *CH_ASKIP;
   Object *CY_HEADER;
   Object *ST_HEADERS;
   Object *CY_SENDERINFO;
   Object *CY_SIGSEPLINE;
   Object *CA_COLTEXT;
   Object *CA_COL1QUOT;
   Object *CA_COL2QUOT;
   Object *CA_COL3QUOT;
   Object *CA_COL4QUOT;
   Object *CA_COLURL;
   Object *CH_FIXFEDIT;
   Object *CH_ALLTEXTS;
   Object *CH_MULTIWIN;
   Object *CH_WRAPHEAD;
   Object *CH_TEXTSTYLES;
   Object *ST_REPLYTO;
   Object *ST_ORGAN;
   Object *ST_EXTHEADER;
   Object *ST_HELLOTEXT;
   Object *ST_BYETEXT;
   Object *ST_EDWRAP;
   Object *CY_EDWRAP;
   Object *ST_EDITOR;
   Object *CH_LAUNCH;
   Object *ST_REPLYHI;
   Object *ST_REPLYTEXT;
   Object *ST_REPLYBYE;
   Object *ST_AREPLYHI;
   Object *ST_AREPLYTEXT;
   Object *ST_AREPLYBYE;
   Object *ST_AREPLYPAT;
   Object *ST_MREPLYHI;
   Object *ST_MREPLYTEXT;
   Object *ST_MREPLYBYE;
   Object *CH_QUOTE;
   Object *ST_REPLYCHAR;
   Object *ST_ALTQUOTECHAR;
   Object *CH_QUOTEEMPTY;
   Object *CH_COMPADDR;
   Object *CH_STRIPSIG;
   Object *ST_FWDSTART;
   Object *ST_FWDEND;
   Object *CH_USESIG;
   Object *CY_SIGNAT;
   Object *BT_SIGEDIT;
   Object *TE_SIGEDIT;
   Object *BT_INSTAG;
   Object *BT_INSENV;
   Object *ST_TAGFILE;
   Object *ST_TAGSEP;
   Object *CH_FCOLS[FOCOLNUM];
   Object *CH_MCOLS[MACOLNUM];
   Object *CH_FIXFLIST;
   Object *CH_BEAT;
   Object *CY_SIZE;
   Object *ST_PGPCMD;
   Object *ST_MYPGPID;
   Object *CH_ENCSELF;
   Object *ST_REMAILER;
   Object *ST_FIRSTLINE;
   Object *ST_LOGFILE;
   Object *CY_LOGMODE;
   Object *CH_SPLITLOG;
   Object *CH_LOGALL;
   Object *CH_POPSTART;
   Object *CH_SENDSTART;
   Object *CH_DELETESTART;
   Object *CH_REMOVESTART;
   Object *CH_LOADALL;
   Object *CH_MARKNEW;
   Object *CH_CHECKBD;
   Object *CH_SENDQUIT;
   Object *CH_DELETEQUIT;
   Object *CH_REMOVEQUIT;
   Object *LV_MIME;
   Object *GR_MIME;
   Object *ST_CTYPE;
   Object *ST_EXTENS;
   Object *ST_COMMAND;
   Object *ST_DEFVIEWER;
   Object *BT_MADD;
   Object *BT_MDEL;
   Object *CH_IDENTBIN;
   Object *ST_DETACHDIR;
   Object *ST_ATTACHDIR;
   Object *ST_GALLDIR;
   Object *ST_PROXY;
   Object *ST_PHOTOURL;
   Object *CH_ADDINFO;
   Object *CY_ATAB;
   Object *ST_NEWGROUP;
   Object *CH_ACOLS[ABCOLNUM];
   Object *LV_REXX;
   Object *ST_RXNAME;
   Object *ST_SCRIPT;
   Object *CY_ISADOS;
   Object *CH_CONSOLE;
   Object *CH_WAITTERM;
   Object *ST_TEMPDIR;
   Object *ST_APPX;
   Object *ST_APPY;
   Object *CH_CLGADGET;
   Object *CH_CONFIRM;
   Object *NB_CONFIRMDEL;
   Object *CH_REMOVE;
   Object *CH_SAVESENT;
   Object *RA_MDN_DISP;
   Object *RA_MDN_PROC;
   Object *RA_MDN_DELE;
   Object *RA_MDN_RULE;
   Object *CH_SEND_MDN;
   Object *TX_PACKER;
   Object *TX_ENCPACK;
   Object *NB_PACKER;
   Object *NB_ENCPACK;
   Object *ST_ARCHIVER;
   Object *ST_APPICON;
   Object *CH_FCNTMENU;
   Object *CH_MCNTMENU;
   Object *CY_INFOBAR;
   Object *ST_INFOBARTXT;
   Object *CH_WARNSUBJECT;
   Object *NB_EMAILCACHE;
   Object *NB_AUTOSAVE;
   Object *CH_AUTOTRANSLATEIN;
   Object *RA_SMTPSECURE;
   Object *CH_EMBEDDEDREADPANE;
   Object *CH_DELAYEDSTATUS;
   Object *NB_DELAYEDSTATUS;
   Object *BT_FILTERUP;
   Object *BT_FILTERDOWN;
   Object *BT_MORE;
   Object *BT_LESS;
   Object *CH_QUICKSEARCHBAR;
   Object *CH_WBAPPICON;
   Object *CH_DOCKYICON;
   Object *ST_DEFAULTCHARSET;
};

struct CO_ClassData  /* configuration window */
{
   struct CO_GUIData GUI;
   int  VisiblePage;
   int  LastSig;
   BOOL Visited[MAXCPAGES];
   BOOL UpdateAll;
};

#define P3SSL_OFF    0
#define P3SSL_SSL    1
#define P3SSL_STLS   2

struct POP3
{
   char  Account[SIZE_USERID+SIZE_HOST];
   char  Server[SIZE_HOST];
   int   Port;
   char  User[SIZE_USERID];
   char  Password[SIZE_USERID];
   BOOL  Enabled;
   int   SSLMode;
   BOOL  UseAPOP;
   BOOL  DeleteOnServer;
};

struct MimeView
{
   char  ContentType[SIZE_CTYPE];
   char  Command[SIZE_COMMAND];
   char  Extension[SIZE_NAME];
};

/*** RxHook structure ***/
struct RxHook
{
   BOOL  IsAmigaDOS;
   BOOL  UseConsole;
   BOOL  WaitTerm;
   char  Name[SIZE_NAME];
   char  Script[SIZE_PATHFILE];
};

// flags for hiding GUI elements
#define HIDE_INFO    1
#define HIDE_XY      2
#define HIDE_TBAR    4
#define hasHideInfoFlag(f)    (isFlagSet((f), HIDE_INFO))
#define hasHideXYFlag(f)      (isFlagSet((f), HIDE_XY))
#define hasHideToolBarFlag(f) (isFlagSet((f), HIDE_TBAR))

// notify flags for the notifiying method for new messages
#define NOTIFY_REQ     1
#define NOTIFY_SOUND   2
#define NOTIFY_CMD     4
#define hasRequesterNotify(f) (isFlagSet((f), NOTIFY_REQ))
#define hasSoundNotify(f)     (isFlagSet((f), NOTIFY_SOUND))
#define hasCommandNotify(f)   (isFlagSet((f), NOTIFY_CMD))

enum PrintMethod { PRINTMETHOD_RAW };

/*** Configuration main structure ***/
struct Config
{
   struct POP3 *    P3[MAXP3];
   struct MimeView *MV[MAXMV];

   struct MinList filterList; // list of currently available filter node

   int   TimeZone;
   int   PreSelection;
   int   TransferWindow;
   int   WarnSize;
   int   CheckMailDelay;
   int   NotifyType;
   int   ShowHeader;
   int   ShowSenderInfo;
   int   SigSepLine;
   int   EdWrapCol;
   int   EdWrapMode;
   int   FolderCols;
   int   MessageCols;
   int   LogfileMode;
   int   AddToAddrbook;
   int   AddrbookCols;
   int   IconPositionX;
   int   IconPositionY;
   int   ConfirmDelete;
   int   MDN_Display;
   int   MDN_Process;
   int   MDN_Delete;
   int   MDN_Filter;
   int   XPKPackEff;
   int   XPKPackEncryptEff;
   int   LetterPart;
   int   WriteIndexes;
   int   AutoSave;
   int   HideGUIElements;
   int   StackSize;
   int   SizeFormat;
   int   InfoBar;
   int   EmailCache;
   int   SMTP_Port;
   int   TRBufferSize;
   int   EmbeddedMailDelay;
   int   StatusChangeDelay;
   int   KeepAliveInterval;

   enum  PrintMethod   PrintMethod;
   enum  SMTPSecMethod SMTP_SecureMethod;

   BOOL  DaylightSaving;
   BOOL  Allow8bit;
   BOOL  Use_SMTP_AUTH;
   BOOL  AvoidDuplicates;
   BOOL  UpdateStatus;
   BOOL  DownloadLarge;
   BOOL  DisplayAllTexts;
   BOOL  FixedFontEdit;
   BOOL  MultipleWindows;
   BOOL  UseTextstyles;
   BOOL  WrapHeader;
   BOOL  LaunchAlways;
   BOOL  QuoteMessage;
   BOOL  QuoteEmptyLines;
   BOOL  CompareAddress;
   BOOL  StripSignature;
   BOOL  UseSignature;
   BOOL  FixedFontList;
   BOOL  SwatchBeat;
   BOOL  EncryptToSelf;
   BOOL  SplitLogfile;
   BOOL  LogAllEvents;
   BOOL  GetOnStartup;
   BOOL  SendOnStartup;
   BOOL  CleanupOnStartup;
   BOOL  RemoveOnStartup;
   BOOL  LoadAllFolders;
   BOOL  UpdateNewMail;
   BOOL  CheckBirthdates;
   BOOL  SendOnQuit;
   BOOL  CleanupOnQuit;
   BOOL  RemoveOnQuit;
   BOOL  IdentifyBin;
   BOOL  AddMyInfo;
   BOOL  IconifyOnQuit;
   BOOL  Confirm;
   BOOL  RemoveAtOnce;
   BOOL  SaveSent;
   BOOL  SendMDNAtOnce;
   BOOL  JumpToNewMsg;
   BOOL  JumpToIncoming;
   BOOL  PrinterCheck;
   BOOL  IsOnlineCheck;
   BOOL  ConfirmOnQuit;
   BOOL  AskJumpUnread;
   BOOL  WarnSubject;
   BOOL  FolderCntMenu;
   BOOL  MessageCntMenu;
   BOOL  AutomaticTranslationIn;
   BOOL  AutoColumnResize;
   BOOL  EmbeddedReadPane;
   BOOL  StatusChangeDelayOn;
   BOOL  SysCharsetCheck;
   BOOL  QuickSearchBar;
   BOOL  WBAppIcon;
   BOOL  DockyIcon;

   struct MUI_PenSpec ColoredText;
   struct MUI_PenSpec Color1stLevel;
   struct MUI_PenSpec Color2ndLevel;
   struct MUI_PenSpec Color3rdLevel;
   struct MUI_PenSpec Color4thLevel;
   struct MUI_PenSpec ColorURL;
   struct RxHook      RX[MAXRX];
   struct TRSocketOpt SocketOptions;

   char  RealName[SIZE_REALNAME];
   char  EmailAddress[SIZE_ADDRESS];
   char  SMTP_Server[SIZE_HOST];
   char  SMTP_Domain[SIZE_HOST];
   char  SMTP_AUTH_User[SIZE_USERID];
   char  SMTP_AUTH_Pass[SIZE_USERID];
   char  NotifySound[SIZE_PATHFILE];
   char  NotifyCommand[SIZE_COMMAND];
   char  ShortHeaders[SIZE_PATTERN];
   char  ReplyTo[SIZE_ADDRESS];
   char  Organization[SIZE_DEFAULT];
   char  ExtraHeaders[SIZE_LARGE];
   char  NewIntro[SIZE_INTRO];
   char  Greetings[SIZE_INTRO];
   char  Editor[SIZE_PATHFILE];
   char  ReplyHello[SIZE_INTRO];
   char  ReplyIntro[SIZE_INTRO];
   char  ReplyBye[SIZE_INTRO];
   char  AltReplyHello[SIZE_INTRO];
   char  AltReplyIntro[SIZE_INTRO];
   char  AltReplyBye[SIZE_INTRO];
   char  AltReplyPattern[SIZE_PATTERN];
   char  MLReplyHello[SIZE_INTRO];
   char  MLReplyIntro[SIZE_INTRO];
   char  MLReplyBye[SIZE_INTRO];
   char  ForwardIntro[SIZE_INTRO];
   char  ForwardFinish[SIZE_INTRO];
   char  QuoteText[SIZE_SMALL];
   char  AltQuoteText[SIZE_SMALL];
   char  TagsFile[SIZE_PATHFILE];
   char  TagsSeparator[SIZE_SMALL];
   char  PGPCmdPath[SIZE_PATH];
   char  MyPGPID[SIZE_DEFAULT];
   char  ReMailer[SIZE_ADDRESS];
   char  RMCommands[SIZE_INTRO];
   char  LogfilePath[SIZE_PATH];
   char  DetachDir[SIZE_PATH];
   char  AttachDir[SIZE_PATH];
   char  GalleryDir[SIZE_PATH];
   char  MyPictureURL[SIZE_URL];
   char  NewAddrGroup[SIZE_NAME];
   char  ProxyServer[SIZE_HOST];
   char  TempDir[SIZE_PATH];
   char  PackerCommand[SIZE_COMMAND];
   char  XPKPack[5];
   char  XPKPackEncrypt[5];
   char  SupportSite[SIZE_HOST];
   char  LocalCharset[SIZE_CTYPE+1];
   char  IOCInterface[SIZE_SMALL];
   char  AppIconText[SIZE_COMMAND];
   char  InfoBarText[SIZE_COMMAND];
};

enum SizeFormat { SF_DEFAULT=0, SF_MIXED, SF_1PREC, SF_2PREC, SF_3PREC };
enum InfoBarPos { IB_POS_TOP=0, IB_POS_CENTER, IB_POS_BOTTOM, IB_POS_OFF };

extern struct Config *C;
extern struct Config *CE;

// external hooks
extern struct Hook CO_AddMimeViewHook;
extern struct Hook CO_AddPOP3Hook;
extern struct Hook CO_DelMimeViewHook;
extern struct Hook CO_DelPOP3Hook;
extern struct Hook CO_EditSignatHook;
extern struct Hook CO_SwitchSignatHook;
extern struct Hook CO_GetDefaultPOPHook;
extern struct Hook CO_GetMVEntryHook;
extern struct Hook CO_GetP3EntryHook;
extern struct Hook CO_GetRXEntryHook;
extern struct Hook CO_OpenHook;
extern struct Hook CO_PL_DspFuncHook;
extern struct Hook CO_PutMVEntryHook;
extern struct Hook CO_PutP3EntryHook;
extern struct Hook CO_PutRXEntryHook;
extern struct Hook CO_RemoteToggleHook;
extern struct Hook AddNewFilterToListHook;
extern struct Hook RemoveActiveFilterHook;
extern struct Hook SetActiveFilterDataHook;
extern struct Hook GetActiveFilterDataHook;
extern struct Hook AddNewRuleToListHook;
extern struct Hook RemoveLastRuleHook;

void              CO_FreeConfig(struct Config *co);
BOOL              CO_IsValid(void);
struct MimeView * CO_NewMimeView(void);
struct POP3 *     CO_NewPOP3(struct Config *co, BOOL first);
void              CO_SetDefaults(struct Config *co, int page);
void              CO_Validate(struct Config *co, BOOL update);

void              GhostOutFilter(struct CO_GUIData *gui, struct FilterNode *filter);

#endif /* YAM_CONFIG_H */
