/***************************************************************************

 YAM - Yet Another Mailer
 Copyright (C) 1995-2000 by Marcel Beck <mbeck@yam.ch>
 Copyright (C) 2000-2001 by YAM Open Source Team

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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <libraries/asl.h>
#include <libraries/genesis.h>
#include <libraries/pm.h>
#include <mui/NList_mcc.h>
#include <proto/datatypes.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/genesis.h>
#include <proto/icon.h>
#include <proto/iffparse.h>
#include <proto/intuition.h>
#include <proto/keymap.h>
#include <proto/locale.h>
#include <proto/muimaster.h>
#include <proto/pm.h>
#include <proto/rexxsyslib.h>
#include <proto/utility.h>
#include <proto/wb.h>
#include <proto/xpkmaster.h>

#include "extra.h"
#include "NewReadArgs.h"
#include "YAM.h"
#include "YAM_addressbook.h"
#include "YAM_classes.h"
#include "YAM_config.h"
#include "YAM_configFile.h"
#include "YAM_debug.h"
#include "YAM_folderconfig.h"
#include "YAM_global.h"
#include "YAM_hook.h"
#include "YAM_locale.h"
#include "YAM_main.h"
#include "YAM_mainFolder.h"
#include "YAM_read.h"
#include "YAM_rexx.h"
#include "YAM_write.h"
#include "classes/Classes.h"

/***************************************************************************
 Module: Root
***************************************************************************/

struct Global *G;

static struct NewRDArgs nrda;
static BPTR olddirlock = -1; /* -1 is an unset indicator */

struct Args {
   char  *user;
   char  *password;
   char  *maildir;
   char  *prefsfile;
   LONG   nocheck;
   LONG   hide;
   LONG   debug;
   char  *mailto;
   char  *subject;
   char  *letter;
   char **attach;
};

/**************************************************************************/

// Timer Class
struct TC_Data
{
   struct MsgPort     *port;
   struct timerequest *req;
};

static struct TC_Data TCData = { NULL,NULL };

/// TC_Start
//  Start a one second delay
static void TC_Start(void)
{
   TCData.req->tr_node.io_Command = TR_ADDREQUEST;
   TCData.req->tr_time.tv_secs    = 1;
   TCData.req->tr_time.tv_micro   = 0;
   SendIO(&TCData.req->tr_node);
}

///
/// TC_Exit
//  Frees timer resources
static void TC_Exit(void)
{
   if (TCData.port)
   {
      if (TCData.req)
      {
         if (CheckIO(&TCData.req->tr_node)) return;
         AbortIO(&TCData.req->tr_node);
         WaitIO(&TCData.req->tr_node);
         CloseDevice(&TCData.req->tr_node);
         DeleteIORequest(&TCData.req->tr_node);
      }
      DeleteMsgPort(TCData.port);
   }
   TCData.port = NULL;
   TCData.req = NULL;
}

///
/// TC_Init
//  Initializes timer resources
static BOOL TC_Init(void)
{
   if ((TCData.port = CreateMsgPort()))
      if ((TCData.req = (struct timerequest *)CreateIORequest(TCData.port, sizeof(struct timerequest))))
         if (!OpenDevice(TIMERNAME, UNIT_VBLANK, &TCData.req->tr_node, 0))
            return TRUE;
   return FALSE;
}

///
/// TC_ActiveEditor
//  Returns TRUE if the internal editor is currently being used
static BOOL TC_ActiveEditor(int wrwin)
{
   if (G->WR[wrwin])
   {
      APTR ao;
      get(G->WR[wrwin]->GUI.WI, MUIA_Window_ActiveObject, &ao);
      return (BOOL)(ao==G->WR[wrwin]->GUI.TE_EDIT);
   }
   return FALSE;
}

///
/// TC_Dispatcher
//  Dispatcher for timer class (called once every second)
static void TC_Dispatcher(void)
{
   if (CheckIO(&TCData.req->tr_node))
   {
      int i;
      WaitIO(&TCData.req->tr_node);
      if (++G->SI_Count >= C->WriteIndexes && C->WriteIndexes)
         if (!TC_ActiveEditor(0) && !TC_ActiveEditor(1))
         {
            MA_UpdateIndexes(FALSE);
            G->SI_Count = 0;
         }
      if (++G->GM_Count >= C->CheckMailDelay*60 && C->CheckMailDelay)
      {
         for (i = 0; i < MAXWR; i++) if (G->WR[i]) break;
         if (i == MAXWR && !G->CO)
         {
            MA_PopNow(POP_TIMED,-1);
            G->GM_Count = 0;
         }
      }
      for (i = 0; i < MAXWR; i++) if (G->WR[i]) if (++G->WR[i]->AS_Count >= C->AutoSave && C->AutoSave)
      {
         EditorToFile(G->WR[i]->GUI.TE_EDIT, WR_AutoSaveFile(i), NULL);
         G->WR[i]->AS_Count = 0;
         G->WR[i]->AS_Done = TRUE;
      }
      TC_Start();
   }
}
///
/// AY_PrintStatus
//  Shows progress of program initialization
static void AY_PrintStatus(char *txt, int percent)
{
   set(G->AY_Text, MUIA_Gauge_InfoText, txt);
   set(G->AY_Text, MUIA_Gauge_Current, percent);
   DoMethod(G->App, MUIM_Application_InputBuffered);
}
///
/// AY_SendMailFunc
//  User clicked e-mail URL in About window
HOOKPROTONHNONP(AY_SendMailFunc, void)
{
   int wrwin;
   if (G->MA) if ((wrwin = MA_NewNew(NULL, 0)) >= 0)
   {
      setstring(G->WR[wrwin]->GUI.ST_TO, "YAM Support <support@yam.ch>");
      set(G->WR[wrwin]->GUI.WI, MUIA_Window_ActiveObject, G->WR[wrwin]->GUI.ST_SUBJECT);
   }
}
MakeStaticHook(AY_SendMailHook, AY_SendMailFunc);
///
/// AY_GoPageFunc
//  User clicked homepage URL in About window
HOOKPROTONHNONP(AY_GoPageFunc, void)
{
   GotoURL("http://www.yam.ch/");
}
MakeStaticHook(AY_GoPageHook, AY_GoPageFunc);
///
/// AY_New
//  Creates About window
static BOOL AY_New(BOOL hidden)
{
   char logopath[SIZE_PATHFILE];
   APTR ft_text, bt_sendmail, bt_gopage;
   struct DateTime dt;
   char datebuf[LEN_DATSTRING];

   dt.dat_Stamp.ds_Days   = yamversiondays;
   dt.dat_Stamp.ds_Minute = 0;
   dt.dat_Stamp.ds_Tick   = 0;
   dt.dat_Format  = FORMAT_DOS;
   dt.dat_Flags   = 0L;
   dt.dat_StrDay  = NULL;
   dt.dat_StrDate = datebuf;
   dt.dat_StrTime = NULL;
   DateToStr(&dt);

   strmfp(logopath, G->ProgDir, "Icons/logo");
   G->AY_Win = WindowObject,
      MUIA_Window_Title, GetStr(MSG_MA_About),
      MUIA_Window_ID, MAKE_ID('C','O','P','Y'),
      MUIA_Window_Activate, FALSE,
      MUIA_HelpNode, "COPY",
      WindowContents, VGroup,
         MUIA_Background, MUII_GroupBack,
         Child, HGroup,
            MUIA_Group_Spacing, 0,
            Child, HSpace(0),
            Child, NewObject(CL_BodyChunk->mcc_Class,NULL,
               MUIA_Bodychunk_File, logopath,
            End,
            Child, HSpace(0),
         End,
         Child, HCenter((VGroup,
            Child, CLabel(GetStr(MSG_Copyright1)),
            Child, ColGroup(2),
               Child, bt_sendmail = TextObject,
                  MUIA_Text_Contents, "\033c\033u\0335support@yam.ch",
                  MUIA_InputMode, MUIV_InputMode_RelVerify,
               End,
               Child, bt_gopage = TextObject,
                  MUIA_Text_Contents, "\033c\033u\0335http://www.yam.ch/",
                  MUIA_InputMode, MUIV_InputMode_RelVerify,
               End,
            End,
            Child, RectangleObject,
               MUIA_Rectangle_HBar, TRUE,
               MUIA_FixHeight, 8,
            End,
            Child, ColGroup(2),
               MUIA_Group_HorizSpacing, 8,
               MUIA_Group_VertSpacing, 2,
               Child, Label(GetStr(MSG_Version)),
               Child, LLabel(yamversion),
               Child, Label(GetStr(MSG_CompilationDate)),
               Child, LLabel(datebuf),
             End,
         End)),
         Child, G->AY_Group = PageGroup,
            Child, ListviewObject,
               MUIA_Listview_Input, FALSE,
               MUIA_Listview_List, ft_text = FloattextObject, ReadListFrame, End,
            End,
            Child, ScrollgroupObject,
               MUIA_Scrollgroup_FreeHoriz, FALSE,
               MUIA_Scrollgroup_Contents, VGroupV,
                  GroupFrame,
                  Child, G->AY_List = VGroup,
                     Child, TextObject,
                        MUIA_Text_Contents, GetStr(MSG_UserLogin),
                        MUIA_Background,    MUII_TextBack,
                        MUIA_Frame,         MUIV_Frame_Text,
                        MUIA_Text_PreParse, MUIX_C MUIX_PH,
                     End,
                  End,
                  Child, HVSpace,
               End,
            End,
         End,
         Child, G->AY_Text = GaugeObject,
            GaugeFrame,
            MUIA_Gauge_InfoText, " ",
            MUIA_Gauge_Horiz, TRUE,
         End,
      End,
   End;

   /* if the WindowObject could be created */
   if (G->AY_Win)
   {
      /* now we create the about text */
      G->AY_AboutText = AllocStrBuf(SIZE_LARGE);
      G->AY_AboutText = StrBufCat(G->AY_AboutText, GetStr(MSG_Copyright2));
      G->AY_AboutText = StrBufCat(G->AY_AboutText, GetStr(MSG_UsedSoftware));
      G->AY_AboutText = StrBufCat(G->AY_AboutText, "\0338Magic User Interface\0332 (Stefan Stuntz)\n"
                                                   "\0338TextEditor.mcc, BetterString.mcc\0332 (Allan Odgaard)\n"
                                                   "\0338Toolbar.mcc\0332 (Benny Kj�r Nielsen)\n"
                                                   "\0338NListtree.mcc\0332 (Carsten Scholling)\n"
                                                   "\0338NList.mcc, NListview.mcc\0332 (Gilles Masson)\n"
                                                   "\0338XPK\0332 (Urban D. M�ller, Dirk St�cker)\n"
                                                   "\0338popupmenu.library\0332 (Henrik Isaksson)\n\n");
      G->AY_AboutText = StrBufCat(G->AY_AboutText, GetStr(MSG_WebSite));
      set(ft_text, MUIA_Floattext_Text, G->AY_AboutText);

      DoMethod(G->App, OM_ADDMEMBER, G->AY_Win);
      DoMethod(bt_sendmail, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 2, MUIM_CallHook, &AY_SendMailHook);
      DoMethod(bt_gopage  , MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Application, 2, MUIM_CallHook, &AY_GoPageHook);
      DoMethod(G->AY_Win  , MUIM_Notify, MUIA_Window_CloseRequest, TRUE, G->AY_Win, 3, MUIM_Set,MUIA_Window_Open, FALSE);

      set(G->AY_Win, MUIA_Window_Open, !hidden);

      return TRUE;
   }
   return FALSE;
}
///
/// PopUp
//  Un-iconify YAM
void PopUp(void)
{
   int winopen;
   nnset(G->App, MUIA_Application_Iconified,FALSE);
   get(G->MA->GUI.WI, MUIA_Window_Open, &winopen);
   if (!winopen) set(G->MA->GUI.WI, MUIA_Window_Open, TRUE);
   DoMethod(G->MA->GUI.WI, MUIM_Window_ScreenToFront);
}
///
/// DoublestartHook
//  A second copy of YAM was started
HOOKPROTONHNONP(DoublestartFunc, void)
{
   if (G->App && G->MA->GUI.WI) PopUp();
}
MakeStaticHook(DoublestartHook, DoublestartFunc);
///
/// StayInProg
//  Makes sure that the user really wants to quit the program
static BOOL StayInProg(void)
{
   BOOL req = FALSE;
   int i, fq;

   if (G->AB->Modified)
   {
      if (MUI_Request(G->App, G->MA->GUI.WI, 0, NULL, GetStr(MSG_MA_ABookModifiedGad), GetStr(MSG_AB_Modified)))
         CallHookPkt(&AB_SaveABookHook, 0, 0);
   }
   if (G->CO || C->ConfirmOnQuit) req = TRUE;
   for (i = 0; i < 4; i++) if (G->EA[i]) req = TRUE;
   for (i = 0; i < 2; i++) if (G->WR[i]) req = TRUE;
   get(G->App, MUIA_Application_ForceQuit, &fq);
   if (fq) req = FALSE;
   if (!req) return FALSE;
   return (BOOL)!MUI_Request(G->App, G->MA->GUI.WI, 0, GetStr(MSG_MA_ConfirmReq), GetStr(MSG_YesNoReq), GetStr(MSG_QuitYAMReq));
}
///
/// Root_GlobalDispatcher
//  Processes return value of MUI_Application_NewInput
static BOOL Root_GlobalDispatcher(ULONG app_input)
{
   switch (app_input)
   {
      case MUIV_Application_ReturnID_Quit: return (BOOL)!StayInProg();
      case ID_CLOSEALL: if (!C->IconifyOnQuit) return (BOOL)!StayInProg();
                        set(G->App, MUIA_Application_Iconified, TRUE); break;
      case ID_RESTART:  return 2;
      case ID_ICONIFY:  MA_UpdateIndexes(FALSE); break;
   }
   return FALSE;
}
///
/// Root_New
//  Creates MUI application
static BOOL Root_New(BOOL hidden)
{
#define MUIA_Application_UsedClasses 0x8042e9a7
   static const char *classes[] = {
     "TextEditor.mcc", "Toolbar.mcc", "BetterString.mcc", "InfoText.mcc", "NListtree.mcc", "NList.mcc", "NListview.mcc", NULL
   };
   G->App = ApplicationObject,
      MUIA_Application_Author     ,"YAM Open Source Team",
      MUIA_Application_Base       ,"YAM",
      MUIA_Application_Title      ,"YAM",
      MUIA_Application_Version    ,yamversionstring,
      MUIA_Application_Copyright  ,"� 2000-2001 by YAM Open Source Team",
      MUIA_Application_Description,GetStr(MSG_AppDescription),
      MUIA_Application_UseRexx    ,FALSE,
      MUIA_Application_SingleTask ,!getenv("MultipleYAM"),
      MUIA_Application_UsedClasses, classes,
   End;
   if (G->App)
   {
      set(G->App, MUIA_Application_HelpFile, "YAM.guide");
      set(G->App, MUIA_Application_Iconified, hidden);
      DoMethod(G->App, MUIM_Notify, MUIA_Application_DoubleStart, TRUE, MUIV_Notify_Application, 2, MUIM_CallHook, &DoublestartHook);
      DoMethod(G->App, MUIM_Notify, MUIA_Application_Iconified, TRUE, MUIV_Notify_Application, 2, MUIM_Application_ReturnID, ID_ICONIFY);
      if (AY_New(hidden)) return TRUE;
   }
   return FALSE;
}
///

/// Terminate
//  Deallocates used memory and MUI modules and terminates
static void Terminate(void)
{
   int i;

   if (G->CO)
   {
      CO_FreeConfig(CE);
      free(CE);
      DisposeModule(&G->CO);
   }

   for (i = 0; i < MAXEA; i++)
      DisposeModule(&G->EA[i]);

   for (i = 0; i < MAXRE; i++)
   {
      if (G->RE[i])
      {
         RE_CleanupMessage(i);
         DisposeModule(&G->RE[i]);
      }
   }

   for (i = 0; i <=MAXWR; i++)
   {
      if (G->WR[i])
      {
         WR_Cleanup(i);
         DisposeModule(&G->WR[i]);
      }
   }

   if (G->TR)
   {
      TR_Cleanup();
      TR_CloseTCPIP();
      DisposeModule(&G->TR);
   }

   if (G->FO) DisposeModule(&G->FO);
   if (G->FI) DisposeModule(&G->FI);
   if (G->ER) DisposeModule(&G->ER);
   if (G->US) DisposeModule(&G->US);

   if (G->MA)
   {
      MA_UpdateIndexes(FALSE);
      G->Weights[0] = xget(G->MA->GUI.LV_FOLDERS, MUIA_HorizWeight);
      G->Weights[1] = xget(G->MA->GUI.LV_MAILS,   MUIA_HorizWeight);
      SaveLayout(TRUE);
      set(G->MA->GUI.WI, MUIA_Window_Open, FALSE);
   }

   if (G->AB) DisposeModule(&G->AB);
   if (G->MA) DisposeModule(&G->MA);

   if (G->TTin) free(G->TTin);
   if (G->TTout) free(G->TTout);

   for (i = 0; i < MAXASL; i++)
      if (G->ASLReq[i])
         MUI_FreeAslRequest(G->ASLReq[i]);

   for (i = 0; i < MAXWR+1; i++)
      if (G->WR_NRequest[i].nr_stuff.nr_Msg.nr_Port)
         DeleteMsgPort(G->WR_NRequest[i].nr_stuff.nr_Msg.nr_Port);

   if (G->AppIcon) RemoveAppIcon(G->AppIcon);
   if (G->AppPort) DeleteMsgPort(G->AppPort);
   if (G->RexxHost) CloseDownARexxHost(G->RexxHost);

   TC_Exit();
   if (G->AY_AboutText) FreeStrBuf(G->AY_AboutText);

   if (G->HideIcon) FreeDiskObject(G->HideIcon);

   if (G->App) MUI_DisposeObject(G->App);

   for (i = 0; i < MAXICONS; i++)
   {
      if (G->DiskObj[i]) FreeDiskObject(G->DiskObj[i]);
   }
   for (i = 0; i < MAXIMAGES; i++)
   {
      if (G->BImage[i]) FreeBCImage(G->BImage[i]);
   }
   CO_FreeConfig(C);
   YAM_CleanupClasses();
   ExitClasses();

   if (DataTypesBase) CloseLibrary(DataTypesBase);
   if (XpkBase)       CloseLibrary(XpkBase);
   if (PopupMenuBase) CloseLibrary((struct Library *)PopupMenuBase);
   if (MUIMasterBase) CloseLibrary(MUIMasterBase);
   if (RexxSysBase) CloseLibrary((struct Library *)RexxSysBase);
   if (IFFParseBase) CloseLibrary(IFFParseBase);
   if (KeymapBase) CloseLibrary(KeymapBase);
   if (WorkbenchBase) CloseLibrary(WorkbenchBase);
   CloseYAMCatalog();
   if (G->Locale) CloseLocale(G->Locale);
   if (LocaleBase) CloseLibrary((struct Library *)LocaleBase);

   free(C);
   free(G);
}
///
/// Abort
//  Shows error requester, then terminates the program
static void Abort(APTR formatnum, ...)
{
   va_list a;
   static char error[SIZE_DEFAULT];

   va_start(a, formatnum);
   if(formatnum)
   {
      vsprintf(error, GetStr(formatnum), a);

      if(MUIMasterBase)
      {
         MUI_Request(G->App ? G->App : NULL, NULL, 0, GetStr(MSG_ErrorStartup), GetStr(MSG_Quit), error);
      }
      else if(IntuitionBase)
      {
         struct EasyStruct ErrReq = { sizeof (struct EasyStruct), 0, NULL, NULL, NULL };

         ErrReq.es_Title        = (char *)GetStr(MSG_ErrorStartup);
         ErrReq.es_TextFormat   = error;
         ErrReq.es_GadgetFormat = (char *)GetStr(MSG_Quit);

         EasyRequest(NULL, &ErrReq, NULL, error);
      }
      else
        puts(error);
   }
   va_end(a);
   exit(5);
}
///
/// CheckMCC
//  Checks if a certain version of a MCC is available
static BOOL CheckMCC(char *name, int minver, int minrev, BOOL req)
{
   Object *obj;
   BOOL success = FALSE;

   obj = MUI_NewObject(name, TAG_DONE);
   if (obj) {
      ULONG tmp;
      int ver, rev;

      get(obj, MUIA_Version, &tmp);
      ver = (int)tmp;
      get(obj, MUIA_Revision, &tmp);
      rev = (int)tmp;

      if (ver > minver || (ver == minver && rev >= minrev)) {
         success = TRUE;
      }
      MUI_DisposeObject(obj);
   }

   if(!success && req)
      Abort(MSG_ERR_OPENLIB, name, minver, minrev);

   return success;
}
///
/// InitLib
//  Opens a library
static struct Library *InitLib(char *libname, int version, int revision, BOOL required, BOOL close)
{
   struct Library *lib = OpenLibrary(libname, version);
   if(lib && revision)
   {
      if(lib->lib_Version == version && lib->lib_Revision < revision)
      {
         CloseLibrary(lib); lib = NULL;
      }
   }
   if(!lib && required)
      Abort(MSG_ERR_OPENLIB, libname, version, revision);
   if(lib && close)
      CloseLibrary(lib);
   return lib;
}
///
/// SetupAppIcons
//  Sets location of mailbox status icon on workbench screen
void SetupAppIcons(void)
{
   int i;
   for (i = 0; i < 4; i++) if (G->DiskObj[i])
   {
      G->DiskObj[i]->do_CurrentX = C->IconPositionX;
      G->DiskObj[i]->do_CurrentY = C->IconPositionY;
   }
}
///
/// Initialise2
//  Phase 2 of program initialization (after user logs in)
static void Initialise2(BOOL hidden)
{
   BOOL newfolders = FALSE;
   int i;
   struct Folder *folder, **oldfolders = NULL;

   AY_PrintStatus(GetStr(MSG_LoadingConfig), 30);
   CO_SetDefaults(C, -1);
   CO_LoadConfig(C, G->CO_PrefsFile, &oldfolders);
   CO_Validate(C, FALSE);
   AY_PrintStatus(GetStr(MSG_CreatingGUI), 40);
   if(!(G->MA = MA_New()) || !(G->AB = AB_New()))
      Abort(MSG_ErrorMuiApp);
   MA_SetupDynamicMenus();
   CallHookPkt(&MA_ChangeSelectedHook, 0, 0);
   SetupAppIcons();
   LoadLayout();
   set(G->MA->GUI.LV_FOLDERS, MUIA_HorizWeight, G->Weights[0]);
   set(G->MA->GUI.LV_MAILS,   MUIA_HorizWeight, G->Weights[1]);
   AY_PrintStatus(GetStr(MSG_LoadingFolders), 50);
   if (!FO_LoadTree(CreateFilename(".folders")) && oldfolders)
   {
      for (i = 0; i < 100; i++) if (oldfolders[i]) DoMethod(G->MA->GUI.NL_FOLDERS, MUIM_NListtree_Insert, oldfolders[i]->Name, oldfolders[i], MUIV_NListtree_Insert_ListNode_Root, TAG_DONE);
      newfolders = TRUE;
   }
   if (oldfolders) { for (i = 0; oldfolders[i]; i++) free(oldfolders[i]); free(oldfolders); }
   if (!FO_GetFolderByType(FT_INCOMING,NULL)) newfolders |= FO_CreateFolder(FT_INCOMING, FolderNames[0], GetStr(MSG_MA_Incoming));
   if (!FO_GetFolderByType(FT_OUTGOING,NULL)) newfolders |= FO_CreateFolder(FT_OUTGOING, FolderNames[1], GetStr(MSG_MA_Outgoing));
   if (!FO_GetFolderByType(FT_SENT    ,NULL)) newfolders |= FO_CreateFolder(FT_SENT    , FolderNames[2], GetStr(MSG_MA_Sent));
   if (!FO_GetFolderByType(FT_DELETED ,NULL)) newfolders |= FO_CreateFolder(FT_DELETED , FolderNames[3], GetStr(MSG_MA_Deleted));
   if (newfolders) FO_SaveTree(CreateFilename(".folders"));
   AY_PrintStatus(GetStr(MSG_RebuildIndices), 60);
   MA_UpdateIndexes(TRUE);
   AY_PrintStatus(GetStr(MSG_LoadingFolders), 75);
   for (i = 0; ; i++)
   {
      struct MUI_NListtree_TreeNode *tn = (struct MUI_NListtree_TreeNode *)DoMethod(G->MA->GUI.NL_FOLDERS, MUIM_NListtree_GetEntry, MUIV_NListtree_GetEntry_ListNode_Root, i, MUIV_NListtree_GetEntry_Flag_Visible, TAG_DONE);
      if (!tn) break;
      folder = tn->tn_User;
      if (!folder) break;
      if ((folder->Type == FT_INCOMING || folder->Type == FT_OUTGOING || folder->Type == FT_DELETED || C->LoadAllFolders) && !(folder->XPKType&1)) MA_GetIndex(folder);
      else if (folder->Type != FT_GROUP) folder->LoadedMode = MA_LoadIndex(folder, FALSE);
      DoMethod(G->App, MUIM_Application_InputBuffered);
   }
   MA_ChangeFolder(FO_GetFolderByType(FT_INCOMING, NULL), TRUE);
   AY_PrintStatus(GetStr(MSG_LoadingABook), 90);
   AB_LoadTree(G->AB_Filename, FALSE, FALSE);
   if(!(G->RexxHost = SetupARexxHost("YAM", NULL)))
      Abort(MSG_ErrorARexx);
   AY_PrintStatus(GetStr(MSG_OPENGUI), 100);
   set(G->MA->GUI.WI, MUIA_Window_Open, !hidden);
   set(G->AY_Win, MUIA_Window_Open, FALSE);
   set(G->AY_Text, MUIA_ShowMe, FALSE);
}
///
/// Initialise
//  Phase 1 of program initialization (before user logs in)
static void Initialise(BOOL hidden)
{
   static const char *imnames[MAXIMAGES] = {
     "status_unread",   "status_old",    "status_forward",  "status_reply",
     "status_waitsend", "status_error",  "status_hold",     "status_sent",
     "status_new",      "status_delete", "status_download", "status_group",
     "status_urgent",   "status_attach", "status_report",   "status_crypt",
     "status_signed",
     "folder_fold",     "folder_unfold",       "folder_incoming", "folder_incoming_new",
     "folder_outgoing", "folder_outgoing_new", "folder_deleted",  "folder_deleted_new",
     "folder_sent"
   };
   static const char *icnames[MAXICONS] = {
     "empty", "old", "new", "check"
   };
   char iconpath[SIZE_PATH], iconfile[SIZE_PATHFILE];
   int i;

   DateStamp(&G->StartDate);

   /* First open locale.library, so we can display a translated error requester
      in case some of the other libraries can't be opened. */
   if ((LocaleBase = (struct LocaleBase *)InitLib("locale.library", 38, 0, TRUE, FALSE)))
     G->Locale = OpenLocale(NULL);

   // Now load the catalog of YAM
   OpenYAMCatalog();

   WorkbenchBase = InitLib("workbench.library", 36, 0, TRUE, FALSE);
   KeymapBase = InitLib("keymap.library", 36, 0, TRUE, FALSE);
   IFFParseBase = InitLib("iffparse.library", 36, 0, TRUE, FALSE);
   RexxSysBase = (struct RxsLib *)InitLib(RXSNAME, 36, 0, TRUE, FALSE);
   MUIMasterBase = InitLib("muimaster.library", 19, 0, TRUE, FALSE);

   // we open the popupmenu.library for the ContextMenus in YAM but it`s not a MUST.
   PopupMenuBase = (struct PopupMenuBase *)InitLib(POPUPMENU_NAME, 9, 0, FALSE, FALSE);

   /* We can't use CheckMCC() due to a bug in Toolbar.mcc! */
   InitLib("mui/Toolbar.mcc", 15, 6, TRUE, TRUE);

   // we have to have at least v19.98 of NList.mcc to get YAM working without risking
   // to have it buggy - so we make it a requirement.
   CheckMCC(MUIC_NList, 19, 98, TRUE);

   // we make v18.7 the minimum requirement for YAM because earlier versions are
   // buggy
   CheckMCC(MUIC_NListtree, 18, 7, TRUE);

   if(!InitClasses() || !YAM_SetupClasses())
      Abort(MSG_ErrorClasses);
   if(!Root_New(hidden))
      Abort(FindPort("YAM") ? NULL : MSG_ErrorMuiApp);

   AY_PrintStatus(GetStr(MSG_InitLibs), 10);
   XpkBase = InitLib(XPKNAME, 0, 0, FALSE, FALSE);
   if ((DataTypesBase = InitLib("datatypes.library", 39, 0, FALSE, FALSE)))
      if (CheckMCC("Dtpic.mui", 0, 0, FALSE)) G->DtpicSupported = TRUE;
   if (!TC_Init()) Abort(MSG_ErrorTimer);
   for (i = 0; i < MAXASL; i++)
      if (!(G->ASLReq[i] = MUI_AllocAslRequestTags(ASL_FileRequest, ASLFR_RejectIcons, TRUE,
         TAG_END))) Abort(MSG_ErrorAslStruct);
   G->AppPort = CreateMsgPort();
   for (i = 0; i < 3; i++)
   {
      G->WR_NRequest[i].nr_stuff.nr_Msg.nr_Port = CreateMsgPort();
      G->WR_NRequest[i].nr_Name = (UBYTE *)G->WR_Filename[i];
      G->WR_NRequest[i].nr_Flags = NRF_SEND_MESSAGE;
   }
   srand(GetDateStamp());
   AY_PrintStatus(GetStr(MSG_LoadingGFX), 20);
   strmfp(iconfile, G->ProgDir, "YAM");
   if ((G->HideIcon=GetDiskObject(iconfile)))
      set(G->App, MUIA_Application_DiskObject, G->HideIcon);
   strmfp(iconpath, G->ProgDir, "Icons");
   for (i = 0; i < MAXICONS; i++)
   {
      strmfp(iconfile, iconpath, icnames[i]);
      G->DiskObj[i] = GetDiskObject(iconfile);
   }

   // load the standard images now
   for (i = 0; i < MAXIMAGES; i++)
   {
      strmfp(iconfile, iconpath, imnames[i]);
      G->BImage[i] = LoadBCImage(iconfile);
      DoMethod(G->App, MUIM_Application_InputBuffered);
   }
}
///
/// SendWaitingMail
//  Sends pending mail on startup
static void SendWaitingMail(void)
{
   struct Mail *mail;
   BOOL doit = TRUE;
   int tots = 0, hidden;
   struct Folder *fo = FO_GetFolderByType(FT_OUTGOING, NULL);

   if(!fo) return;

   for (mail = fo->Messages; mail; mail = mail->Next)
   {
      if (mail->Status != STATUS_HLD) tots++;
   }
   if (!tots) return;

   MA_ChangeFolder(fo, TRUE);

   get(G->App, MUIA_Application_Iconified, &hidden);
   if (!hidden) doit = MUI_Request(G->App, G->MA->GUI.WI, 0, NULL, GetStr(MSG_YesNoReq), GetStr(MSG_SendStartReq));

   if (doit) MA_Send(SEND_ALL);
}
///
/// DoStartup
//  Performs different checks/cleanup operations on startup
static void DoStartup(BOOL nocheck, BOOL hide)
{
   if (C->CleanupOnStartup) DoMethod(G->App, MUIM_CallHook, &MA_DeleteOldHook);
   if (C->RemoveOnStartup) DoMethod(G->App, MUIM_CallHook, &MA_DeleteDeletedHook);
   if (C->CheckBirthdates && !nocheck && !hide) AB_CheckBirthdates();
   if (TR_IsOnline())
   {
      if (C->GetOnStartup && !nocheck)
      {
         MA_PopNow(POP_START, -1);
         if (G->TR) DisposeModule(&G->TR);
         DoMethod(G->App, MUIM_Application_InputBuffered, TAG_DONE);
      }

      if (C->SendOnStartup && !nocheck)
      {
         SendWaitingMail();
         if (G->TR) DisposeModule(&G->TR);
         DoMethod(G->App, MUIM_Application_InputBuffered, TAG_DONE);
      }
   }
}
///
/// Login
//  Allows automatic login for AmiTCP-Genesis users
static void Login(char *user, char *password, char *maildir, char *prefsfile)
{
   struct genUser *guser;
   BOOL terminate = FALSE, loggedin = FALSE;

   if ((GenesisBase = OpenLibrary("genesis.library", 1)))
   {
      if ((guser = GetGlobalUser()))
      {
         terminate = !(loggedin = US_Login(guser->us_name, "\01", maildir, prefsfile));
         FreeUser(guser);
      }
      CloseLibrary(GenesisBase);
   }
   if (!loggedin && !terminate) terminate = !US_Login(user, password, maildir, prefsfile);
   if(terminate)
     exit(5);
}
///
/// GetDST
//  Checks if daylight saving time is active
static int GetDST(void)
{
   int i;
   char *dst;

   if((dst = getenv("IXGMTOFFSET")))
   {
      return dst[4]?2:1;
   }

   dst = getenv("SUMMERTIME");
   if (!dst) return 0;
   for (i = 0; i < 11; i++)
   {
      while (*dst != ':') if (!*dst++) return 0;
      dst++;
   }
   return (*dst == 'Y' ? 2 : 1);
}
///


/* This makes it possible to leave YAM without explicitely calling cleanup procedure */
static void yam_exitfunc(void)
{
   Terminate();
   if(olddirlock != -1)
      CurrentDir(olddirlock);
   if(nrda.Template)
      NewFreeArgs(&nrda);
   if(UtilityBase)
      CloseLibrary((struct Library *)UtilityBase);
   if(IconBase)
      CloseLibrary(IconBase);
   if(IntuitionBase)
      CloseLibrary((struct Library *) IntuitionBase);
}

/// Main
//  Program entry point, main loop
int main(int argc, char **argv)
{
   struct Args args;
   int wrwin, err, ret;
   char **sptr;
   ULONG signals, appsigs, timsigs, notsigs0, notsigs1, notsigs2, rexsigs;
   struct Message *msg;
   struct User *user;
   BOOL yamFirst = TRUE;
   BPTR progdir;

   atexit(yam_exitfunc); /* we need to free the stuff on exit()! */

   memset(&args, 0, sizeof(struct Args));

   WBmsg = (struct WBStartup *)(0 == argc ? argv : NULL);

   IntuitionBase = (struct IntuitionBase *)InitLib("intuition.library", 36, 0, TRUE, FALSE);
   IconBase = InitLib("icon.library", 36, 0, TRUE, FALSE);
   UtilityBase = (struct UtilityBase *)InitLib("utility.library", 36, 0, TRUE, FALSE);

   nrda.Template = "USER/K,PASSWORD/K,MAILDIR/K,PREFSFILE/K,NOCHECK/S,HIDE/S,DEBUG/S,MAILTO/K,SUBJECT/K,LETTER/K,ATTACH/M";
   nrda.ExtHelp = NULL;
   nrda.Window = NULL;
   nrda.Parameters = (LONG *)&args;
   nrda.FileParameter = -1;
   nrda.PrgToolTypesOnly = FALSE;
   if ((err = NewReadArgs(WBmsg, &nrda)))
   {
      PrintFault(err, "YAM");
      exit(5);
   }

   if(!(progdir = GetProgramDir())) /* security only, can happen for residents only */
      exit(5);
   olddirlock = CurrentDir(progdir);

   for(;;)
   {
      G = calloc(1, sizeof(struct Global));
      C = calloc(1, sizeof(struct Config));
      NameFromLock(progdir, G->ProgDir, sizeof(G->ProgDir));
      if(!args.maildir)
        strcpy(G->MA_MailDir, G->ProgDir);
      args.hide = -args.hide;
      args.nocheck = -args.nocheck;
      G->TR_Debug = -args.debug;
      G->TR_Allow = TRUE;
      G->CO_DST = GetDST();
      if(yamFirst)
      {
         Object *root, *grp, *bt_okay;

         Initialise(args.hide);
         Login(args.user, args.password, args.maildir, args.prefsfile);
         Initialise2(args.hide);

         grp = HGroup,
            Child, RectangleObject, End,
            Child, bt_okay = SimpleButton(GetStr(MSG_ABOUT_OKAY_GAD)),
            Child, RectangleObject, End,
         End;

         get(G->AY_Win, MUIA_Window_RootObject, &root);
         if(root && grp && DoMethod(root, MUIM_Group_InitChange))
         {
            DoMethod(root, OM_ADDMEMBER, grp);
            DoMethod(root, MUIM_Group_ExitChange);
            DoMethod(bt_okay, MUIM_Notify, MUIA_Pressed, FALSE, MUIV_Notify_Window, 3, MUIM_Set, MUIA_Window_Open, FALSE);
            SetAttrs(G->AY_Win,
               MUIA_Window_Activate, TRUE,
               MUIA_Window_DefaultObject, bt_okay,
               TAG_DONE);
         }

         DoMethod(G->App, MUIM_Application_Load, MUIV_Application_Load_ENVARC);
         AppendLog(0, GetStr(MSG_LOG_Started), "", "", "", "");
         MA_StartMacro(MACRO_STARTUP, NULL);
         DoStartup(args.nocheck, args.hide);
         if (args.mailto || args.letter || args.subject || args.attach) if ((wrwin = MA_NewNew(NULL, 0)) >= 0)
         {
            if (args.mailto) setstring(G->WR[wrwin]->GUI.ST_TO, args.mailto);
            if (args.subject) setstring(G->WR[wrwin]->GUI.ST_SUBJECT, args.subject);
            if (args.letter) FileToEditor(args.letter, G->WR[wrwin]->GUI.TE_EDIT);
            if (args.attach) for (sptr = args.attach; *sptr; sptr++)
               if (FileSize(*sptr) >= 0) WR_AddFileToList(wrwin, *sptr, NULL, FALSE);
         }
         yamFirst = FALSE;
      }
      else
      {
         Initialise(FALSE);
         Login(NULL, NULL, NULL, NULL);
         Initialise2(FALSE);
         DoMethod(G->App, MUIM_Application_Load, MUIV_Application_Load_ENVARC);
      }
      user = US_GetCurrentUser();
      AppendLogNormal(1, GetStr(MSG_LOG_LoggedIn), user->Name, "", "", "");
      AppendLogVerbose(1, GetStr(MSG_LOG_LoggedInVerbose), user->Name, G->CO_PrefsFile, G->MA_MailDir, "");
      TC_Start();
      appsigs  = 1<<G->AppPort->mp_SigBit;
      timsigs  = 1<<TCData.port->mp_SigBit;
      notsigs0 = 1<<G->WR_NRequest[0].nr_stuff.nr_Msg.nr_Port->mp_SigBit;
      notsigs1 = 1<<G->WR_NRequest[1].nr_stuff.nr_Msg.nr_Port->mp_SigBit;
      notsigs2 = 1<<G->WR_NRequest[2].nr_stuff.nr_Msg.nr_Port->mp_SigBit;
      rexsigs  = 1<<G->RexxHost->port->mp_SigBit;
      while (!(ret = Root_GlobalDispatcher(DoMethod(G->App, MUIM_Application_NewInput, &signals))))
      {
         if (signals)
         {
            signals = Wait(signals | timsigs | SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | SIGBREAKF_CTRL_F | appsigs | notsigs0 | notsigs1 | rexsigs);
            if (signals & SIGBREAKF_CTRL_C) { ret = 1; break; }
            if (signals & SIGBREAKF_CTRL_D) { ret = 0; break; }
            if (signals & SIGBREAKF_CTRL_F) PopUp();
            if (signals & timsigs) TC_Dispatcher();
            if (signals & rexsigs) ARexxDispatch(G->RexxHost);
            if (signals & appsigs)
            {
               struct AppMessage *apmsg;
               while ((apmsg = (struct AppMessage *)GetMsg(G->AppPort)))
               {
                  if (apmsg->am_Type == AMTYPE_APPICON)
                  {
                     PopUp();
                     if (apmsg->am_NumArgs)
                     {
                        int wrwin;
                        if      (G->WR[0]) wrwin = 0;
                        else if (G->WR[1]) wrwin = 1;
                        else wrwin = MA_NewNew(NULL, 0);
                        if (wrwin >= 0) WR_App(wrwin, apmsg);
                     }
                  }
                  ReplyMsg((struct Message *)apmsg);
               }
            }
            if (signals & notsigs0)
            {
               while ((msg = GetMsg(G->WR_NRequest[0].nr_stuff.nr_Msg.nr_Port))) ReplyMsg(msg);
               if (G->WR[0]) FileToEditor(G->WR_Filename[0], G->WR[0]->GUI.TE_EDIT);
            }
            if (signals & notsigs1)
            {
               while ((msg = GetMsg(G->WR_NRequest[1].nr_stuff.nr_Msg.nr_Port))) ReplyMsg(msg);
               if (G->WR[1]) FileToEditor(G->WR_Filename[1], G->WR[1]->GUI.TE_EDIT);
            }
            if (signals & notsigs2)
            {
               while ((msg = GetMsg(G->WR_NRequest[2].nr_stuff.nr_Msg.nr_Port))) ReplyMsg(msg);
               if (G->WR[2]) FileToEditor(G->WR_Filename[2], G->WR[2]->GUI.TE_EDIT);
            }
         }
      }
      if (C->SendOnQuit && !args.nocheck) if (TR_IsOnline()) SendWaitingMail();
      if (C->CleanupOnQuit) DoMethod(G->App, MUIM_CallHook, &MA_DeleteOldHook);
      if (C->RemoveOnQuit) DoMethod(G->App, MUIM_CallHook, &MA_DeleteDeletedHook);

      if(ret == 1)
      {
         AppendLog(99, GetStr(MSG_LOG_Terminated), "", "", "", "");
         MA_StartMacro(MACRO_QUIT, NULL);
         FreeData2D(&Header);
         exit(0);
      }
      FreeData2D(&Header);
      Terminate();
   }
   /* not reached */
   return 0;
}
///
