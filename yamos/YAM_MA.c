/***************************************************************************

 YAM - Yet Another Mailer
 Copyright (C) 1995-2000 by Marcel Beck <mbeck@yam.ch>
 Copyright (C) 2000-2002 by YAM Open Source Team

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

#include <stdlib.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <libraries/asl.h>
#include <libraries/iffparse.h>
#include <libraries/gadtools.h>
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/pm.h>
#include <proto/utility.h>
#include <rexx/storage.h>

#include "extra.h"
#include "YAM.h"
#include "YAM_addressbook.h"
#include "YAM_addressbookEntry.h"
#include "YAM_classes.h"
#include "YAM_config.h"
#include "YAM_debug.h"
#include "YAM_error.h"
#include "YAM_find.h"
#include "YAM_folderconfig.h"
#include "YAM_global.h"
#include "YAM_hook.h"
#include "YAM_locale.h"
#include "YAM_main.h"
#include "YAM_mainFolder.h"
#include "YAM_read.h"
#include "YAM_rexx.h"
#include "YAM_userlist.h"
#include "YAM_utilities.h"
#include "YAM_write.h"
#include "classes/Classes.h"

/* local protos */
static ULONG MA_GetSortType(int);
static struct Mail **MA_CreateFullList(struct Folder*);
static struct Mail *MA_MoveCopySingle(struct Mail*, int, struct Folder*, struct Folder*, BOOL);
static void MA_UpdateStatus(void);
static char *MA_AppendRcpt(char*, struct Person*, BOOL);
static int MA_CmpDate(struct Mail**, struct Mail**);
static void MA_InsertIntroText(FILE*, char*, struct ExpandTextData*);
static void MA_EditorNotification(int);
static void MA_SetupQuoteString(struct WR_ClassData*, struct ExpandTextData*, struct Mail*);
static int MA_CheckWriteWindow(int);
static struct Person *MA_GetAddressSelect(struct Mail*);
static char *MA_GetRealSubject(char*);
static int MA_MailCompare(struct Mail*, struct Mail*, int);

/***************************************************************************
 Module: Main
***************************************************************************/

/*** Private functions ***/
/// MA_GetSortType
//  Calculates value for sort indicator
static ULONG MA_GetSortType(int sort)
{
   static const ULONG sort2col[8] = { 0,4,6,1,1,3,5,0 };
   if(sort > 0)
      return sort2col[sort];
   else
      return sort2col[-sort] | MUIV_NList_SortTypeAdd_2Values;
}

///
/// MA_SetSortFlag
//  Sets sort indicators in message listview header
void MA_SetSortFlag(void)
{
   struct Folder *fo = FO_GetCurrentFolder();
   if (!fo) return;

   set(G->MA->GUI.NL_MAILS, MUIA_NList_SortType, MA_GetSortType(fo->Sort[0]));
   set(G->MA->GUI.NL_MAILS, MUIA_NList_SortType2, MA_GetSortType(fo->Sort[1]));
}

///
/// MA_ChangeTransfer
//  Disables menus and toolbar buttons during transfer operations
void MA_ChangeTransfer(BOOL on)
{
   struct MA_GUIData *gui = &G->MA->GUI;
   if (gui->TO_TOOLBAR) DoMethod(gui->TO_TOOLBAR, MUIM_Toolbar_MultiSet, MUIV_Toolbar_Set_Ghosted, !on, 10,11, -1);
   DoMethod(G->App, MUIM_MultiSet, MUIA_Menuitem_Enabled, on, gui->MI_IMPORT, gui->MI_EXPORT, gui->MI_SENDALL, gui->MI_EXCHANGE, gui->MI_GETMAIL, gui->MI_CSINGLE, NULL);
}

///
/// MA_ChangeSelectedFunc
//  User selected some message(s) in the message list
HOOKPROTONHNONP(MA_ChangeSelectedFunc, void)
{
   struct MA_GUIData *gui = &G->MA->GUI;
   struct Folder *fo = FO_GetCurrentFolder();
   int selected, type, i;
   BOOL active, hasattach = FALSE, beingedited = FALSE;
   struct Mail *mail;

   if (!fo) return;

   type = fo->Type;
   DoMethod(gui->NL_MAILS, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &mail);
   fo->LastActive = xget(gui->NL_MAILS, MUIA_NList_Active);
   if ((active = (mail != NULL))) if (mail->Flags & MFLAG_MULTIPART) hasattach = TRUE;
   for (i = 0; i < MAXWR; i++) if (mail && G->WR[i]) if (G->WR[i]->Mail == mail) beingedited = TRUE;
// if (!mail) if (!xget(gui->NL_MAILS, MUIA_NList_Entries)) set(gui->WI, MUIA_Window_ActiveObject, gui->LV_FOLDERS);
   DoMethod(gui->NL_MAILS, MUIM_NList_Select, MUIV_NList_Select_All, MUIV_NList_Select_Ask, &selected);
   if (gui->TO_TOOLBAR)
   {
      DoMethod(gui->TO_TOOLBAR, MUIM_Toolbar_Set, 1, MUIV_Toolbar_Set_Ghosted, !active || !OUTGOING(type) || beingedited);
      DoMethod(gui->TO_TOOLBAR, MUIM_Toolbar_Set, 0, MUIV_Toolbar_Set_Ghosted, !active);
      DoMethod(gui->TO_TOOLBAR, MUIM_Toolbar_MultiSet, MUIV_Toolbar_Set_Ghosted, !active && !selected, 2,3,4,7,8, -1);
   }
   DoMethod(G->App, MUIM_MultiSet, MUIA_Menuitem_Enabled, active || selected,
      gui->MI_MOVE, gui->MI_DELETE, gui->MI_GETADDRESS, gui->MI_REPLY, gui->MI_FORWARD, gui->MI_STATUS,
      gui->MI_EXPMSG, gui->MI_COPY, gui->MI_PRINT, gui->MI_SAVE, gui->MI_CHSUBJ, NULL);
   DoMethod(G->App, MUIM_MultiSet, MUIA_Menuitem_Enabled, active, gui->MI_READ, gui->MI_BOUNCE, NULL);
   DoMethod(G->App, MUIM_MultiSet, MUIA_Menuitem_Enabled, active && OUTGOING(type) && !beingedited, gui->MI_EDIT, NULL);
   DoMethod(G->App, MUIM_MultiSet, MUIA_Menuitem_Enabled, type == FT_OUTGOING && (active || selected), gui->MI_SEND, gui->MI_TOHOLD, gui->MI_TOQUEUED, NULL);
   DoMethod(G->App, MUIM_MultiSet, MUIA_Menuitem_Enabled, !OUTGOING(type) && (active || selected) , gui->MI_TOREAD, gui->MI_TOUNREAD, NULL);
   DoMethod(G->App, MUIM_MultiSet, MUIA_Menuitem_Enabled, hasattach && (active || selected), gui->MI_ATTACH, gui->MI_SAVEATT, gui->MI_REMATT, NULL);
}
MakeHook(MA_ChangeSelectedHook, MA_ChangeSelectedFunc);

///
/// MA_SetMessageInfoFunc
//  Builds help bubble for message list
HOOKPROTONHNONP(MA_SetMessageInfoFunc, void)
{
   static char buffer[SIZE_DEFAULT+SIZE_SUBJECT+2*SIZE_REALNAME+2*SIZE_ADDRESS+SIZE_MFILE];
   char *sh = NULL;
   struct Mail *mail = MA_GetActiveMail(NULL, NULL, NULL);   
   if (mail) SPrintF(sh = buffer, GetStr(MSG_MA_MessageInfo), mail->From.RealName, mail->From.Address, mail->To.RealName, mail->To.Address, mail->Subject, DateStamp2String(&mail->Date, C->SwatchBeat ? DSS_DATEBEAT : DSS_DATETIME), mail->MailFile, mail->Size);
   set(G->MA->GUI.NL_MAILS, MUIA_ShortHelp, sh);
}
MakeHook(MA_SetMessageInfoHook, MA_SetMessageInfoFunc);

///
/// MA_SetFolderInfoFunc
//  Builds help bubble for folder list
HOOKPROTONHNONP(MA_SetFolderInfoFunc, void)
{
   static char buffer[SIZE_DEFAULT+SIZE_NAME+SIZE_PATH];
   char *sh = NULL;
   struct Folder *fo = FO_GetCurrentFolder();

   if (fo && (fo->Type != FT_GROUP))
   {
      SPrintF(sh = buffer, GetStr(MSG_MA_FolderInfo), fo->Name, fo->Path, fo->Size, fo->Total, fo->New, fo->Unread);
   }

   // Update the Folder information in the InfoBar
   DoMethod(G->MA->GUI.IB_INFOBAR, MUIM_InfoBar_RefreshText);

   set(G->MA->GUI.NL_FOLDERS, MUIA_ShortHelp, sh);
}
MakeHook(MA_SetFolderInfoHook, MA_SetFolderInfoFunc);

///
/// MA_GetActiveMail
//  Returns pointers to the active message and folder
struct Mail *MA_GetActiveMail(struct Folder *forcefolder, struct Folder **folderp, int *activep)
{
   struct Folder *folder;
   int active;
   struct Mail *mail = NULL;

   folder = forcefolder ? forcefolder : FO_GetCurrentFolder();

   if(!folder) return(NULL);

   MA_GetIndex(folder);
   get(G->MA->GUI.NL_MAILS, MUIA_NList_Active, &active);
   if (active != MUIV_NList_Active_Off) DoMethod(G->MA->GUI.NL_MAILS, MUIM_NList_GetEntry, active, &mail);
   if (folderp) *folderp = folder;
   if (activep) *activep = active;
   return mail;
}

///
/// MA_SetMailStatus
//  Sets the status of a message
void MA_SetMailStatus(struct Mail *mail, enum MailStatus stat)
{
   struct MailInfo *mi;
   char statstr[3];
   int pf;

   strcpy(statstr, Status[stat&0xFF]);
   if ((pf = ((mail->Flags & 0x0700) >> 8))) sprintf(statstr+1, "%c", pf+'0');
   if (stat != mail->Status)
   {
      mail->Status = stat&0xFF;
      MA_ExpireIndex(mail->Folder);
      mi = GetMailInfo(mail);
      SetComment(mi->FName, statstr);
      if (mi->Display) DoMethod(G->MA->GUI.NL_MAILS, MUIM_NList_Redraw, mi->Pos);
   }
   else SetComment(GetMailFile(NULL, NULL, mail), statstr);
}

///
/// MA_CreateFullList
//  Builds a list containing all messages in a folder
static struct Mail **MA_CreateFullList(struct Folder *fo)
{
   int selected = fo->Total;
   struct Mail *mail, **mlist = NULL;

   if(!fo) return(NULL);

   if (selected) if ((mlist = calloc(selected+2, sizeof(struct Mail *))))
   {
      mlist[0] = (struct Mail *)selected;
      mlist[1] = (struct Mail *)2;
      for (selected = 2, mail = fo->Messages; mail; selected++, mail = mail->Next)
         mlist[selected] = mail;
   }
   return mlist;
}

///
/// MA_CreateMarkedList
//  Builds a linked list containing the selected messages
struct Mail **MA_CreateMarkedList(APTR lv)
{
   int id, selected;
   struct Mail *mail, **mlist = NULL;

   DoMethod(lv, MUIM_NList_Select, MUIV_NList_Select_All, MUIV_NList_Select_Ask, &selected);
   if (selected)
   {
      if ((mlist = calloc(selected+2, sizeof(struct Mail *))))
      {
         mlist[0] = (struct Mail *)selected;
         mlist[1] = (struct Mail *)1;
         for (selected = 2, id = MUIV_NList_NextSelected_Start; ; selected++)
         {
            DoMethod(lv, MUIM_NList_NextSelected, &id);
            if (id == MUIV_NList_NextSelected_End) break;
            DoMethod(lv, MUIM_NList_GetEntry, id, &mail);
            mail->Position = id;
            mlist[selected] = mail;
         }
      }
   }
   else
   {
      DoMethod(G->MA->GUI.NL_MAILS, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &mail);
      if (mail) if ((mlist = calloc(3, sizeof(struct Mail *))))
      {
         get(G->MA->GUI.NL_MAILS, MUIA_NList_Active, &id);
         mail->Position = id;
         mlist[0] = (struct Mail *)1;
         mlist[2] = mail;
      }
   }
   return mlist;
}

///
/// MA_DeleteSingle
//  Deletes a single message
void MA_DeleteSingle(struct Mail *mail, BOOL forceatonce)
{
   if (C->RemoveAtOnce || mail->Folder->Type == FT_DELETED || forceatonce)
   {        
      struct MailInfo *mi = GetMailInfo(mail);
      AppendLogVerbose(21, GetStr(MSG_LOG_DeletingVerbose), AddrName(mail->From), mail->Subject, mail->Folder->Name, "");
      DeleteFile(mi->FName);
      if (mi->Display) DoMethod(G->MA->GUI.NL_MAILS, MUIM_NList_Remove, mi->Pos);
      RemoveMailFromList(mail);
   }
   else MA_MoveCopy(mail, mail->Folder, FO_GetFolderByType(FT_DELETED, NULL), FALSE);
}

///
/// MA_MoveCopySingle
//  Moves or copies a single message from one folder to another
static struct Mail *MA_MoveCopySingle(struct Mail *mail, int pos, struct Folder *from, struct Folder *to, BOOL copyit)
{
   struct Mail cmail = *mail;
   char mfile[SIZE_MFILE];
   APTR lv;

   strcpy(mfile, mail->MailFile);
   if (TransferMailFile(copyit, mail, to))
   {
      strcpy(cmail.MailFile, mail->MailFile);
      if (copyit) AppendLogVerbose(25, GetStr(MSG_LOG_CopyingVerbose), AddrName(mail->From), mail->Subject, from->Name, to->Name);
             else AppendLogVerbose(23, GetStr(MSG_LOG_MovingVerbose),  AddrName(mail->From), mail->Subject, from->Name, to->Name);
      if (copyit) strcpy(mail->MailFile, mfile);
      else
      {
         if ((lv = WhichLV(from))) DoMethod(lv, MUIM_NList_Remove, pos);
         RemoveMailFromList(mail);
      }
      mail = AddMailToList(&cmail, to);
      if ((lv = WhichLV(to))) DoMethod(lv, MUIM_NList_InsertSingle, mail, MUIV_NList_Insert_Sorted);
      if (mail->Status == STATUS_SNT && to->Type == FT_OUTGOING) MA_SetMailStatus(mail, STATUS_WFS);
      return mail;
   }
   return NULL;
}

///
/// MA_MoveCopy
//  Moves or copies messages from one folder to another
void MA_MoveCopy(struct Mail *mail, struct Folder *frombox, struct Folder *tobox, BOOL copyit)
{
   APTR lv;
   struct MailInfo *mi;
   struct Mail **mlist;
   int i, pos, selected = 0;

   if (frombox == tobox && !copyit) return;
   if (!(lv = WhichLV(frombox)) && !mail) return;
   if (mail)
   {
      selected = 1;
      mi = GetMailInfo(mail);
      MA_MoveCopySingle(mail, mi->Pos, frombox, tobox, copyit);
   }
   else if ((mlist = MA_CreateMarkedList(lv)))
   {
      selected = (int)*mlist;
      set(lv, MUIA_NList_Quiet, TRUE);
      BusyGauge(GetStr(MSG_BusyMoving), itoa(selected), selected);
      for (i = 0; i < selected; i++)
      {
         mail = mlist[i+2];
         if (copyit) pos = mail->Position; else { mi = GetMailInfo(mail); pos = mi->Pos; }
         MA_MoveCopySingle(mail, pos, frombox, tobox, copyit);
         BusySet(i+1);
      }
      BusyEnd;
      set(lv, MUIA_NList_Quiet, FALSE);
      free(mlist);
   }
   if (copyit) AppendLogNormal(24, GetStr(MSG_LOG_Copying), (void *)selected, FolderName(frombox), FolderName(tobox), "");
          else AppendLogNormal(22, GetStr(MSG_LOG_Moving),  (void *)selected, FolderName(frombox), FolderName(tobox), "");
   if (!copyit) DisplayStatistics(frombox, FALSE);
   DisplayStatistics(tobox, TRUE);
   MA_ChangeSelectedFunc();
}

///
/// MA_UpdateStatus
//  Changes status of all new messages to unread
static void MA_UpdateStatus(void)
{
   int i;
   struct Mail *mail;
   struct Folder **flist;

   if ((flist = FO_CreateList()))
   {
      for(i = 1; i <= (int)*flist; i++)
      {
        if(!OUTGOING(flist[i]->Type) && flist[i]->LoadedMode == 2)
        {
          BOOL updated = FALSE;

          for (mail = flist[i]->Messages; mail; mail = mail->Next)
          {
            if (mail->Status == STATUS_NEW) { updated = TRUE; MA_SetMailStatus(mail, STATUS_UNR); }
          }

          if (updated) DisplayStatistics(flist[i], TRUE);
        }
      }
      free(flist);
   }
}
///

/*** Main button functions ***/
/// MA_ReadMessage
//  Loads active message into a read window
HOOKPROTONHNO(MA_ReadMessage, void, int *arg)
{
   static int lastwin = 0;
   struct Mail *mail;
   int i, winnum;

   if((mail = MA_GetActiveMail(ANYBOX, NULL, NULL)))
   {
      // Check if this mail is already in a readwindow
      for(i = 0; i < MAXRE; i++)
      {
        if(G->RE[i] && mail == G->RE[i]->MailPtr)
        {
           DoMethod(G->RE[i]->GUI.WI, MUIM_Window_ToFront);
           set(G->RE[i]->GUI.WI, MUIA_Window_Activate, TRUE);
           return;
        }
      }

      if((winnum = RE_Open(C->MultipleWindows ? -1 : lastwin, TRUE)) != -1)
      {
         lastwin = winnum;
         if (SafeOpenWindow(G->RE[winnum]->GUI.WI)) RE_ReadMessage(winnum, mail);
         else
         {
            DisposeModulePush(&G->RE[winnum]);
         }
      }
   }
}
MakeStaticHook(MA_ReadMessageHook, MA_ReadMessage);

///
/// MA_AppendRcpt
//  Appends a recipient address to a string
static char *MA_AppendRcpt(char *sbuf, struct Person *pe, BOOL excludeme)
{
   char *ins;

   if(!pe) return sbuf;

   if (strchr(pe->Address,'@'))
   {
      ins = BuildAddrName2(pe);
   }
   else
   {
      char addr[SIZE_ADDRESS];
      strcpy(addr, pe->Address);
      strcat(addr, strchr(C->EmailAddress, '@'));
      ins = BuildAddrName(addr, pe->RealName);
   }
   if (excludeme) if (!stricmp(pe->Address, C->EmailAddress)) return sbuf;
   if (stristr(sbuf, ins)) return sbuf;
   if (*sbuf) sbuf = StrBufCat(sbuf, ", ");
   return StrBufCat(sbuf, ins);
}

///
/// MA_CmpDate
//  Compares two messages by date
static int MA_CmpDate(struct Mail **pentry1, struct Mail **pentry2)
{
   return CompareDates(&(pentry2[0]->Date), &(pentry1[0]->Date));
}

///
/// MA_InsertIntroText
//  Inserts a phrase into the message text
static void MA_InsertIntroText(FILE *fh, char *text, struct ExpandTextData *etd)
{
   if (*text)
   {
      char *sbuf;
      sbuf = ExpandText(text, etd);
      fprintf(fh, "%s\n", sbuf);
      FreeStrBuf(sbuf);
   }
}

///
/// MA_EditorNotification
//  Starts file notification for temporary message file
static void MA_EditorNotification(int winnum)
{
   FileToEditor(G->WR_Filename[winnum], G->WR[winnum]->GUI.TE_EDIT);
   StartNotify(&G->WR_NRequest[winnum]);
   set(G->WR[winnum]->GUI.TE_EDIT, MUIA_TextEditor_HasChanged, FALSE);
}

///
/// MA_SetupQuoteString
//  Creates quote string by replacing variables with values
static void MA_SetupQuoteString(struct WR_ClassData *wr, struct ExpandTextData *etd, struct Mail *mail)
{
   struct ExpandTextData l_etd;
   char *sbuf;
   if (!etd) etd = &l_etd;
   etd->OS_Name      = mail ? (*(mail->From.RealName) ? mail->From.RealName : mail->From.Address) : "";
   etd->OS_Address   = mail ? mail->From.Address : "";
   etd->OM_Subject   = mail ? mail->Subject : "";
   etd->OM_Date      = mail ? &(mail->Date) : &(G->StartDate);
   etd->R_Name       = "";
   etd->R_Address    = "";
   sbuf = ExpandText(C->QuoteText, etd);
   stccpy(wr->QuoteText, TrimEnd(sbuf), SIZE_DEFAULT);
   FreeStrBuf(sbuf);
   stccpy(wr->AltQuoteText, C->AltQuoteText, SIZE_SMALL);
}

///
/// MA_CheckWriteWindow
//  Opens a write window
static int MA_CheckWriteWindow(int winnum)
{
   if (SafeOpenWindow(G->WR[winnum]->GUI.WI)) return winnum;
   WR_Cleanup(winnum);
   DisposeModulePush(&G->WR[winnum]);
   return -1;
}

///
/// MA_NewNew
//  Creates a new, empty message
int MA_NewNew(struct Mail *mail, int flags)
{
   BOOL quiet = flags & NEWF_QUIET;
   struct Folder *folder = FO_GetCurrentFolder();
   int winnum = -1;
   struct WR_ClassData *wr;
   FILE *out;

   if(!folder) return(-1);

   /* First check if the basic configuration is okay, then open write window */
   if (CO_IsValid()) if ((winnum = WR_Open(quiet ? 2 : -1, FALSE)) >= 0)
   {
      if ((out = fopen(G->WR_Filename[winnum], "w")))
      {
         wr = G->WR[winnum];
         wr->Mode = NEW_NEW;
         wr->Mail = mail;
         if (mail) setstring(wr->GUI.ST_TO, BuildAddrName2(GetReturnAddress(mail)));
         else if(folder->MLSupport)
         {
            if(folder->MLAddress[0]) setstring(wr->GUI.ST_TO, folder->MLAddress);
            if(folder->MLFromAddress[0]) setstring(wr->GUI.ST_FROM, folder->MLFromAddress);
            if(folder->MLReplyToAddress[0]) setstring(wr->GUI.ST_REPLYTO, folder->MLReplyToAddress);
         }

         MA_SetupQuoteString(wr, NULL, NULL);
         MA_InsertIntroText(out, C->NewIntro, NULL);
         MA_InsertIntroText(out, C->Greetings, NULL);
         fclose(out);

         // add a signature to the mail depending on the selected signature for this list
         WR_AddSignature(G->WR_Filename[winnum], folder->MLSupport ? folder->MLSignature: -1);

         if (!quiet) set(wr->GUI.WI, MUIA_Window_Open, TRUE);
         MA_EditorNotification(winnum);
         set(wr->GUI.WI, MUIA_Window_ActiveObject, wr->GUI.ST_TO);
         if (C->LaunchAlways && !quiet) DoMethod(G->App, MUIM_CallHook, &WR_EditHook, winnum);
      }
      else
      {
        DisposeModulePush(&G->WR[winnum]);
      }
   }
   if (winnum >= 0 && !quiet) return MA_CheckWriteWindow(winnum);
   return winnum;
}

///
/// MA_NewEdit
//  Edits a message
int MA_NewEdit(struct Mail *mail, int flags, int ReadwinNum)
{
   BOOL quiet = flags & NEWF_QUIET;
   int i, winnum = -1;
   struct Folder *folder;
   struct WR_ClassData *wr;
   struct ExtendedMail *email;
   FILE *out;
   char *cmsg, *sbuf;

   // return if mail is already being written/edited
   for (i = 0; i < MAXWR; i++) if (G->WR[i] && G->WR[i]->Mail == mail) { DoMethod(G->WR[i]->GUI.WI, MUIM_Window_ToFront); return -1; }
   // check if necessary settings fror writing are OK and open new window
   if (CO_IsValid()) if ((winnum = WR_Open(quiet ? 2 : -1, FALSE)) >= 0)
   {
      if ((out = fopen(G->WR_Filename[winnum], "w")))
      {
         wr = G->WR[winnum];
         wr->Mode = NEW_EDIT;
         wr->Mail = mail;
         wr->ReadwinNum = ReadwinNum;
         folder = mail->Folder;
         email = MA_ExamineMail(folder, mail->MailFile, NULL, TRUE);
         MA_SetupQuoteString(wr, NULL, mail);
         RE_InitPrivateRC(mail, PM_ALL);
         cmsg = RE_ReadInMessage(4, RIM_EDIT);
         fputs(cmsg, out);
         free(cmsg);
         strcpy(wr->MsgID, email->IRTMsgID);
         setstring(wr->GUI.ST_SUBJECT, mail->Subject);
         setstring(wr->GUI.ST_FROM, BuildAddrName2(&mail->From));
         setstring(wr->GUI.ST_REPLYTO, BuildAddrName2(&mail->ReplyTo));
         sbuf = StrBufCpy(NULL, BuildAddrName2(&mail->To));
         if (mail->Flags & MFLAG_MULTIRCPT)
         {
            *sbuf = 0;
            sbuf = MA_AppendRcpt(sbuf, &mail->To, FALSE);
            for (i = 0; i < email->NoSTo; i++) sbuf = MA_AppendRcpt(sbuf, &email->STo[i], FALSE);
            setstring(wr->GUI.ST_TO, sbuf);
            *sbuf = 0;
            for (i = 0; i < email->NoCC; i++) sbuf = MA_AppendRcpt(sbuf, &email->CC[i], FALSE);
            setstring(wr->GUI.ST_CC, sbuf);
            *sbuf = 0;
            for (i = 0; i < email->NoBCC; i++) sbuf = MA_AppendRcpt(sbuf, &email->BCC[i], FALSE);
            setstring(wr->GUI.ST_BCC, sbuf);
         }
         else setstring(wr->GUI.ST_TO, sbuf);
         FreeStrBuf(sbuf);
         if (email->Headers) setstring(wr->GUI.ST_EXTHEADER, email->Headers);
         setcheckmark(wr->GUI.CH_DELSEND, email->DelSend);
         setcheckmark(wr->GUI.CH_RECEIPT, email->RetRcpt);
         setcheckmark(wr->GUI.CH_DISPNOTI, email->ReceiptType == RCPT_TYPE_ALL);
         setcheckmark(wr->GUI.CH_ADDINFO, (mail->Flags&MFLAG_SENDERINFO) == MFLAG_SENDERINFO);
         setcycle(wr->GUI.CY_IMPORTANCE, 1-mail->Importance);
         setmutex(wr->GUI.RA_SIGNATURE, email->Signature);
         setmutex(wr->GUI.RA_SECURITY, wr->OldSecurity = email->Security);
         if (folder->Type != FT_OUTGOING) DoMethod(G->App, MUIM_MultiSet, MUIA_Disabled, TRUE, wr->GUI.BT_SEND, wr->GUI.BT_HOLD, NULL);
         WR_SetupOldMail(winnum);
         RE_FreePrivateRC();
         fclose(out);
         MA_FreeEMailStruct(email);
         if (!quiet) set(wr->GUI.WI, MUIA_Window_Open, TRUE);
         MA_EditorNotification(winnum);
         sbuf = (STRPTR)xget(wr->GUI.ST_TO, MUIA_String_Contents);
         set(wr->GUI.WI, MUIA_Window_ActiveObject, *sbuf ? wr->GUI.TE_EDIT : wr->GUI.ST_TO);
         if (C->LaunchAlways && !quiet) DoMethod(G->App, MUIM_CallHook, &WR_EditHook, winnum);
      }
      else
      {
        DisposeModulePush(&G->WR[winnum]);
      }
   }
   if (winnum >= 0 && !quiet) return MA_CheckWriteWindow(winnum);
   return winnum;
}

///
/// MA_NewBounce
//  Bounces a message
int MA_NewBounce(struct Mail *mail, int flags)
{
   BOOL quiet = flags & NEWF_QUIET;
   int winnum = -1;
   struct WR_ClassData *wr;

   if (CO_IsValid()) if ((winnum = WR_Open(quiet ? 2 : -1, TRUE)) >= 0)
   {
      wr = G->WR[winnum];
      wr->Mode = NEW_BOUNCE;
      wr->Mail = mail;
      if (!quiet) set(wr->GUI.WI, MUIA_Window_Open, TRUE);
      set(wr->GUI.WI, MUIA_Window_ActiveObject, wr->GUI.ST_TO);
   }
   if (winnum >= 0 && !quiet) return MA_CheckWriteWindow(winnum);
   return winnum;
}

///
/// MA_NewForward
//  Forwards a list of messages
int MA_NewForward(struct Mail **mlist, int flags)
{
   BOOL quiet = flags & NEWF_QUIET;
   char buffer[SIZE_LARGE];
   int i, winnum = -1, mlen = (2+(int)mlist[0])*sizeof(struct Mail *);
   struct WR_ClassData *wr;
   struct Mail *mail = NULL;
   struct ExtendedMail *email;
   struct ExpandTextData etd;
   FILE *out;
   char *cmsg, *rsub;

   if (CO_IsValid()) if ((winnum = WR_Open(quiet ? 2 : -1, FALSE)) >= 0)
   {
      if ((out = fopen(G->WR_Filename[winnum], "w")))
      {
         wr = G->WR[winnum];
         wr->Mode = NEW_FORWARD;
         wr->MList = memcpy(malloc(mlen), mlist, mlen);
         rsub = AllocStrBuf(SIZE_SUBJECT);
         qsort(&mlist[2], (int)mlist[0], sizeof(struct Mail *), (int (*)(const void *, const void *))MA_CmpDate);
         MA_InsertIntroText(out, C->NewIntro, NULL);
         for (i = 0; i < (int)mlist[0]; i++)
         {
            mail = mlist[i+2];
            email = MA_ExamineMail(mail->Folder, mail->MailFile, NULL, TRUE);
            MA_SetupQuoteString(wr, &etd, mail);
            etd.OM_MessageID = email->MsgID;
            etd.R_Name = *mail->To.RealName ? mail->To.RealName : mail->To.Address;
            etd.R_Address = mail->To.Address;
            if (*mail->Subject)
            {
               sprintf(buffer, "%s (fwd)", mail->Subject);
               if (!strstr(rsub, buffer))
               {
                  if (*rsub) rsub = StrBufCat(rsub, "; ");
                  rsub = StrBufCat(rsub, buffer);
               }
            }
            RE_InitPrivateRC(mail, PM_ALL);
            etd.HeaderFile = G->RE[4]->FirstPart->Filename;
            MA_InsertIntroText(out, C->ForwardIntro, &etd);
            MA_FreeEMailStruct(email);
            cmsg = RE_ReadInMessage(4, RIM_EDIT);
            fputs(cmsg, out);
            free(cmsg);
            MA_InsertIntroText(out, C->ForwardFinish, &etd);
            if (!(flags & NEWF_FWD_NOATTACH)) WR_SetupOldMail(winnum);
            RE_FreePrivateRC();
         }
         MA_InsertIntroText(out, C->Greetings, NULL);
         fclose(out);

         // add a signature to the mail depending on the selected signature for this list
         WR_AddSignature(G->WR_Filename[winnum], mail->Folder->Type != FT_INCOMING ? mail->Folder->MLSignature: -1);

         setstring(wr->GUI.ST_SUBJECT, rsub);
         FreeStrBuf(rsub);
         if (!quiet) set(wr->GUI.WI, MUIA_Window_Open, TRUE);
         MA_EditorNotification(winnum);
         set(wr->GUI.WI, MUIA_Window_ActiveObject, wr->GUI.ST_TO);
         if (C->LaunchAlways && !quiet) DoMethod(G->App, MUIM_CallHook, &WR_EditHook, winnum);
      }
      else
      {
        DisposeModulePush(&G->WR[winnum]);
      }
   }
   if (winnum >= 0 && !quiet) return MA_CheckWriteWindow(winnum);
   return winnum;
}

///
/// MA_NewReply
//  Creates a reply to a list of messages
int MA_NewReply(struct Mail **mlist, int flags)
{
   int j, i, repmode = 1, winnum = -1, mlen = (2+(int)mlist[0])*sizeof(struct Mail *);
   BOOL doabort = FALSE, multi = (int)mlist[0] > 1, altpat = FALSE, quiet = flags & NEWF_QUIET;
   struct WR_ClassData *wr;
   struct Mail *mail;
   struct ExtendedMail *email;
   struct ExpandTextData etd;
   struct Person *repto, rtml;
   struct Folder *folder = NULL;
   FILE *out;
   char *mlistad = NULL, buffer[SIZE_LARGE];
   char *cmsg, *rfrom = NULL, *rrepto = NULL, *rto = NULL, *rcc = NULL, *rsub = NULL;
   char *domain;

   if (CO_IsValid() && (winnum = WR_Open(quiet ? 2 : -1, FALSE)) >= 0)
   {
      if ((out = fopen(G->WR_Filename[winnum], "w")))
      {
         wr = G->WR[winnum];
         wr->Mode = NEW_REPLY;
         wr->MList = memcpy(malloc(mlen), mlist, mlen);
         rto = AllocStrBuf(SIZE_ADDRESS);
         rcc = AllocStrBuf(SIZE_ADDRESS);
         rsub = AllocStrBuf(SIZE_SUBJECT);
         qsort(&mlist[2], (int)mlist[0], sizeof(struct Mail *), (int (*)(const void *, const void *))MA_CmpDate);

         // Now we iterate through all selected mails
         for (j = 0; j < (int)mlist[0]; j++)
         {
            mail = mlist[j+2];
            folder = mail->Folder;
            email = MA_ExamineMail(folder, mail->MailFile, NULL, TRUE);
            MA_SetupQuoteString(wr, &etd, mail);
            etd.OM_MessageID = email->MsgID;

            // If this mail already have a subject we are going to add a "Re:" to it.
            if (*mail->Subject)
            {
               if (j) strcpy(buffer, mail->Subject);
               else sprintf(buffer, "Re: %s", MA_GetRealSubject(mail->Subject));

               if (!strstr(rsub, buffer))
               {
                  if (*rsub) rsub = StrBufCat(rsub, "; ");
                  rsub = StrBufCat(rsub, buffer);
               }
            }
            if (!multi) strcpy(wr->MsgID, email->MsgID);

            // Now we analyse the folder of the selected mail and if it
            // is a mailing list we have to do some operation
            if (folder)
            {
               char tofld[SIZE_LARGE], fromfld[SIZE_LARGE];

               strcpy(tofld, BuildAddrName2(&mail->To));
               strcpy(fromfld, BuildAddrName2(&mail->From));

               // if the mail we are going to reply resists in the incoming folder
               // we have to check all other folders first.
               if (folder->Type == FT_INCOMING)
               {
                  struct Folder **flist;

                  if ((flist = FO_CreateList()))
                  {
                     for (i = 1; i <= (int)*flist; i++)
                     {
                       if (flist[i]->MLSupport && flist[i]->MLPattern[0] && MatchNoCase(tofld, flist[i]->MLPattern))
                       {
                          mlistad = flist[i]->MLAddress[0] ? flist[i]->MLAddress : fromfld;
                          folder = flist[i];
                          if (flist[i]->MLFromAddress[0])    rfrom  = flist[i]->MLFromAddress;
                          if (flist[i]->MLReplyToAddress[0]) rrepto = flist[i]->MLReplyToAddress;
                          break;
                       }
                     }
                     free(flist);
                  }
               }
               else if (folder->MLSupport && folder->MLPattern[0] && MatchNoCase(tofld, folder->MLPattern))
               {
                  mlistad = folder->MLAddress[0] ? folder->MLAddress : fromfld;
                  if (folder->MLFromAddress[0])    rfrom  = folder->MLFromAddress;
                  if (folder->MLReplyToAddress[0]) rrepto = folder->MLReplyToAddress;
               }
            }

            if(mlistad && !(flags & (NEWF_REP_PRIVATE|NEWF_REP_MLIST)))
            {
               ExtractAddress(mlistad, repto = &rtml);
               if (!strstr(rto, mlistad))
               {
                  if (*rto) rto = StrBufCat(rto, ", ");
                  rto = StrBufCat(rto, mlistad);
               }
            }
            else repto = GetReturnAddress(mail);

            // If this mail is a standard (non-ML) mail and the user hasn`t pressed shift
            if (mail->Flags & MFLAG_MULTIRCPT && !(flags & (NEWF_REP_PRIVATE|NEWF_REP_MLIST)))
            {
              if (!(repmode = MUI_Request(G->App, G->MA->GUI.WI, 0, NULL, GetStr(MSG_MA_ReplyReqOpt), GetStr(MSG_MA_ReplyReq))))
              {
                MA_FreeEMailStruct(email);
                fclose(out);
                doabort = TRUE;
                goto abort_repl;
              }
            }

            if (repmode == 1)
            {
               if (flags & NEWF_REP_PRIVATE) repto = &mail->From;
               else if ((flags & NEWF_REP_MLIST) || mlistad) ; // do nothing
               else if (C->CompareAddress && *mail->ReplyTo.Address && stricmp(mail->From.Address, mail->ReplyTo.Address))
               {
                  sprintf(buffer, GetStr(MSG_MA_CompareReq), mail->From.Address, mail->ReplyTo.Address);
                  switch (MUI_Request(G->App, G->MA->GUI.WI, 0, NULL, GetStr(MSG_MA_Compare3ReqOpt), buffer))
                  {
                     case 3:
                     {
                        rcc = MA_AppendRcpt(rcc, &mail->From, FALSE);
                     }
                     // continue

                     case 2:
                     {
                        repto = &mail->ReplyTo;
                     }
                     break;

                     case 1:
                     {
                        repto = &mail->From;
                     }
                     break;

                     case 0:
                     {
                        MA_FreeEMailStruct(email);
                        doabort = TRUE;
                        fclose(out);
                        goto abort_repl;
                     }
                  }
               }
               rto = MA_AppendRcpt(rto, repto, FALSE);
            }
            else
            {
               if (repmode == 2) rto = MA_AppendRcpt(rto, GetReturnAddress(mail), FALSE);
               rto = MA_AppendRcpt(rto, &mail->To, TRUE);
               for (i = 0; i < email->NoSTo; i++) rto = MA_AppendRcpt(rto, &email->STo[i], TRUE);
               for (i = 0; i < email->NoCC; i++) rcc = MA_AppendRcpt(rcc, &email->CC[i], TRUE);
            }

            etd.R_Name = repto->RealName;
            etd.R_Address = repto->Address;
            altpat = FALSE;
            if (!(domain = strchr(repto->Address,'@'))) domain = strchr(C->EmailAddress,'@');
            if (*C->AltReplyPattern) if (MatchNoCase(domain, C->AltReplyPattern)) altpat = TRUE;
            if (!j) MA_InsertIntroText(out, mlistad ? C->MLReplyHello : (altpat ? C->AltReplyHello : C->ReplyHello), &etd);
            if (C->QuoteMessage && !(flags & NEWF_REP_NOQUOTE))
            {
               if (j) fputs("\n", out);
               RE_InitPrivateRC(mail, PM_TEXTS);
               etd.HeaderFile = G->RE[4]->FirstPart->Filename;
               MA_InsertIntroText(out, mlistad ? C->MLReplyIntro : (altpat ? C->AltReplyIntro : C->ReplyIntro), &etd);
               cmsg = RE_ReadInMessage(4, RIM_QUOTE);
               QuoteWordWrap(cmsg, C->EdWrapMode ? C->EdWrapCol-strlen(wr->QuoteText)-1 : 1024, NULL, wr->QuoteText, out);
               free(cmsg);
               RE_FreePrivateRC();
            }
            MA_FreeEMailStruct(email);
         }
         MA_InsertIntroText(out, mlistad ? C->MLReplyBye : (altpat ? C->AltReplyBye: C->ReplyBye), &etd);
         fclose(out);

         // now we add the configured signature to the reply
         WR_AddSignature(G->WR_Filename[winnum], mlistad ? folder->MLSignature: -1);

         /* If this is a reply to a mail belonging to a mailing list,
            set the "From:" and "Reply-To:" addresses accordingly */
         if (rfrom)  setstring(wr->GUI.ST_FROM,    rfrom);
         if (rrepto) setstring(wr->GUI.ST_REPLYTO, rrepto);

         setstring(wr->GUI.ST_TO, rto);
         setstring(*rto ? wr->GUI.ST_CC : wr->GUI.ST_TO, rcc);
         setstring(wr->GUI.ST_SUBJECT, rsub);
         if (!quiet) set(wr->GUI.WI, MUIA_Window_Open, TRUE);
         MA_EditorNotification(winnum);
         set(wr->GUI.WI, MUIA_Window_ActiveObject, wr->GUI.TE_EDIT);
         if (C->LaunchAlways && !quiet) DoMethod(G->App, MUIM_CallHook, &WR_EditHook, winnum);
      }
      else doabort = TRUE;
   }
   if (winnum >= 0 && !quiet) return MA_CheckWriteWindow(winnum);

abort_repl:

   if (doabort)
   {
    DisposeModulePush(&G->WR[winnum]);
   }
   FreeStrBuf(rto);
   FreeStrBuf(rcc);
   FreeStrBuf(rsub);
   return winnum;
}

///
/// MA_RemoveAttach
//  Removes attachments from a message
void MA_RemoveAttach(struct Mail *mail)
{
   struct Part *part;
   int f;
   FILE *out, *in;
   char *cmsg, buf[SIZE_LINE], fname[SIZE_PATHFILE], tfname[SIZE_PATHFILE];
   struct Folder *fo = mail->Folder;

   sprintf(tfname, "%s.tmp", GetMailFile(fname, NULL, mail));
   RE_InitPrivateRC(mail, PM_ALL);
   cmsg = RE_ReadInMessage(4, RIM_QUIET);
   if ((part = G->RE[4]->FirstPart->Next)) if (part->Next)
      if ((out = fopen(tfname, "w")))
      {
         if ((in = fopen(G->RE[4]->FirstPart->Filename, "r")))
         {
            BOOL infield = FALSE, inbody = FALSE;
            while (fgets(buf, SIZE_LINE, in))
            {
               if (!ISpace(*buf)) infield = !strnicmp(buf, "content-transfer-encoding", 25) || !strnicmp(buf, "content-type", 12);
               if (!infield || inbody) fputs(buf, out);
            }
            fclose(in);
         }
         fputs("Content-Transfer-Encoding: 8bit\nContent-Type: text/plain; charset=iso-8859-1\n\n", out);
         fputs(cmsg, out);
         MA_ExpireIndex(mail->Folder);
         fputs(GetStr(MSG_MA_AttachRemoved), out);
         for (part = part->Next; part; part = part->Next)
            fprintf(out, "%s (%ld %s, %s)\n", part->Name ? part->Name : GetStr(MSG_Unnamed), part->Size, GetStr(MSG_Bytes), part->ContentType);
         fclose(out);
         f = FileSize(tfname); fo->Size += f - mail->Size; mail->Size = f;
         mail->Flags &= ~MFLAG_MULTIPART;
         DeleteFile(fname);
         if (fo->XPKType > 1) DoPack(tfname, fname, mail->Folder);
         else RenameFile(tfname, fname);
         MA_SetMailStatus(mail, mail->Status);
         AppendLog(81, GetStr(MSG_LOG_CroppingAtt), mail->MailFile, mail->Folder->Name, "", "");
      }
   free(cmsg);
   RE_FreePrivateRC();
}

///
/// MA_RemoveAttachFunc
//  Removes attachments from selected messages
HOOKPROTONHNONP(MA_RemoveAttachFunc, void)
{
   struct Mail **mlist;
   int i;

   if ((mlist = MA_CreateMarkedList(G->MA->GUI.NL_MAILS)))
   {
      int selected = (int)*mlist;
      BusyGauge(GetStr(MSG_BusyRemovingAtt), "", selected);
      for (i = 0; i < selected; i++)
      {
         MA_RemoveAttach(mlist[i+2]);
         BusySet(i+1);
      }
      DoMethod(G->MA->GUI.NL_MAILS, MUIM_NList_Redraw, MUIV_NList_Redraw_All);
      MA_ChangeSelectedFunc();
      DisplayStatistics(NULL, TRUE);
      BusyEnd;
   }
}
MakeStaticHook(MA_RemoveAttachHook, MA_RemoveAttachFunc);

///
/// MA_SaveAttachFunc
//  Saves all attachments of selected messages to disk
HOOKPROTONHNONP(MA_SaveAttachFunc, void)
{
   struct Mail **mlist, *mail;
   struct Part *part;
   char *cmsg;
   int i;

   if ((mlist = MA_CreateMarkedList(G->MA->GUI.NL_MAILS)))
   {
      if (ReqFile(ASL_DETACH, G->MA->GUI.WI, GetStr(MSG_RE_SaveMessage), 5, C->DetachDir, ""))
         for (i = 0; i < (int)*mlist; i++)
         {
            RE_InitPrivateRC(mail = mlist[i+2], PM_ALL);
            if ((cmsg = RE_ReadInMessage(4, RIM_QUIET)))
            {
               free(cmsg);
               if ((part = G->RE[4]->FirstPart->Next)) if (part->Next)
                  RE_SaveAll(4, G->ASLReq[ASL_DETACH]->fr_Drawer);
            }
            RE_FreePrivateRC();
         }
      free(mlist);
   }
}
MakeStaticHook(MA_SaveAttachHook, MA_SaveAttachFunc);

///
/// MA_SavePrintFunc
//  Prints selected messages
HOOKPROTONHNO(MA_SavePrintFunc, void, int *arg)
{
   BOOL doprint = *arg != 0;
   struct TempFile *tf;
   struct Mail **mlist;
   char *cmsg;
   int i;

   if (doprint && C->PrinterCheck) if (!CheckPrinter()) return;
   if ((mlist = MA_CreateMarkedList(G->MA->GUI.NL_MAILS)))
   {
      for (i = 0; i < (int)*mlist; i++)
      {
         RE_InitPrivateRC(mlist[i+2], PM_TEXTS);
         if ((cmsg = RE_ReadInMessage(4, RIM_READ)))
         {
            if ((tf = OpenTempFile("w")))
            {
               fputs(cmsg, tf->FP);
               fclose(tf->FP); tf->FP = NULL;
               if (doprint) CopyFile("PRT:", 0, tf->Filename, 0);
               else RE_Export(4, tf->Filename, "", "", 0, FALSE, FALSE, ContType[CT_TX_PLAIN]);
               CloseTempFile(tf);
            }
            free(cmsg);
         }
         RE_FreePrivateRC();
      }
      free(mlist);
   }
}
MakeStaticHook(MA_SavePrintHook, MA_SavePrintFunc);

///
/// MA_NewMessage
//  Starts a new message
int MA_NewMessage(int mode, int flags)
{
   struct Mail *mail, **mlist = NULL;
   int winnr = -1;
   switch (mode)
   {
      case NEW_NEW:     winnr = MA_NewNew(NULL, flags);
                        break;
      case NEW_EDIT:    if ((mail = MA_GetActiveMail(ANYBOX, NULL, NULL))) winnr = MA_NewEdit(mail, flags, -1);
                        break;
      case NEW_BOUNCE:  if ((mail = MA_GetActiveMail(ANYBOX, NULL, NULL))) winnr = MA_NewBounce(mail, flags);
                        break;
      case NEW_FORWARD: if ((mlist = MA_CreateMarkedList(G->MA->GUI.NL_MAILS))) winnr = MA_NewForward(mlist, flags);
                        break;
      case NEW_REPLY:   if ((mlist = MA_CreateMarkedList(G->MA->GUI.NL_MAILS))) winnr = MA_NewReply(mlist, flags);
                        break;
   }
   if (mlist) free(mlist);
   return winnr;
}

///
/// MA_NewMessageFunc
HOOKPROTONHNO(MA_NewMessageFunc, void, int *arg)
{
   int mode = arg[0], flags = 0;
   ULONG qual = arg[1];
   if (arg[2]) return; // Toolbar qualifier bug work-around
   if (mode == NEW_FORWARD && qual & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)) mode = NEW_BOUNCE;
   if (mode == NEW_FORWARD && qual & IEQUALIFIER_CONTROL) flags = NEWF_FWD_NOATTACH;
   if (mode == NEW_REPLY && qual & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)) flags = NEWF_REP_PRIVATE;
   if (mode == NEW_REPLY && qual & (IEQUALIFIER_LALT|IEQUALIFIER_RALT)) flags = NEWF_REP_MLIST;
   if (mode == NEW_REPLY && qual & IEQUALIFIER_CONTROL) flags = NEWF_REP_NOQUOTE;
   MA_NewMessage(mode, flags);
}
MakeStaticHook(MA_NewMessageHook, MA_NewMessageFunc);

///
/// MA_DeleteMessage
//  Deletes selected messages
void MA_DeleteMessage(BOOL delatonce, BOOL force)
{
   struct Mail **mlist, *mail;
   int i, selected;
   APTR lv = G->MA->GUI.NL_MAILS;
   char buffer[SIZE_DEFAULT];
   struct Folder *delfolder = FO_GetFolderByType(FT_DELETED, NULL), *folder = FO_GetCurrentFolder();
   BOOL ignoreall = FALSE;

   if(!folder || !delfolder) return;

   if (!(mlist = MA_CreateMarkedList(lv))) return;
   selected = (int)*mlist;
   if (C->Confirm && selected >= C->ConfirmDelete && !force)
   {
      SPrintF(buffer, selected==1 ? GetStr(MSG_MA_1Selected) : GetStr(MSG_MA_xSelected), selected);
      strcat(buffer, GetStr(MSG_MA_ConfirmDel));
      if (!MUI_Request(G->App, G->MA->GUI.WI, 0, GetStr(MSG_MA_ConfirmReq), GetStr(MSG_OkayCancelReq), buffer))
      {
         free(mlist);
         return;
      }
   }
   set(lv, MUIA_NList_Quiet, TRUE);

   BusyGauge(GetStr(MSG_BusyDeleting), itoa(selected), selected);
   if (C->RemoveAtOnce || folder == delfolder) delatonce = TRUE;
   for (i = 0; i < selected; i++)
   {
      mail = mlist[i+2];
      if (mail->Flags & MFLAG_SENDMDN) if ((mail->Status == STATUS_NEW || mail->Status == STATUS_UNR) && !ignoreall) ignoreall = RE_DoMDN(MDN_DELE, mail, TRUE);
      if (delatonce) MA_DeleteSingle(mail, TRUE);
      else
      {
         struct MailInfo *mi = GetMailInfo(mail);
         MA_MoveCopySingle(mail, mi->Pos, mail->Folder, delfolder, FALSE);
      }
      BusySet(i+1);
   }
   BusyEnd;
   set(lv, MUIA_NList_Quiet, FALSE);
   free(mlist);
   if (delatonce)
      AppendLogNormal(20, GetStr(MSG_LOG_Deleting), (void *)selected, folder->Name, "", "");
   else
   {
      AppendLogNormal(22, GetStr(MSG_LOG_Moving), (void *)selected, folder->Name, delfolder->Name, "");
      DisplayStatistics(delfolder, FALSE);
   }
   DisplayStatistics(NULL, TRUE);
   MA_ChangeSelectedFunc();
}

///
/// MA_DeleteMessageFunc
HOOKPROTONHNO(MA_DeleteMessageFunc, void, int *arg)
{
   BOOL delatonce = arg[0] & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT);
   if (arg[1]) return; // Toolbar qualifier bug work-around
   MA_DeleteMessage(delatonce, FALSE);
}
MakeStaticHook(MA_DeleteMessageHook, MA_DeleteMessageFunc);

///
/// MA_DelKey
//  User pressed DEL key
HOOKPROTONHNO(MA_DelKeyFunc, void, int *arg)
{
   APTR obj;
   get(G->MA->GUI.WI, MUIA_Window_ActiveObject, &obj);
   if (!obj || obj == MUIV_Window_ActiveObject_None) get(G->MA->GUI.WI, MUIA_Window_DefaultObject, &obj);
   if (obj == G->MA->GUI.LV_FOLDERS)
      CallHookPkt(&FO_DeleteFolderHook, 0, 0);
   else
      MA_DeleteMessage(arg[0], FALSE);
}
MakeStaticHook(MA_DelKeyHook, MA_DelKeyFunc);

///
/// MA_GetAddressSelect
//  Asks user which address (from/replyto) to store
static struct Person *MA_GetAddressSelect(struct Mail *mail)
{
   struct Person *pe = GetReturnAddress(mail);
   if (C->CompareAddress && *mail->ReplyTo.Address) if (stricmp(mail->From.Address, mail->ReplyTo.Address))
   {
      char buffer[SIZE_LARGE];
      sprintf(buffer, GetStr(MSG_MA_CompareReq), mail->From.Address, mail->ReplyTo.Address);
      switch (MUI_Request(G->App, G->MA->GUI.WI, 0, NULL, GetStr(MSG_MA_Compare2ReqOpt), buffer))
      {
         case 2: pe = &mail->ReplyTo; break;
         case 1: pe = &mail->From; break;
         case 0: pe = NULL;
      }
   }
   return pe;
}

///
/// MA_GetAddress
//  Stores address from a list of messages to the address book
void MA_GetAddress(struct Mail **mlist)
{
   int i, j, winnum, num = (int)mlist[0], mode = AET_USER;
   struct Folder *folder = mlist[2]->Folder;
   BOOL outgoing = folder ? OUTGOING(folder->Type) : FALSE;
   struct ExtendedMail *email;

   if (num == 1)
   {
      if (outgoing && mlist[2]->Flags & MFLAG_MULTIRCPT) mode = AET_LIST;
   }
   else mode = AET_LIST;
   if (C->UseCManager)
   {
/*
      struct CMUser *user;
      if (mode == AET_USER) if (user = CM_AllocEntry(CME_USER))
      {
         char *p;
         stccpy(user->Address, pe->Address, 80);
         stccpy(user->Name, pe->RealName, 80);
         if (p = strchr(user->Name, ' ')) { *p = 0; stccpy(user->LastName, p, 80); }
         CM_AddEntry(user);
         CM_FreeEntry(user);
      }
*/
   }
   else
   {
      struct Person *pe = NULL;
      if (mode == AET_USER)
      {
         pe = outgoing ? &(mlist[2]->To) : MA_GetAddressSelect(mlist[2]);
         if (!pe) return;
      }
      DoMethod(G->App, MUIM_CallHook, &AB_OpenHook, ABM_EDIT);
      if ((winnum = EA_Init(mode, NULL)) >= 0)
         if (mode == AET_USER)
         {
            setstring(G->EA[winnum]->GUI.ST_REALNAME, pe->RealName);
            setstring(G->EA[winnum]->GUI.ST_ADDRESS, pe->Address);
         }
         else
            for (i = 2; i < num+2; i++)
            {
               if (outgoing)
               {
                  DoMethod(G->EA[winnum]->GUI.LV_MEMBER, MUIM_List_InsertSingle, BuildAddrName2(&(mlist[i]->To)), MUIV_List_Insert_Bottom);
                  if ((mlist[i]->Flags & MFLAG_MULTIRCPT)) if ((email = MA_ExamineMail(mlist[i]->Folder, mlist[i]->MailFile, NULL, TRUE)))
                  {
                     for (j = 0; j < email->NoSTo; j++) DoMethod(G->EA[winnum]->GUI.LV_MEMBER, MUIM_List_InsertSingle, BuildAddrName2(&(email->STo[j])), MUIV_List_Insert_Bottom);
                     for (j = 0; j < email->NoCC; j++) DoMethod(G->EA[winnum]->GUI.LV_MEMBER, MUIM_List_InsertSingle, BuildAddrName2(&(email->CC[j])), MUIV_List_Insert_Bottom);
                     MA_FreeEMailStruct(email);
                  }
               }
               else
                  DoMethod(G->EA[winnum]->GUI.LV_MEMBER, MUIM_List_InsertSingle, BuildAddrName2(GetReturnAddress(mlist[i])), MUIV_List_Insert_Bottom);
            }
   }
}

///
/// MA_GetAddressFunc
//  Stores addresses from selected messages to the address book
HOOKPROTONHNONP(MA_GetAddressFunc, void)
{
   struct Mail **mlist = MA_CreateMarkedList(G->MA->GUI.NL_MAILS);
   if (mlist)
   {
      MA_GetAddress(mlist);
      free(mlist);
   }
}
MakeStaticHook(MA_GetAddressHook, MA_GetAddressFunc);

///
/// MA_PopNow
//  Fetches new mail from POP3 account(s)
void MA_PopNow(int mode, int pop)
{
   if (G->TR) return; // Don't proceed if another transfer is in progress
   if (C->UpdateStatus) MA_UpdateStatus();
   MA_StartMacro(MACRO_PREGET, itoa(mode-POP_USER));
   TR_GetMailFromNextPOP(TRUE, pop, mode);
}

///
/// MA_PopNowFunc
HOOKPROTONHNO(MA_PopNowFunc, void, int *arg)
{
   ULONG qual = (ULONG)arg[2];
   if (arg[3]) return; // Toolbar qualifier bug work-around
   if (qual & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)) G->TR_Exchange = TRUE;
   MA_PopNow(arg[0],arg[1]);
}
MakeStaticHook(MA_PopNowHook, MA_PopNowFunc);
///

/*** Sub-button functions ***/
/// MA_AllocRules
//  Allocates and initializes search structures for filters
int MA_AllocRules(struct Search **search, enum ApplyMode mode)
{
   int scnt = 0, i, j, stat;
   struct Rule *rule;

   for (i = 0; i < MAXRU; i++) if ((rule = C->RU[i]))
   {
      if (mode == APPLY_AUTO && !rule->ApplyToNew) continue;
      if (mode == APPLY_USER && !rule->ApplyOnReq) continue;
      if (mode == APPLY_SENT && !rule->ApplyToSent) continue;
      if (mode == APPLY_REMOTE && !rule->Remote) continue;
      for (j = 0; j < 2; j++)
      {
         search[scnt+j*MAXRU] = malloc(sizeof(struct Search));
         stat = 0;
         if (rule->Field[j] == 11) for (; stat < 8; stat++) if (!stricmp(rule->Match[j], Status[stat])) break;
         FI_PrepareSearch(search[scnt+j*MAXRU], rule->Field[j], rule->CaseSens[j], rule->SubField[j], rule->Comparison[j], stat, rule->Substring[j], rule->Match[j], rule->CustomField[j]);
         search[scnt+j*MAXRU]->Rule = rule;
      }
      scnt++;
   }
   return scnt;
}

///
/// MA_FreeRules
//  Frees filter search structures
void MA_FreeRules(struct Search **search, int scnt)
{
   int i, j;
   for (i = 0; i < scnt; i++) for (j = 0; j < 2; j++)
   {
      FreeData2D(&(search[i+j*MAXRU]->List));
      free(search[i+j*MAXRU]);
   }
}

///
/// MA_ExecuteRuleAction
//  Applies filter action to a message
BOOL MA_ExecuteRuleAction(struct Rule *rule, struct Mail *mail)
{
  struct Mail *mlist[3];
  struct Folder* fo;
  mlist[0] = (struct Mail *)1; mlist[1] = NULL; mlist[2] = mail;

  if ((rule->Actions & 1) == 1) if (*rule->BounceTo)
  {
    G->RRs.Bounced++;
    MA_NewBounce(mail, TRUE);
    setstring(G->WR[2]->GUI.ST_TO, rule->BounceTo);
    DoMethod(G->App, MUIM_CallHook, &WR_NewMailHook, WRITE_QUEUE, 2);
  }

  if ((rule->Actions & 2) == 2) if (*rule->ForwardTo)
  {
    G->RRs.Forwarded++;
    MA_NewForward(mlist, TRUE);
    setstring(G->WR[2]->GUI.ST_TO, rule->ForwardTo);
    WR_NewMail(WRITE_QUEUE, 2);
  }

  if ((rule->Actions & 4) == 4) if (*rule->ReplyFile)
  {
    MA_NewReply(mlist, TRUE);
    FileToEditor(rule->ReplyFile, G->WR[2]->GUI.TE_EDIT);
    WR_NewMail(WRITE_QUEUE, 2);
    G->RRs.Replied++;
  }

  if ((rule->Actions & 8) == 8) if (*rule->ExecuteCmd)
  {
    char buf[SIZE_COMMAND+SIZE_PATHFILE];
    strcpy(buf, rule->ExecuteCmd); strcat(buf, " ");
    strcat(buf, GetMailFile(NULL, NULL, mail));
    ExecuteCommand(buf, FALSE, OUT_DOS);
    G->RRs.Executed++;
  }

  if ((rule->Actions & 16) == 16) if (*rule->PlaySound)
  {
    PlaySound(rule->PlaySound);
  }

  if ((rule->Actions & 32) == 32)
  {
    if((fo = FO_GetFolderByName(rule->MoveTo, NULL)))
    {
      if (mail->Folder != fo)
      {
        G->RRs.Moved++;
        if (fo->LoadedMode != 2 && (fo->XPKType&1)) fo->Flags |= FOFL_FREEXS;
        MA_MoveCopy(mail, mail->Folder, fo, FALSE);
        return FALSE;
      }
    }
    else ER_NewError(GetStr(MSG_ER_CANTMOVEMAIL), mail->MailFile, rule->MoveTo);
  }

  if ((rule->Actions & 64) == 64)
  {
    G->RRs.Deleted++;
    if (mail->Flags & MFLAG_SENDMDN) if (mail->Status == STATUS_NEW || mail->Status == STATUS_UNR) RE_DoMDN(MDN_DELE|MDN_AUTOACT, mail, FALSE);
    MA_DeleteSingle(mail, FALSE);
    return FALSE;
  }

  return TRUE;
}

///
/// MA_ApplyRulesFunc
//  Apply filters
HOOKPROTONHNO(MA_ApplyRulesFunc, void, int *arg)
{
   struct Mail *mail, **mlist = NULL;
   struct Folder *folder;
   int m, i, scnt, matches = 0, minselected = (arg[1] & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)) ? 1 : 2;
   enum ApplyMode mode = arg[0];
   struct Search *search[2*MAXRU];
   APTR lv = G->MA->GUI.NL_MAILS;
   char buf[SIZE_LARGE];

   if (arg[2]) return; // Toolbar qualifier bug work-around
   folder = (mode == APPLY_AUTO) ? FO_GetFolderByType(FT_INCOMING, NULL) : FO_GetCurrentFolder();

   if(!folder) return;

   if (mode == APPLY_USER && folder->Type != FT_INCOMING)
   {
      sprintf(buf, GetStr(MSG_MA_ConfirmFilter), folder->Name);
      if (!MUI_Request(G->App, G->MA->GUI.WI, 0, GetStr(MSG_MA_ConfirmReq), GetStr(MSG_YesNoReq), buf)) return;
   }
   memset(&G->RRs, 0, sizeof(struct RuleResult));
   set(lv, MUIA_NList_Quiet, TRUE); G->AppIconQuiet = TRUE;
   if ((scnt = MA_AllocRules(search, mode)))
   {
      if (mode == APPLY_USER || mode == APPLY_RX || mode == APPLY_RX_ALL) if ((mlist = MA_CreateMarkedList(lv))) if ((int)mlist[0] < minselected) { free(mlist); mlist = NULL; }
      if (!mlist) mlist = MA_CreateFullList(folder);
      if (mlist)
      {
         BusyGauge(GetStr(MSG_BusyFiltering), "", (int)*mlist);
         for (m = 0; m < (int)*mlist; m++)
         {
            mail = mlist[m+2];
            if ((mode == APPLY_AUTO || mode == APPLY_RX) && mail->Status != STATUS_NEW) continue;
            G->RRs.Checked++;

            for (i = 0; i < scnt; i++)
            {
               if (FI_DoComplexSearch(search[i], search[i]->Rule->Combine, search[i+MAXRU], mail))
               {
                  matches++;
                  if (!MA_ExecuteRuleAction(search[i]->Rule, mail)) break;
               }
            }
            BusySet(m+1);
         }
         free(mlist);
         if (G->RRs.Moved) MA_FlushIndexes(FALSE);
         if (G->RRs.Checked) AppendLog(26, GetStr(MSG_LOG_Filtering), (void *)(G->RRs.Checked), folder->Name, (void *)matches, "");

         BusyEnd;
      }
      MA_FreeRules(search, scnt);
   }
   set(lv, MUIA_NList_Quiet, FALSE); G->AppIconQuiet = FALSE;
   DisplayStatistics(NULL, TRUE);

   if (G->RRs.Checked && mode == APPLY_USER)
   {
      sprintf(buf, GetStr(MSG_MA_FilterStats), G->RRs.Checked, G->RRs.Forwarded, G->RRs.Moved, G->RRs.Deleted);
      MUI_Request(G->App, G->MA->GUI.WI, 0, NULL, GetStr(MSG_OkayReq), buf);
   }
}
MakeHook(MA_ApplyRulesHook, MA_ApplyRulesFunc);

///
/// MA_SendMList
//  Sends a list of messages
BOOL MA_SendMList(struct Mail **mlist)
{
   BOOL success = FALSE;
   MA_StartMacro(MACRO_PRESEND, NULL);
   if (TR_OpenTCPIP())
   {
      if (CO_IsValid()) if ((G->TR = TR_New(TR_SEND)))
      {
         if (SafeOpenWindow(G->TR->GUI.WI)) success = TR_ProcessSEND(mlist);
         else { MA_ChangeTransfer(TRUE); DisposeModulePush(&G->TR); }
      }
      TR_CloseTCPIP();
   }
   else ER_NewError(GetStr(MSG_ER_NoTCP), NULL, NULL);
   MA_StartMacro(MACRO_POSTSEND, NULL);
   return success;
}

///
/// MA_Send
//  Sends selected or all messages
BOOL MA_Send(enum SendMode sendpos)
{
   struct Mail **mlist;
   APTR lv = G->MA->GUI.NL_MAILS;
   BOOL success = FALSE;
   if (!G->TR)
   {
      MA_ChangeFolder(FO_GetFolderByType(FT_OUTGOING, NULL), TRUE);
      if (sendpos == SEND_ALL) DoMethod(lv, MUIM_NList_Select, MUIV_NList_Select_All, MUIV_NList_Select_On, NULL);
      if ((mlist = MA_CreateMarkedList(lv)))
      {
         success = MA_SendMList(mlist);
         free(mlist);
      }
   }
   return success;
}

///
/// MA_SendFunc
HOOKPROTONHNO(MA_SendFunc, void, int *arg)
{
   MA_Send(arg[0]);
}
MakeHook(MA_SendHook, MA_SendFunc);
///

/*** Menu options ***/
/// MA_SetStatusTo
//  Sets status of selectes messages
void MA_SetStatusTo(int status)
{
   APTR lv = G->MA->GUI.NL_MAILS;
   struct Mail **mlist;

   if ((mlist = MA_CreateMarkedList(lv)))
   {
      int i;
      set(lv, MUIA_NList_Quiet, TRUE);
      for (i = 0; i < (int)*mlist; i++) if (mlist[i+2]->Status != status) MA_SetMailStatus(mlist[i+2], status);
      set(lv, MUIA_NList_Quiet, FALSE);
      free(mlist);
      DisplayStatistics(NULL, TRUE);
   }
}

///
/// MA_SetStatusToFunc
HOOKPROTONHNO(MA_SetStatusToFunc, void, int *arg)
{
   MA_SetStatusTo(*arg);
}
MakeStaticHook(MA_SetStatusToHook, MA_SetStatusToFunc);

///
/// MA_DeleteOldFunc
//  Deletes old messages
HOOKPROTONHNONP(MA_DeleteOldFunc, void)
{
   struct Folder **flist;
   struct DateStamp today;
   long today_days;
   int f;
   struct Mail *mail, *next;

   DateStamp(&today); today_days = today.ds_Days;
   if ((flist = FO_CreateList()))
   {
      BusyGauge(GetStr(MSG_BusyDeletingOld), "", (int)*flist);

      for (f = 1; f <= (int)*flist; f++)
      {
        if (flist[f]->MaxAge)
        {
          if (MA_GetIndex(flist[f]))
          {
            for(mail = flist[f]->Messages; mail; mail = next)
            {
              next = mail->Next;
              today.ds_Days = today_days - flist[f]->MaxAge;
              if (CompareDates(&today, &(mail->Date)) < 0)
              {
                if (flist[f]->Type == FT_DELETED || (mail->Status != STATUS_NEW && mail->Status != STATUS_UNR))
                {
                  MA_DeleteSingle(mail, C->RemoveOnQuit);
                }
              }
            }
          }
        }

        BusySet(f);
      }
      free(flist);

      BusyEnd;
   }
}
MakeHook(MA_DeleteOldHook, MA_DeleteOldFunc);

///
/// MA_DeleteDeletedFunc
//  Removes messages from 'deleted' folder
HOOKPROTONHNO(MA_DeleteDeletedFunc, void, int *arg)
{
  BOOL quiet = *arg != 0;
  int i = 0;
  struct Mail *mail;
  struct Folder *folder = FO_GetFolderByType(FT_DELETED, NULL);

  if(!folder) return;

  BusyGauge(GetStr(MSG_BusyEmptyingTrash), "", folder->Total);

  for (mail = folder->Messages; mail; mail = mail->Next)
  {
    BusySet(++i);
    AppendLogVerbose(21, GetStr(MSG_LOG_DeletingVerbose), AddrName(mail->From), mail->Subject, folder->Name, "");
    DeleteFile(GetMailFile(NULL, NULL, mail));
  }

  // We only clear the folder if it wasn`t empty anyway..
  if(i > 0)
  {
    ClearMailList(folder, TRUE);

    MA_ExpireIndex(folder);

    if(FO_GetCurrentFolder() == folder) DisplayMailList(folder, G->MA->GUI.NL_MAILS);

    AppendLogNormal(20, GetStr(MSG_LOG_Deleting), (void *)i, folder->Name, "", "");

    if(quiet == FALSE) DisplayStatistics(folder, TRUE);
  }

  BusyEnd;
}            
MakeHook(MA_DeleteDeletedHook, MA_DeleteDeletedFunc);

///
/// MA_RescanIndexFunc
//  Updates index of current folder
HOOKPROTONHNONP(MA_RescanIndexFunc, void)
{
   struct Folder *folder = FO_GetCurrentFolder();

   if(!folder) return;

   MA_ScanMailBox(folder);
   MA_SaveIndex(folder);
   MA_ChangeFolder(NULL, FALSE);
}
MakeHook(MA_RescanIndexHook, MA_RescanIndexFunc);

///
/// MA_ExportMessages
//  Saves messages to a UUCP mailbox file
BOOL MA_ExportMessages(BOOL all, char *filename, BOOL append)
{
   BOOL success = FALSE;
   char outname[SIZE_PATHFILE];
   struct Mail **mlist;
   if (all) mlist = MA_CreateFullList(FO_GetCurrentFolder());
   else mlist = MA_CreateMarkedList(G->MA->GUI.NL_MAILS);

   if (mlist)
   {
      if (!filename) if (ReqFile(ASL_IMPORT, G->MA->GUI.WI, GetStr(MSG_MA_ExportMessages), 1, C->DetachDir, ""))
      {
         strmfp(filename = outname, G->ASLReq[ASL_IMPORT]->fr_Drawer, G->ASLReq[ASL_IMPORT]->fr_File);
         if (FileExists(filename))
         {
            char * a = GetStr(MSG_MA_ExportAppendOpts);
            char * b = GetStr(MSG_MA_ExportAppendReq);
            switch (MUI_Request(G->App, G->MA->GUI.WI, 0, GetStr(MSG_MA_ExportMessages), a, b))
            {
               case 1: append = FALSE; break;
               case 2: append = TRUE; break;
               case 0: filename = NULL;
            }
         }
      }
      if (filename) if ((G->TR = TR_New(TR_EXPORT)))
      {
         if (SafeOpenWindow(G->TR->GUI.WI)) success = TR_ProcessEXPORT(filename, mlist, append);
         else {  MA_ChangeTransfer(TRUE); DisposeModulePush(&G->TR); }
      }
      free(mlist);
   }
   return success;
}

///
/// MA_ExportMessagesFunc
HOOKPROTONHNO(MA_ExportMessagesFunc, void, int *arg)
{
   MA_ExportMessages((BOOL)*arg, NULL, FALSE);
}
MakeStaticHook(MA_ExportMessagesHook, MA_ExportMessagesFunc);

///
/// MA_ImportMessages
//  Imports messages from a UUCP mailbox file
BOOL MA_ImportMessages(char *fname)
{
   struct Folder *actfo = FO_GetCurrentFolder();
   FILE *fh;

   if(!actfo) return(FALSE);

   if ((fh = fopen(fname, "r")))
   {
      if ((G->TR = TR_New(TR_IMPORT)))
      {
         stccpy(G->TR->ImportFile, fname, SIZE_PATHFILE);
         TR_SetWinTitle(TRUE, FilePart(fname));
         G->TR->ImportBox = actfo;

         if (SafeOpenWindow(G->TR->GUI.WI))
         {
            TR_GetMessageList_IMPORT(fh);
            fclose(fh);
            return TRUE;
         }
         else { MA_ChangeTransfer(TRUE); DisposeModulePush(&G->TR); }
      }
      fclose(fh);
   }
   return FALSE;
}

///
/// MA_ImportMessagesFunc
HOOKPROTONHNONP(MA_ImportMessagesFunc, void)
{
   if (ReqFile(ASL_IMPORT, G->MA->GUI.WI, GetStr(MSG_MA_ImportMessages), 0, C->DetachDir, ""))
   {
      char inname[SIZE_PATHFILE];
      strmfp(inname, G->ASLReq[ASL_IMPORT]->fr_Drawer, G->ASLReq[ASL_IMPORT]->fr_File);
      MA_ImportMessages(inname);
   }
}
MakeStaticHook(MA_ImportMessagesHook, MA_ImportMessagesFunc);

///
/// MA_MoveMessageFunc
//  Moves selected messages to a user specified folder
HOOKPROTONHNONP(MA_MoveMessageFunc, void)
{
   struct Folder *src = FO_GetCurrentFolder(), *dst;

   if(!src) return;

   if ((dst = FolderRequest(GetStr(MSG_MA_MoveMsg), GetStr(MSG_MA_MoveMsgReq), GetStr(MSG_MA_MoveGad), GetStr(MSG_Cancel), src, G->MA->GUI.WI)))
      MA_MoveCopy(NULL, src, dst, FALSE);
}
MakeStaticHook(MA_MoveMessageHook, MA_MoveMessageFunc);

///
/// MA_CopyMessageFunc
//  Copies selected messages to a user specified folder
HOOKPROTONHNONP(MA_CopyMessageFunc, void)
{
   struct Folder *src = FO_GetCurrentFolder(), *dst;

   if(!src) return;

   if ((dst = FolderRequest(GetStr(MSG_MA_CopyMsg), GetStr(MSG_MA_MoveMsgReq), GetStr(MSG_MA_CopyGad), GetStr(MSG_Cancel), NULL, G->MA->GUI.WI)))
      MA_MoveCopy(NULL, src, dst, TRUE);
}
MakeStaticHook(MA_CopyMessageHook, MA_CopyMessageFunc);

///
/// MA_ChangeSubject
//  Changes subject of a message
void MA_ChangeSubject(struct Mail *mail, char *subj)
{
   struct Folder *fo = mail->Folder;
   int f;
   FILE *oldfh, *newfh;
   char *oldfile, newfile[SIZE_PATHFILE], fullfile[SIZE_PATHFILE], buf[SIZE_LINE];

   if (!strcmp(subj, mail->Subject)) return;
   if (!StartUnpack(oldfile = GetMailFile(NULL, NULL, mail), fullfile, fo)) return;
   strmfp(newfile, GetFolderDir(fo), "00000.tmp");
   if ((newfh = fopen(newfile, "w")))
   {
      if ((oldfh = fopen(fullfile, "r")))
      {
         BOOL infield = FALSE, inbody = FALSE, hasorigsubj = FALSE;
         while (fgets(buf, SIZE_LINE, oldfh))
         {
            if (*buf == '\n' && !inbody)
            {
               inbody = TRUE;
               if (!hasorigsubj) EmitHeader(newfh, "X-Original-Subject", mail->Subject);
               EmitHeader(newfh, "Subject", subj);
            }
            if (!ISpace(*buf))
            {
               infield = !strnicmp(buf, "subject:", 8);
               if (!strnicmp(buf, "x-original-subject:", 19)) hasorigsubj = TRUE;
            }
            if (!infield || inbody) fputs(buf, newfh);
         }
         fclose(oldfh);
         DeleteFile(oldfile);
      }
      fclose(newfh);
      f = FileSize(newfile); fo->Size += f - mail->Size; mail->Size = f;
      AppendLog(82, GetStr(MSG_LOG_ChangingSubject), mail->Subject, mail->MailFile, fo->Name, subj);
      strcpy(mail->Subject, subj);
      MA_ExpireIndex(fo);
      if (fo->XPKType > 1) DoPack(newfile, oldfile, fo);
      else RenameFile(newfile, oldfile);
      MA_SetMailStatus(mail, mail->Status);
   }
   FinishUnpack(fullfile);
}

///
/// MA_ChangeSubjectFunc
//  Changes subject of selected messages
HOOKPROTONHNONP(MA_ChangeSubjectFunc, void)
{
   struct Mail **mlist, *mail;
   int i, selected;
   BOOL ask = TRUE;
   APTR lv = G->MA->GUI.NL_MAILS;
   char subj[SIZE_SUBJECT];

   if (!(mlist = MA_CreateMarkedList(lv))) return;
   selected = (int)*mlist;
   for (i = 0; i < selected; i++)
   {
      mail = mlist[i+2];
      if (ask)
      {
         strcpy(subj, mail->Subject);
         switch (StringRequest(subj, SIZE_SUBJECT, GetStr(MSG_MA_ChangeSubj), GetStr(MSG_MA_ChangeSubjReq), GetStr(MSG_Okay), (i || selected == 1) ? NULL : GetStr(MSG_MA_All), GetStr(MSG_Cancel), FALSE, G->MA->GUI.WI))
         {
            case 0: free(mlist); return;
            case 2: ask = FALSE;
         }
      }
      MA_ChangeSubject(mail, subj);
   }
   free(mlist);
   DoMethod(lv, MUIM_NList_Redraw, MUIV_NList_Redraw_All);
   DisplayStatistics(NULL, TRUE);
}
MakeStaticHook(MA_ChangeSubjectHook, MA_ChangeSubjectFunc);

///
/// MA_AboutMUIFunc
//  Displays 'About MUI' window
HOOKPROTONHNONP(MA_AboutMUIFunc, void)
{
   static APTR muiwin = NULL;

   if (!muiwin) muiwin = AboutmuiObject,
      MUIA_Window_RefWindow, G->MA->GUI.WI,
      MUIA_Aboutmui_Application, G->App,
   End;
   if (muiwin) SafeOpenWindow(muiwin); else DisplayBeep(0);
}
MakeStaticHook(MA_AboutMUIHook, MA_AboutMUIFunc);

///
/// MA_CheckVersionFunc
//  Checks YAM homepage for new program versions
HOOKPROTONHNONP(MA_CheckVersionFunc, void)
{
   struct TempFile *tf;
   int mon, day, year;
   long thisver, currver;
   char newver[SIZE_SMALL], buf[SIZE_LARGE];

   if (TR_OpenTCPIP())
   {
      sscanf(yamversiondate, "%ld.%ld.%ld", &day, &mon, &year);
      thisver = (year<78 ? 1000000:0)+year*10000+mon*100+day;
      BusyText(GetStr(MSG_BusyGettingVerInfo), "");
      tf = OpenTempFile(NULL);
      if (TR_DownloadURL(C->SupportSite, "files/", "version", tf->Filename))
      {
         if ((tf->FP = fopen(tf->Filename,"r")))
         {
            fscanf(tf->FP, "%ld.%ld.%ld", &day, &mon, &year);
            GetLine(tf->FP, newver, SIZE_SMALL);
            currver = (year<78 ? 1000000:0)+year*10000+mon*100+day;
            sprintf(buf, GetStr(MSG_MA_LatestVersion), &newver[1], day, mon, year < 78 ? 2000+year : 1900+year, yamversion, yamversiondate,
               currver > thisver ? GetStr(MSG_MA_NewVersion) : GetStr(MSG_MA_NoNewVersion));
            if (MUI_Request(G->App, G->MA->GUI.WI, 0, GetStr(MSG_MA_CheckVersion), GetStr(MSG_MA_VersionReqOpt), buf)) GotoURL(C->SupportSite);
         }
         else ER_NewError(GetStr(MSG_ER_CantOpenTempfile), tf->Filename, NULL);
      }
      CloseTempFile(tf);
      BusyEnd;
      TR_CloseTCPIP();
   }
   else ER_NewError(GetStr(MSG_ER_NoTCP), NULL, NULL);
}
MakeStaticHook(MA_CheckVersionHook, MA_CheckVersionFunc);

///
/// MA_ShowErrorsFunc
//  Opens error message window
HOOKPROTONHNONP(MA_ShowErrorsFunc, void)
{
   ER_NewError(NULL, NULL, NULL);
}
MakeStaticHook(MA_ShowErrorsHook, MA_ShowErrorsFunc);

///
/// MA_StartMacro
//  Launches user-defined ARexx script or AmigaDOS command
BOOL MA_StartMacro(enum Macro num, char *param)
{
   BPTR fh;
   char command[SIZE_LARGE], *wtitle = "CON:////YAM ARexx Window/AUTO";
   struct RexxMsg *sentrm;

   strcpy(command, C->RX[num].Script);
   if (!*command) return 0;
   if (param) { strcat(command, " "); strcat(command, param); }
   if (C->RX[num].IsAmigaDOS)
   {
      BusyText(GetStr(MSG_MA_EXECUTINGCMD), "");
      ExecuteCommand(command, !C->RX[num].WaitTerm, C->RX[num].UseConsole ? OUT_DOS : OUT_NIL);
      BusyEnd;
   }
   else
   {
      if (!(fh = Open(C->RX[num].UseConsole ? wtitle : "NIL:", MODE_NEWFILE)))
      {
         ER_NewError(GetStr(MSG_ER_ErrorConsole), NULL, NULL);
         return FALSE;
      }
      if (!(sentrm = SendRexxCommand(G->RexxHost, command, fh)))
      {
         Close(fh);
         ER_NewError(GetStr(MSG_ER_ErrorARexxScript), command, NULL);
         return FALSE;
      }
      if (C->RX[num].WaitTerm)
      {
         extern void DoRXCommand( struct RexxHost *, struct RexxMsg *);
         struct RexxMsg *rm;
         BOOL waiting = TRUE;
         BusyText(GetStr(MSG_MA_EXECUTINGCMD), "");
         do
         {
            WaitPort(G->RexxHost->port);
            while ((rm = (struct RexxMsg *)GetMsg(G->RexxHost->port)))
            {
               if ((rm->rm_Action & RXCODEMASK) != RXCOMM) ReplyMsg((struct Message *)rm);
               else if (rm->rm_Node.mn_Node.ln_Type == NT_REPLYMSG)
               {
                  struct RexxMsg *org = (struct RexxMsg *)rm->rm_Args[15];
                  if (org)
                  {
                     if (rm->rm_Result1) ReplyRexxCommand(org, 20, ERROR_NOT_IMPLEMENTED, NULL);
                     else ReplyRexxCommand(org, 0, 0, (char *)rm->rm_Result2);
                  }
                  if (rm == sentrm) waiting = FALSE;
                  FreeRexxCommand(rm);
                  --G->RexxHost->replies;
               }
               else if (rm->rm_Args[0]) DoRXCommand(G->RexxHost, rm);
               else ReplyMsg((struct Message *)rm);
            }
         }
         while (waiting);
         BusyEnd;
      }
   }
   return TRUE;
}

///
/// MA_CallRexxFunc
//  Launches a script from the ARexx menu
HOOKPROTONHNO(MA_CallRexxFunc, void, int *arg)
{
   char scname[SIZE_COMMAND];
   int script = *arg;
   if (script >= 0)
   {                 
      MA_StartMacro(MACRO_MEN0+script, NULL);
   }
   else
   {
      strmfp(scname, G->ProgDir, "rexx");
      if (ReqFile(ASL_REXX, G->MA->GUI.WI, GetStr(MSG_MA_ExecuteScript), 0, scname, ""))
      {
         strmfp(scname, G->ASLReq[ASL_REXX]->fr_Drawer, G->ASLReq[ASL_REXX]->fr_File);
         SendRexxCommand(G->RexxHost, scname, 0);
      }
   }
}
MakeStaticHook(MA_CallRexxHook, MA_CallRexxFunc);
///

/*** Hooks ***/

/// PO_Window
/*** PO_Window - Window hook for popup objects ***/
HOOKPROTONH(PO_Window, void, Object *pop, Object *win)
{
   set(win, MUIA_Window_DefaultObject, pop);
}
MakeHook(PO_WindowHook, PO_Window);

///
/// MA_LV_FConFunc
/*** MA_LV_FConFunc - Folder listview construction hook ***/
HOOKPROTONHNO(MA_LV_FConFunc, struct Folder *, struct MUIP_NListtree_ConstructMessage *msg)
{
   struct Folder *entry;

   if(!msg) return(NULL);

   entry = calloc(1, sizeof(struct Folder));
   memcpy(entry, msg->UserData, sizeof(struct Folder));

   return(entry);
}
MakeStaticHook(MA_LV_FConHook, MA_LV_FConFunc);

///
/// MA_LV_FDesFunc
/*** MA_LV_FDesFunc - Folder listtree destruction hook ***/
HOOKPROTONHNO(MA_LV_FDesFunc, LONG, struct MUIP_NListtree_DestructMessage *msg)
{
   if(!msg) return(-1);
   if(msg->UserData) FO_FreeFolder((struct Folder *)msg->UserData);
   return(0);
}
MakeStaticHook(MA_LV_FDesHook, MA_LV_FDesFunc);

///
/// FindAddressHook()
HOOKPROTONH(FindAddressFunc, LONG, Object *obj, struct MUIP_NListtree_FindUserDataMessage *msg)
{
	struct ABEntry *entry = (struct ABEntry *)msg->UserData;
	return Stricmp(msg->User, entry->Address);
}
MakeStaticHook(FindAddressHook, FindAddressFunc);

///
/// MA_LV_DspFunc
/*** MA_LV_DspFunc - Message listview display hook ***/
HOOKPROTO(MA_LV_DspFunc, long, char **array, struct Mail *entry)
{
   BOOL outbox;
   struct Folder *folder;
   int type;

   if (G->MA && (folder = FO_GetCurrentFolder())) type = folder->Type;
   else return 0;

   outbox = OUTGOING(type);
   if (entry)
   {
      if (folder)
      {
         static char dispfro[SIZE_DEFAULT], dispsta[SIZE_DEFAULT], dispsiz[SIZE_SMALL];
         sprintf(array[0] = dispsta, "\033o[%ld]", entry->Status);
         if (entry->Importance == 1) strcat(dispsta, "\033o[12]");
         if (entry->Flags & MFLAG_CRYPT) strcat(dispsta, "\033o[15]");
         else if (entry->Flags & MFLAG_SIGNED) strcat(dispsta, "\033o[16]");
         else if (entry->Flags & MFLAG_REPORT) strcat(dispsta, "\033o[14]");
         else if (entry->Flags & MFLAG_MULTIPART) strcat(dispsta, "\033o[13]");

#ifndef DISABLE_ADDRESSBOOK_LOOKUP
{
         struct Person *pe;
			struct MUI_NListtree_TreeNode *tn;
			STRPTR multiple, to, addr;

			multiple = entry->Flags & MFLAG_MULTIRCPT ? "\033o[11]" : "";
			pe = outbox ? &entry->To : &entry->From;
			to = (type == FT_CUSTOMMIXED || type == FT_DELETED) && !Stricmp(pe->Address, C->EmailAddress) ? (pe = &entry->To, GetStr(MSG_MA_ToPrefix)) : "";

			set(G->AB->GUI.LV_ADDRESSES, MUIA_NListtree_FindUserDataHook, &FindAddressHook);
         if(tn = (struct MUI_NListtree_TreeNode *)DoMethod(G->AB->GUI.LV_ADDRESSES, MUIM_NListtree_FindUserData, MUIV_NListtree_FindUserData_ListNode_Root, &pe->Address[0], 0))
					addr = ((struct ABEntry *)tn->tn_User)->RealName;
			else	addr = AddrName((*pe));

			sprintf(dispfro, "%s%s%s", multiple, to, addr);
			array[1] = dispfro;
}
#else
         array[1] = dispfro; *dispfro = 0;
         if (entry->Flags & MFLAG_MULTIRCPT) strcat(dispfro, "\033o[11]");
         pe = outbox ? &entry->To : &entry->From;
         if (type == FT_CUSTOMMIXED || type == FT_DELETED) if (!stricmp(pe->Address, C->EmailAddress))
         {
            pe = &entry->To; strcat(dispfro, GetStr(MSG_MA_ToPrefix));
         }
         strncat(dispfro, AddrName((*pe)), SIZE_DEFAULT-strlen(dispfro)-1);
#endif

         array[2] = AddrName((entry->ReplyTo));
         array[3] = entry->Subject;
         array[4] = DateStamp2String(&entry->Date, C->SwatchBeat ? DSS_DATEBEAT : DSS_DATETIME);
         array[5] = dispsiz; *dispsiz = 0;
         FormatSize(entry->Size, dispsiz);
         array[6] = entry->MailFile;
         array[7] = entry->Folder->Name;
      }
   }
   else
   {
      array[0] = GetStr(MSG_MA_TitleStatus);
      array[1] = GetStr(outbox ? MSG_To : MSG_From);
      array[2] = GetStr(MSG_ReturnAddress);
      array[3] = GetStr(MSG_Subject);
      array[4] = GetStr(MSG_Date);
      array[5] = GetStr(MSG_Size);
      array[6] = GetStr(MSG_Filename);
      array[7] = GetStr(MSG_Folder);
   }
   return 0;
}
MakeHook(MA_LV_DspFuncHook,MA_LV_DspFunc);

///
/// MA_GetRealSubject
//  Strips reply prefix / mailing list name from subject
static char *MA_GetRealSubject(char *sub)
{
   char *p, *pend = &sub[strlen(sub)];
   if (strlen(sub) < 3) return sub;
   if (sub[2] == ':' && !sub[3]) return "";
   if (sub[0] == '[') if ((p = strchr(sub, ']'))) if (p < pend-3 && p < &sub[20]) return MA_GetRealSubject(p+2);
   if (strchr(":[({", sub[2])) if ((p = strchr(sub, ':'))) return MA_GetRealSubject(TrimStart(++p));
   return sub;
}

///
/// MA_MailCompare
//  Compares two messages
static int MA_MailCompare(struct Mail *entry1, struct Mail *entry2, int column)
{
   static const int values[9] = { 50, 35, 30, 25, 45, 60, 40, 20, 55 };

   switch (column)
   {
      case 0: return -(values[entry1->Status]+entry1->Importance)+(values[entry2->Status]+entry2->Importance);
      case 1: if (OUTGOING(entry1->Folder->Type))
                 return stricmp(*entry1->To.RealName ? entry1->To.RealName : entry1->To.Address,
                                *entry2->To.RealName ? entry2->To.RealName : entry2->To.Address);
              else
                 return stricmp(*entry1->From.RealName ? entry1->From.RealName : entry1->From.Address,
                                *entry2->From.RealName ? entry2->From.RealName : entry2->From.Address);
      case 2: return stricmp(*entry1->ReplyTo.RealName ? entry1->ReplyTo.RealName : entry1->ReplyTo.Address,
                             *entry2->ReplyTo.RealName ? entry2->ReplyTo.RealName : entry2->ReplyTo.Address);
      case 3: return stricmp(MA_GetRealSubject(entry1->Subject), MA_GetRealSubject(entry2->Subject));
      case 4: return CompareDates(&entry2->Date, &entry1->Date);
      case 5: return entry1->Size-entry2->Size;
      case 6: return strcmp(entry1->MailFile, entry2->MailFile);
      case 7: return stricmp(entry1->Folder->Name, entry2->Folder->Name);
   }
   return 0;
}

///
/// MA_LV_Cmp2Func
//  Message listview sort hook
HOOKPROTONH(MA_LV_Cmp2Func, long, Object *obj, struct NList_CompareMessage *ncm)
{
   struct Mail *entry1 = (struct Mail *)ncm->entry1;
   struct Mail *entry2 = (struct Mail *)ncm->entry2;
   long col1 = ncm->sort_type & MUIV_NList_TitleMark_ColMask;
   long col2 = ncm->sort_type2 & MUIV_NList_TitleMark2_ColMask;
   int cmp;

   if (ncm->sort_type == MUIV_NList_SortType_None) return 0;
   if (ncm->sort_type & MUIV_NList_TitleMark_TypeMask) cmp = MA_MailCompare(entry2, entry1, col1);
   else                                                cmp = MA_MailCompare(entry1, entry2, col1);
   if (cmp || col1 == col2) return cmp;
   if (ncm->sort_type2 & MUIV_NList_TitleMark2_TypeMask) cmp = MA_MailCompare(entry2, entry1, col2);
   else                                                  cmp = MA_MailCompare(entry1, entry2, col2);
   return cmp;
}
MakeHook(MA_LV_Cmp2Hook, MA_LV_Cmp2Func);

///
/// MA_LV_FCmp2Func
//  Folder listtree sort hook
/*
HOOKPROTONH(MA_LV_FCmp2Func, long, Object *obj, struct MUIP_NListtree_CompareMessage *ncm)
{
   struct Folder *entry1 = (struct Folder *)ncm->TreeNode1->tn_User;
   struct Folder *entry2 = (struct Folder *)ncm->TreeNode2->tn_User;
   int cmp = 0;

   if (ncm->SortType != MUIV_NList_SortType_None)
   {
      switch (ncm->SortType & MUIV_NList_TitleMark_ColMask)
      {
         case 0:  cmp = stricmp(entry1->Name, entry2->Name); break;
         case 1:  cmp = entry1->Total-entry2->Total; break;
         case 2:  cmp = entry1->Unread-entry2->Unread; break;
         case 3:  cmp = entry1->New-entry2->New; break;
         case 4:  cmp = entry1->Size-entry2->Size; break;
         case 10: return entry1->SortIndex-entry2->SortIndex;
      }
      if (ncm->SortType & MUIV_NList_TitleMark_TypeMask) cmp = -cmp;
   }

   return cmp;
}
MakeStaticHook(MA_LV_FCmp2Hook, MA_LV_FCmp2Func);
*/
///
/// MA_FolderKeyFunc
//  If the user pressed 0-9 we jump to folder 1-10
HOOKPROTONHNO(MA_FolderKeyFunc, void, int *idx)
{
  struct MUI_NListtree_TreeNode *tn = NULL;
  int i, count = idx[0];

  // we get the first entry and if it`s a LIST we have to get the next one
  // and so on, until we have a real entry for that key or we set nothing active
  for(i=0; i <= count; i++)
  {
    tn = (struct MUI_NListtree_TreeNode *)DoMethod(G->MA->GUI.NL_FOLDERS, MUIM_NListtree_GetEntry, MUIV_NListtree_GetEntry_ListNode_Root, i, MUIF_NONE);
    if(!tn) return;

    if(tn->tn_Flags & TNF_LIST) count++;
  }

  // Force that the list is open at this entry
  DoMethod(G->MA->GUI.NL_FOLDERS, MUIM_NListtree_Open, MUIV_NListtree_Open_ListNode_Parent, tn, MUIF_NONE);

  // Now set this treenode activ
  set(G->MA->GUI.NL_FOLDERS, MUIA_NListtree_Active, tn);
}
MakeHook(MA_FolderKeyHook, MA_FolderKeyFunc);

///

/*** GUI ***/
enum { MMEN_ABOUT=1,MMEN_ABOUTMUI,MMEN_VERSION,MMEN_ERRORS,MMEN_LOGIN,MMEN_HIDE,MMEN_QUIT,
       MMEN_NEWF,MMEN_NEWFG,MMEN_EDITF,MMEN_DELETEF,MMEN_OSAVE,MMEN_ORESET,MMEN_SELALL,MMEN_SELNONE,MMEN_SELTOGG,MMEN_SEARCH,MMEN_FILTER,MMEN_DELDEL,MMEN_INDEX,MMEN_FLUSH,MMEN_IMPORT,MMEN_EXPORT,MMEN_GETMAIL,MMEN_GET1MAIL,MMEN_SENDMAIL,MMEN_EXMAIL,
       MMEN_READ,MMEN_EDIT,MMEN_MOVE,MMEN_COPY,MMEN_DELETE,MMEN_PRINT,MMEN_SAVE,MMEN_DETACH,MMEN_CROP,MMEN_EXPMSG,MMEN_NEW,MMEN_REPLY,MMEN_FORWARD,MMEN_BOUNCE,MMEN_SAVEADDR,MMEN_TOUNREAD,MMEN_TOREAD,MMEN_TOHOLD,MMEN_TOQUEUED,MMEN_CHSUBJ,MMEN_SEND,
       MMEN_ABOOK,MMEN_CONFIG,MMEN_USER,MMEN_MUI,MMEN_SCRIPT };
#define MMEN_MACRO   100
#define MMEN_POPHOST 150

/// MA_SetupDynamicMenus
//  Updates ARexx and POP3 account menu items
void MA_SetupDynamicMenus(void)
{
   static const char *shortcuts[10] = { "0","1","2","3","4","5","6","7","8","9" };
   int i;

   /* Scripts menu */

   if (G->MA->GUI.MN_REXX)
      DoMethod(G->MA->GUI.MS_MAIN, MUIM_Family_Remove, G->MA->GUI.MN_REXX);

   G->MA->GUI.MN_REXX = MenuObject,
      MUIA_Menu_Title, GetStr(MSG_MA_Scripts),
      MUIA_Family_Child, MenuitemObject,
         MUIA_Menuitem_Title, GetStr(MSG_MA_ExecuteScript),
         MUIA_Menuitem_Shortcut, ".", MUIA_UserData, MMEN_SCRIPT,
         End,
      MUIA_Family_Child, MenuitemObject,
         MUIA_Menuitem_Title, NM_BARLABEL,
         End,
      End;

   for (i = 0; i < 10; i++) if (C->RX[i].Script[0])
      DoMethod(G->MA->GUI.MN_REXX, MUIM_Family_AddTail, MenuitemObject,
         MUIA_Menuitem_Title, C->RX[i].Name,
         MUIA_Menuitem_Shortcut, shortcuts[i],
         MUIA_UserData, MMEN_MACRO+i, End);

   DoMethod(G->MA->GUI.MS_MAIN, MUIM_Family_AddTail, G->MA->GUI.MN_REXX);

   /* 'Folder/Check single account' menu */

   if (G->MA->GUI.MI_CSINGLE)
      DoMethod(G->MA->GUI.MN_FOLDER, MUIM_Family_Remove, G->MA->GUI.MI_CSINGLE);

   G->MA->GUI.MI_CSINGLE = MenuitemObject,
      MUIA_Menuitem_Title, GetStr(MSG_MA_CheckSingle),
      End;

   for (i = 0; i < MAXP3; i++) if (C->P3[i])
   {
      sprintf(C->P3[i]->Account, "%s@%s", C->P3[i]->User, C->P3[i]->Server);

      /* Warning: Small memory leak here, each time this function is called,
                  since the strdup()'ed string doesn't get free()'d anywhere,
                  before program exit. The Menuitem class does *not* have a
                  private buffer for the string!
      */

      DoMethod(G->MA->GUI.MI_CSINGLE, MUIM_Family_AddTail, MenuitemObject, MUIA_Menuitem_Title, strdup(C->P3[i]->Account), MUIA_UserData,MMEN_POPHOST+i, End, TAG_DONE);

   }
   DoMethod(G->MA->GUI.MN_FOLDER, MUIM_Family_AddTail, G->MA->GUI.MI_CSINGLE, TAG_DONE);
}

///
/// MA_MailListContextMenu
//  Creates a ContextMenu for the folder Listtree
ULONG MA_MailListContextMenu(struct MUIP_ContextMenuBuild *msg)
{
  struct MUI_NList_TestPos_Result res;
  struct Mail *mail = NULL;
  struct PopupMenu *pop_menu;
  struct Window *win;
  struct MA_GUIData *gui = &G->MA->GUI;
  struct Folder *fo = FO_GetCurrentFolder();
  BOOL beingedited = FALSE, hasattach = FALSE;
  BOOL nomail = FALSE;
  ULONG  ret;
  int i;

  enum{ PMN_READ=1, PMN_EDIT, PMN_REPLY, PMN_FORWARD, PMN_BOUNCE, PMN_SAVEADDR, PMN_MOVE, PMN_COPY,
        PMN_DELETE, PMN_PRINT, PMN_SAVE, PMN_DETACH, PMN_CROP, PMN_EXPMSG, PMN_NEW, PMN_SELALL,
        PMN_SELNONE, PMN_SELTOGG, PMN_CHSUBJ, PMN_TOUNREAD, PMN_TOREAD, PMN_TOHOLD, PMN_TOQUEUED,
        PMN_SEND
      };

  if (!fo) return(0);

  // Now lets find out which entry is under the mouse pointer
  DoMethod(gui->NL_MAILS, MUIM_NList_TestPos, msg->mx, msg->my, &res);

  if(res.entry == -1) nomail = TRUE;
  else
  {
      DoMethod(gui->NL_MAILS, MUIM_NList_GetEntry, res.entry, &mail);

      if(!mail) return(0);

      fo->LastActive = xget(gui->NL_MAILS, MUIA_NList_Active);

      if (mail->Flags & MFLAG_MULTIPART) hasattach = TRUE;

      for (i = 0; i < MAXWR; i++)
      {
         if (G->WR[i] && G->WR[i]->Mail == mail) beingedited = TRUE;
      }

      // Now we set this entry as activ
      if(fo->LastActive != res.entry) set(gui->NL_MAILS, MUIA_NList_Active, res.entry);
  }

  // Get the window structure of the window which this listtree belongs to
  get(_win(gui->NL_MAILS), MUIA_Window_Window, &win);
  if(!win) return(0);

  // We create the PopupMenu now
  pop_menu =   PMMenu(mail ? mail->MailFile : GetStr(MSG_MAIL_NONSEL)),
                 PMItem(GetStripStr(MSG_MA_MRead)),            PM_Disabled, nomail,        PM_UserData, PMN_READ,      End,
                 PMItem(GetStripStr(MSG_MESSAGE_EDIT)),        PM_Disabled, nomail || !OUTGOING(fo->Type) || beingedited, PM_UserData, PMN_EDIT, End,
                 PMItem(GetStripStr(MSG_MESSAGE_REPLY)),       PM_Disabled, nomail,        PM_UserData, PMN_REPLY,     End,
                 PMItem(GetStripStr(MSG_MESSAGE_FORWARD)),     PM_Disabled, nomail,        PM_UserData, PMN_FORWARD,   End,
                 PMItem(GetStripStr(MSG_MESSAGE_BOUNCE)),      PM_Disabled, nomail,        PM_UserData, PMN_BOUNCE,    End,
                 PMItem(GetStripStr(MSG_MA_MSend)),            PM_Disabled, nomail || (fo->Type != FT_OUTGOING), PM_UserData, PMN_SEND,      End,
                 PMItem(GetStripStr(MSG_MA_ChangeSubj)),       PM_Disabled, nomail,        PM_UserData, PMN_CHSUBJ,    End,
                 PMItem(GetStripStr(MSG_MA_SetStatus)),        PM_Disabled, nomail, PMSimpleSub,
                   PMItem(GetStripStr(MSG_MA_ToUnread)),       PM_Disabled, nomail || OUTGOING(fo->Type),   PM_UserData, PMN_TOUNREAD,  End,
                   PMItem(GetStripStr(MSG_MA_ToRead)),         PM_Disabled, nomail || OUTGOING(fo->Type),   PM_UserData, PMN_TOREAD,    End,
                   PMItem(GetStripStr(MSG_MA_ToHold)),         PM_Disabled, nomail || !OUTGOING(fo->Type),  PM_UserData, PMN_TOHOLD,    End,
                   PMItem(GetStripStr(MSG_MA_ToQueued)),       PM_Disabled, nomail || !OUTGOING(fo->Type),  PM_UserData, PMN_TOQUEUED,  End,
                   End,
                 End,
                 PMBar, End,
                 PMItem(GetStripStr(MSG_MESSAGE_GETADDRESS)),  PM_Disabled, nomail, PM_UserData, PMN_SAVEADDR, End,
                 PMItem(GetStripStr(MSG_MESSAGE_MOVE)),        PM_Disabled, nomail, PM_UserData, PMN_MOVE,     End,
                 PMItem(GetStripStr(MSG_MESSAGE_COPY)),        PM_Disabled, nomail, PM_UserData, PMN_COPY,     End,
                 PMItem(GetStripStr(MSG_MA_MDelete)),          PM_Disabled, nomail, PM_UserData, PMN_DELETE,   End,
                 PMBar, End,
                 PMItem(GetStripStr(MSG_MESSAGE_PRINT)),       PM_Disabled, nomail, PM_UserData, PMN_PRINT,   End,
                 PMItem(GetStripStr(MSG_MESSAGE_SAVE)),        PM_Disabled, nomail, PM_UserData, PMN_SAVE,    End,
                 PMItem(GetStripStr(MSG_Attachments)),         PM_Disabled, nomail || !hasattach, PMSimpleSub,
                   PMItem(GetStripStr(MSG_MESSAGE_SAVEATT)),   PM_Disabled, nomail || !hasattach,  PM_UserData, PMN_DETACH,  End,
                   PMItem(GetStripStr(MSG_MESSAGE_CROP)),      PM_Disabled, nomail || !hasattach,  PM_UserData, PMN_CROP,    End,
                   End,
                 End,
                 PMItem(GetStripStr(MSG_MESSAGE_EXPORT)),      PM_Disabled, nomail, PM_UserData, PMN_EXPMSG, End,
                 PMBar, End,
                 PMItem(GetStripStr(MSG_MESSAGE_NEW)),         PM_UserData, PMN_NEW,     End,
                 PMItem(GetStripStr(MSG_MA_Select)),           PMSimpleSub,
                   PMItem(GetStripStr(MSG_MA_SelectAll)),      PM_UserData, PMN_SELALL,  End,
                   PMItem(GetStripStr(MSG_MA_SelectNone)),     PM_UserData, PMN_SELNONE, End,
                   PMItem(GetStripStr(MSG_MA_SelectToggle)),   PM_UserData, PMN_SELTOGG, End,
                   End,
                 End,
               End;

  ret = (ULONG)(PM_OpenPopupMenu(win, PM_Menu,pop_menu, TAG_DONE));

  PM_FreePopupMenu(pop_menu);

  switch(ret)
  {
    case PMN_READ:     DoMethod(G->App, MUIM_CallHook, &MA_ReadMessageHook,    TAG_DONE); break;
    case PMN_EDIT:     DoMethod(G->App, MUIM_CallHook, &MA_NewMessageHook,     NEW_EDIT,    0,   FALSE,  TAG_DONE); break;
    case PMN_REPLY:    DoMethod(G->App, MUIM_CallHook, &MA_NewMessageHook,     NEW_REPLY,   0,   FALSE,  TAG_DONE); break;
    case PMN_FORWARD:  DoMethod(G->App, MUIM_CallHook, &MA_NewMessageHook,     NEW_FORWARD, 0,   FALSE,  TAG_DONE); break;
    case PMN_BOUNCE:   DoMethod(G->App, MUIM_CallHook, &MA_NewMessageHook,     NEW_BOUNCE,  0,   FALSE,  TAG_DONE); break;
    case PMN_SEND:     DoMethod(G->App, MUIM_CallHook, &MA_SendHook,           SEND_ACTIVE, TAG_DONE); break;
    case PMN_CHSUBJ:   DoMethod(G->App, MUIM_CallHook, &MA_ChangeSubjectHook,  TAG_DONE); break;
    case PMN_TOUNREAD: DoMethod(G->App, MUIM_CallHook, &MA_SetStatusToHook,    STATUS_UNR, TAG_DONE); break;
    case PMN_TOREAD:   DoMethod(G->App, MUIM_CallHook, &MA_SetStatusToHook,    STATUS_OLD, TAG_DONE); break;
    case PMN_TOHOLD:   DoMethod(G->App, MUIM_CallHook, &MA_SetStatusToHook,    STATUS_HLD, TAG_DONE); break;
    case PMN_TOQUEUED: DoMethod(G->App, MUIM_CallHook, &MA_SetStatusToHook,    STATUS_WFS, TAG_DONE); break;
    case PMN_SAVEADDR: DoMethod(G->App, MUIM_CallHook, &MA_GetAddressHook,     TAG_DONE); break;
    case PMN_MOVE:     DoMethod(G->App, MUIM_CallHook, &MA_MoveMessageHook,    TAG_DONE); break;
    case PMN_COPY:     DoMethod(G->App, MUIM_CallHook, &MA_CopyMessageHook,    TAG_DONE); break;
    case PMN_DELETE:   DoMethod(G->App, MUIM_CallHook, &MA_DeleteMessageHook,  0, FALSE, TAG_DONE); break;
    case PMN_PRINT:    DoMethod(G->App, MUIM_CallHook, &MA_SavePrintHook,      TRUE,  TAG_DONE); break;
    case PMN_SAVE:     DoMethod(G->App, MUIM_CallHook, &MA_SavePrintHook,      FALSE, TAG_DONE); break;
    case PMN_DETACH:   DoMethod(G->App, MUIM_CallHook, &MA_SaveAttachHook,     TAG_DONE); break;
    case PMN_CROP:     DoMethod(G->App, MUIM_CallHook, &MA_RemoveAttachHook,   TAG_DONE); break;
    case PMN_EXPMSG:   DoMethod(G->App, MUIM_CallHook, &MA_ExportMessagesHook, TAG_DONE); break;
    case PMN_NEW:      DoMethod(G->App, MUIM_CallHook, &MA_NewMessageHook,     NEW_NEW,  0,  FALSE,  TAG_DONE); break;
    case PMN_SELALL:   DoMethod(gui->NL_MAILS, MUIM_NList_Select, MUIV_NList_Select_All, MUIV_NList_Select_On,     NULL, TAG_DONE); break;
    case PMN_SELNONE:  DoMethod(gui->NL_MAILS, MUIM_NList_Select, MUIV_NList_Select_All, MUIV_NList_Select_Off,    NULL, TAG_DONE); break;
    case PMN_SELTOGG:  DoMethod(gui->NL_MAILS, MUIM_NList_Select, MUIV_NList_Select_All, MUIV_NList_Select_Toggle, NULL, TAG_DONE); break;
  }

  return(0);
}

///
/// MA_SortWindow
//  Resorts the main window group accordingly to the InfoBar setting
BOOL MA_SortWindow(void)
{
  BOOL showbar = TRUE;

  DoMethod(G->MA->GUI.GR_MAIN, MUIM_Group_InitChange);

  switch(C->InfoBar)
  {
    case IB_POS_TOP:
    {
      DoMethod(G->MA->GUI.GR_MAIN, MUIM_Group_Sort, G->MA->GUI.IB_INFOBAR, G->MA->GUI.GR_TOP, G->MA->GUI.BC_GROUP, G->MA->GUI.GR_BOTTOM, NULL);
    }
    break;

    case IB_POS_CENTER:
    {
      DoMethod(G->MA->GUI.GR_MAIN, MUIM_Group_Sort, G->MA->GUI.GR_TOP, G->MA->GUI.BC_GROUP, G->MA->GUI.IB_INFOBAR, G->MA->GUI.GR_BOTTOM, NULL);
    }
    break;

    case IB_POS_BOTTOM:
    {
      DoMethod(G->MA->GUI.GR_MAIN, MUIM_Group_Sort, G->MA->GUI.GR_TOP, G->MA->GUI.BC_GROUP, G->MA->GUI.GR_BOTTOM, G->MA->GUI.IB_INFOBAR, NULL);
    }
    break;

    default:
    {
      showbar = FALSE;
    }
  }

  // Here we can do a MUIA_ShowMe, TRUE because ResortWindow is encapsulated
  // in a InitChange/ExitChange..
  set(G->MA->GUI.IB_INFOBAR, MUIA_ShowMe, showbar);

  DoMethod(G->MA->GUI.GR_MAIN, MUIM_Group_ExitChange);

  return TRUE;
}
///
/// MA_MakeMAFormat
//  Creates format definition for message listview
void MA_MakeMAFormat(APTR lv)
{
   static const int defwidth[MACOLNUM] = { -1,-1,-1,-1,-1,-1,-1 };
   char format[SIZE_LARGE];
   BOOL first = TRUE;
   int i;

   *format = 0;
   for (i = 0; i < MACOLNUM; i++) if (C->MessageCols & (1<<i))
   {
      if (first) first = FALSE; else strcat(format, " BAR,");
      sprintf(&format[strlen(format)], "COL=%ld W=%ld", i, defwidth[i]);
      if (i == 5) strcat(format, " P=\033r");
   }
   strcat(format, " BAR");
   set(lv, MUIA_NList_Format, format);
}

///

/// MA_New
//  Creates main window
struct MA_ClassData *MA_New(void)
{
   struct MA_ClassData *data = calloc(1, sizeof(struct MA_ClassData));
   if (data)
   {
      static const struct NewToolbarEntry tb_butt[ARRAY_SIZE(data->GUI.TB_TOOLBAR)] = {
        { MSG_MA_TBRead,     MSG_HELP_MA_BT_READ       },
        { MSG_MA_TBEdit,     MSG_HELP_MA_BT_EDIT       },
        { MSG_MA_TBMove,     MSG_HELP_MA_BT_MOVE       },
        { MSG_MA_TBDelete,   MSG_HELP_MA_BT_DELETE     },
        { MSG_MA_TBGetAddr,  MSG_HELP_MA_BT_GETADDRESS },
        { MSG_Space,         NULL                      },
        { MSG_MA_TBWrite,    MSG_HELP_MA_BT_WRITE      },
        { MSG_MA_TBReply,    MSG_HELP_MA_BT_REPLY      },
        { MSG_MA_TBForward,  MSG_HELP_MA_BT_FORWARD    },
        { MSG_Space,         NULL                      },
        { MSG_MA_TBGetMail,  MSG_HELP_MA_BT_POPNOW     },
        { MSG_MA_TBSendAll,  MSG_HELP_MA_BT_SENDALL    },
        { MSG_Space,         NULL                      },
        { MSG_MA_TBFilter,   MSG_HELP_MA_BT_FILTER     },
        { MSG_MA_TBFind,     MSG_HELP_MA_BT_SEARCH     },
        { MSG_MA_TBAddrBook, MSG_HELP_MA_BT_ABOOK      },
        { MSG_MA_TBConfig,   MSG_HELP_MA_BT_CONFIG     },
        { NULL,              NULL                      }
      };
      char *key = "-repeat 0", *username;
      struct User *user;
      int i;

      for (i = 0; i < ARRAY_SIZE(data->GUI.TB_TOOLBAR); i++)
        SetupToolbar(&(data->GUI.TB_TOOLBAR[i]), tb_butt[i].label?(tb_butt[i].label==MSG_Space?"":GetStr(tb_butt[i].label)):NULL, tb_butt[i].help?GetStr(tb_butt[i].help):NULL, 0);

      if (username = C->RealName,(user = US_GetCurrentUser()))
        username = user->Name;

      sprintf(data->WinTitle, GetStr(MSG_MA_WinTitle), yamversionver, username);

      data->GUI.MS_MAIN = MenustripObject,
         MUIA_Family_Child, MenuObject, MUIA_Menu_Title, GetStr(MSG_MA_Project),
            MUIA_Family_Child, MakeMenuitem(GetStr(MSG_PROJECT_ABOUT), MMEN_ABOUT),
            MUIA_Family_Child, MakeMenuitem(GetStr(MSG_MA_AboutMUI), MMEN_ABOUTMUI),
            MUIA_Family_Child, MakeMenuitem(GetStr(MSG_PROJECT_CHECKVERSION), MMEN_VERSION),
            MUIA_Family_Child, data->GUI.MI_ERRORS = MakeMenuitem(GetStr(MSG_MA_LastErrors), MMEN_ERRORS),
            MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL, End,
            MUIA_Family_Child, MakeMenuitem(GetStr(MSG_MA_Restart), MMEN_LOGIN),
            MUIA_Family_Child, MakeMenuitem(GetStr(MSG_MA_Hide), MMEN_HIDE),
            MUIA_Family_Child, MakeMenuitem(GetStr(MSG_MA_Quit), MMEN_QUIT),
         End,
         MUIA_Family_Child, data->GUI.MN_FOLDER = MenuObject, MUIA_Menu_Title, GetStr(MSG_Folder),
            MUIA_Family_Child, MakeMenuitem(GetStr(MSG_FOLDER_NEWFOLDER), MMEN_NEWF),
            MUIA_Family_Child, MakeMenuitem(GetStr(MSG_FOLDER_NEWFOLDERGROUP), MMEN_NEWFG),
            MUIA_Family_Child, MakeMenuitem(GetStr(MSG_FOLDER_EDIT), MMEN_EDITF),
            MUIA_Family_Child, MakeMenuitem(GetStr(MSG_FOLDER_DELETE), MMEN_DELETEF),
            MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, GetStr(MSG_MA_SortOrder),
               MUIA_Family_Child, MakeMenuitem(GetStr(MSG_MA_OSave), MMEN_OSAVE),
               MUIA_Family_Child, MakeMenuitem(GetStr(MSG_MA_Reset), MMEN_ORESET),
            End,
            MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL, End,
            MUIA_Family_Child, MakeMenuitem(GetStr(MSG_MA_MSearch), MMEN_SEARCH),
            MUIA_Family_Child, MakeMenuitem(GetStr(MSG_MA_MFilter), MMEN_FILTER),
            MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL, End,
            MUIA_Family_Child, MakeMenuitem(GetStr(MSG_MA_RemoveDeleted), MMEN_DELDEL),
            MUIA_Family_Child, MakeMenuitem(GetStr(MSG_MA_UpdateIndex), MMEN_INDEX),
            MUIA_Family_Child, MakeMenuitem(GetStr(MSG_MA_FlushIndices), MMEN_FLUSH),
            MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL, End,
            MUIA_Family_Child, data->GUI.MI_IMPORT = MakeMenuitem(GetStr(MSG_FOLDER_IMPORT), MMEN_IMPORT),
            MUIA_Family_Child, data->GUI.MI_EXPORT = MakeMenuitem(GetStr(MSG_FOLDER_EXPORT), MMEN_EXPORT),
            MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL, End,
            MUIA_Family_Child, data->GUI.MI_SENDALL = MakeMenuitem(GetStr(MSG_MA_MSendAll), MMEN_SENDMAIL),
            MUIA_Family_Child, data->GUI.MI_EXCHANGE = MakeMenuitem(GetStr(MSG_MA_Exchange), MMEN_EXMAIL),
            MUIA_Family_Child, data->GUI.MI_GETMAIL = MakeMenuitem(GetStr(MSG_MA_MGetMail), MMEN_GETMAIL),
        End,
         MUIA_Family_Child, MenuObject, MUIA_Menu_Title, GetStr(MSG_Message),
            MUIA_Family_Child, data->GUI.MI_READ = MakeMenuitem(GetStr(MSG_MA_MRead), MMEN_READ),
            MUIA_Family_Child, data->GUI.MI_EDIT = MakeMenuitem(GetStr(MSG_MESSAGE_EDIT), MMEN_EDIT),
            MUIA_Family_Child, data->GUI.MI_MOVE = MakeMenuitem(GetStr(MSG_MESSAGE_MOVE), MMEN_MOVE),
            MUIA_Family_Child, data->GUI.MI_COPY = MakeMenuitem(GetStr(MSG_MESSAGE_COPY), MMEN_COPY),
            MUIA_Family_Child, data->GUI.MI_DELETE = MenuitemObject, MUIA_Menuitem_Title, GetStr(MSG_MA_MDelete), MUIA_Menuitem_Shortcut, "Del", MUIA_Menuitem_CommandString,TRUE, MUIA_UserData, MMEN_DELETE, End,
            MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL, End,
            MUIA_Family_Child, data->GUI.MI_PRINT = MakeMenuitem(GetStr(MSG_MESSAGE_PRINT), MMEN_PRINT),
            MUIA_Family_Child, data->GUI.MI_SAVE = MakeMenuitem(GetStr(MSG_MESSAGE_SAVE), MMEN_SAVE),
            MUIA_Family_Child, data->GUI.MI_ATTACH = MenuitemObject, MUIA_Menuitem_Title, GetStr(MSG_Attachments),
               MUIA_Family_Child, data->GUI.MI_SAVEATT = MakeMenuitem(GetStr(MSG_MESSAGE_SAVEATT), MMEN_DETACH),
               MUIA_Family_Child, data->GUI.MI_REMATT = MakeMenuitem(GetStr(MSG_MESSAGE_CROP), MMEN_CROP),
            End,
            MUIA_Family_Child, data->GUI.MI_EXPMSG = MakeMenuitem(GetStr(MSG_MESSAGE_EXPORT), MMEN_EXPMSG),
            MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL, End,
            MUIA_Family_Child, MakeMenuitem(GetStr(MSG_MESSAGE_NEW), MMEN_NEW),
            MUIA_Family_Child, data->GUI.MI_REPLY = MakeMenuitem(GetStr(MSG_MESSAGE_REPLY), MMEN_REPLY),
            MUIA_Family_Child, data->GUI.MI_FORWARD = MakeMenuitem(GetStr(MSG_MESSAGE_FORWARD), MMEN_FORWARD),
            MUIA_Family_Child, data->GUI.MI_BOUNCE = MakeMenuitem(GetStr(MSG_MESSAGE_BOUNCE), MMEN_BOUNCE),
            MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL, End,
            MUIA_Family_Child, data->GUI.MI_GETADDRESS = MakeMenuitem(GetStr(MSG_MESSAGE_GETADDRESS), MMEN_SAVEADDR),
            MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, GetStr(MSG_MA_Select),
               MUIA_Family_Child, MakeMenuitem(GetStr(MSG_MA_SelectAll), MMEN_SELALL),
               MUIA_Family_Child, MakeMenuitem(GetStr(MSG_MA_SelectNone), MMEN_SELNONE),
               MUIA_Family_Child, MakeMenuitem(GetStr(MSG_MA_SelectToggle), MMEN_SELTOGG),
            End,
            MUIA_Family_Child, data->GUI.MI_STATUS = MenuitemObject, MUIA_Menuitem_Title, GetStr(MSG_MA_SetStatus),
               MUIA_Family_Child, data->GUI.MI_TOUNREAD = MakeMenuitem(GetStr(MSG_MA_ToUnread), MMEN_TOUNREAD),
               MUIA_Family_Child, data->GUI.MI_TOREAD = MakeMenuitem(GetStr(MSG_MA_ToRead), MMEN_TOREAD),
               MUIA_Family_Child, data->GUI.MI_TOHOLD = MakeMenuitem(GetStr(MSG_MA_ToHold), MMEN_TOHOLD),
               MUIA_Family_Child, data->GUI.MI_TOQUEUED = MakeMenuitem(GetStr(MSG_MA_ToQueued), MMEN_TOQUEUED),
            End,
            MUIA_Family_Child, data->GUI.MI_CHSUBJ = MakeMenuitem(GetStr(MSG_MA_ChangeSubj), MMEN_CHSUBJ),
            MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL, End,
            MUIA_Family_Child, data->GUI.MI_SEND = MakeMenuitem(GetStr(MSG_MA_MSend), MMEN_SEND),
         End,
         MUIA_Family_Child, MenuObject, MUIA_Menu_Title, GetStr(MSG_MA_Settings),
            MUIA_Family_Child, MakeMenuitem(GetStr(MSG_SETTINGS_ADDRESSBOOK), MMEN_ABOOK),
            MUIA_Family_Child, MakeMenuitem(GetStr(MSG_SETTINGS_CONFIG), MMEN_CONFIG),
            MUIA_Family_Child, MakeMenuitem(GetStr(MSG_SETTINGS_USERS), MMEN_USER),
            MUIA_Family_Child, MakeMenuitem(GetStr(MSG_SETTINGS_MUI), MMEN_MUI),
         End,
      End,
      data->GUI.WI = NewObject(CL_MainWin->mcc_Class,NULL,
         MUIA_Window_Title, data->WinTitle,
         MUIA_HelpNode, "MA_W",
         MUIA_Window_ID, MAKE_ID('M','A','I','N'),
         MUIA_Window_Menustrip, data->GUI.MS_MAIN,
         WindowContents, data->GUI.GR_MAIN = VGroup,
            Child, data->GUI.GR_TOP = (C->HideGUIElements & HIDE_TBAR) ?
               VSpace(1) :
               (HGroupV,
                  MUIA_HelpNode, "MA02",
                  Child, data->GUI.TO_TOOLBAR = ToolbarObject,
                     MUIA_Toolbar_ImageType,      MUIV_Toolbar_ImageType_File,
                     MUIA_Toolbar_ImageNormal,    "PROGDIR:Icons/Main.toolbar",
                     MUIA_Toolbar_ImageGhost,     "PROGDIR:Icons/Main_G.toolbar",
                     MUIA_Toolbar_ImageSelect,    "PROGDIR:Icons/Main_S.toolbar",
                     MUIA_Toolbar_Description,    data->GUI.TB_TOOLBAR,
                     MUIA_Toolbar_ParseUnderscore,TRUE,
                     MUIA_Font,                   MUIV_Font_Tiny,
                     MUIA_ShortHelp, TRUE,
                  End,
                  Child, HSpace(0),
               End),
            Child, data->GUI.BC_GROUP = HGroup,
               MUIA_ShowMe, FALSE,
               // Create the status flag image objects
               Child, data->GUI.BC_STAT[ 0] = MakeStatusFlag("status_unread"),
               Child, data->GUI.BC_STAT[ 1] = MakeStatusFlag("status_old"),
               Child, data->GUI.BC_STAT[ 2] = MakeStatusFlag("status_forward"),
               Child, data->GUI.BC_STAT[ 3] = MakeStatusFlag("status_reply"),
               Child, data->GUI.BC_STAT[ 4] = MakeStatusFlag("status_waitsend"),
               Child, data->GUI.BC_STAT[ 5] = MakeStatusFlag("status_error"),
               Child, data->GUI.BC_STAT[ 6] = MakeStatusFlag("status_hold"),
               Child, data->GUI.BC_STAT[ 7] = MakeStatusFlag("status_sent"),
               Child, data->GUI.BC_STAT[ 8] = MakeStatusFlag("status_new"),
               Child, data->GUI.BC_STAT[ 9] = MakeStatusFlag("status_delete"),
               Child, data->GUI.BC_STAT[10] = MakeStatusFlag("status_download"),
               Child, data->GUI.BC_STAT[11] = MakeStatusFlag("status_group"),
               Child, data->GUI.BC_STAT[12] = MakeStatusFlag("status_urgent"),
               Child, data->GUI.BC_STAT[13] = MakeStatusFlag("status_attach"),
               Child, data->GUI.BC_STAT[14] = MakeStatusFlag("status_report"),
               Child, data->GUI.BC_STAT[15] = MakeStatusFlag("status_crypt"),
               Child, data->GUI.BC_STAT[16] = MakeStatusFlag("status_signed"),
               // Create the default folder image objects
               Child, data->GUI.BC_FOLDER[0] = MakeFolderImage("folder_fold"),
               Child, data->GUI.BC_FOLDER[1] = MakeFolderImage("folder_unfold"),
               Child, data->GUI.BC_FOLDER[2] = MakeFolderImage("folder_incoming"),
               Child, data->GUI.BC_FOLDER[3] = MakeFolderImage("folder_incoming_new"),
               Child, data->GUI.BC_FOLDER[4] = MakeFolderImage("folder_outgoing"),
               Child, data->GUI.BC_FOLDER[5] = MakeFolderImage("folder_outgoing_new"),
               Child, data->GUI.BC_FOLDER[6] = MakeFolderImage("folder_deleted"),
               Child, data->GUI.BC_FOLDER[7] = MakeFolderImage("folder_deleted_new"),
               Child, data->GUI.BC_FOLDER[8] = MakeFolderImage("folder_sent"),
               Child, data->GUI.ST_LAYOUT = StringObject,
                  MUIA_ObjectID, MAKE_ID('S','T','L','A'),
                  MUIA_String_MaxLen, SIZE_DEFAULT,
               End,
            End,
            Child, data->GUI.IB_INFOBAR = InfoBarObject,
              MUIA_ShowMe,  !(C->InfoBar == IB_POS_OFF),
            End,
            Child, data->GUI.GR_BOTTOM = HGroup,
               MUIA_Group_Spacing, 1,
               Child, data->GUI.LV_FOLDERS = NListviewObject,
                  MUIA_HelpNode, "MA00",
                  MUIA_CycleChain, 1,
                  MUIA_HorizWeight, 30,
                  MUIA_Listview_DragType,  MUIV_Listview_DragType_Immediate,
                  MUIA_NListview_NList, data->GUI.NL_FOLDERS = NewObject(CL_FolderList->mcc_Class,NULL,
                     InputListFrame,
//                     MUIA_NList_MinColSortable      , 0,
//                     MUIA_NList_TitleClick          , TRUE,
                     MUIA_NList_DragType            , MUIV_NList_DragType_Immediate,
                     MUIA_NList_DragSortable        , TRUE,
//                     MUIA_NListtree_CompareHook     , &MA_LV_FCmp2Hook,
                     MUIA_NListtree_DisplayHook     , &MA_LV_FDspFuncHook,
                     MUIA_NListtree_ConstructHook   , &MA_LV_FConHook,
                     MUIA_NListtree_DestructHook    , &MA_LV_FDesHook,
                     MUIA_NListtree_DragDropSort    , TRUE,
                     MUIA_NListtree_Title           , TRUE,
                     MUIA_NListtree_DoubleClick     , MUIV_NListtree_DoubleClick_All,
                     MUIA_NList_DefaultObjectOnClick, FALSE,
                     MUIA_ContextMenu               , (C->FolderCntMenu && PopupMenuBase),
                     MUIA_Font                      , C->FixedFontList ? MUIV_NList_Font_Fixed : MUIV_NList_Font,
                     MUIA_Dropable                  , TRUE,
                     MUIA_NList_Exports             , MUIV_NList_Exports_ColWidth|MUIV_NList_Exports_ColOrder,
                     MUIA_NList_Imports             , MUIV_NList_Imports_ColWidth|MUIV_NList_Imports_ColOrder,
                     MUIA_ObjectID                  , MAKE_ID('N','L','0','1'),
                  End,
               End,
               Child, BalanceObject, End,
               Child, data->GUI.LV_MAILS = NListviewObject,
                  MUIA_HelpNode, "MA01",
                  MUIA_CycleChain,1,
                  MUIA_NListview_NList, data->GUI.NL_MAILS = NewObject(CL_MailList->mcc_Class, NULL,
                     MUIA_NList_MinColSortable, 0,
                     MUIA_NList_TitleClick          , TRUE,
                     MUIA_NList_TitleClick2         , TRUE,
                     MUIA_NList_DragType            , MUIV_NList_DragType_Default,
                     MUIA_NList_MultiSelect         , MUIV_NList_MultiSelect_Default,
                     MUIA_NList_CompareHook2        , &MA_LV_Cmp2Hook,
                     MUIA_NList_DisplayHook         , &MA_LV_DspFuncHook,
                     MUIA_NList_AutoVisible         , TRUE,
                     MUIA_NList_Title               , TRUE,
                     MUIA_NList_TitleSeparator      , TRUE,
                     MUIA_NList_DefaultObjectOnClick, FALSE,
                     MUIA_ContextMenu               , (C->MessageCntMenu && PopupMenuBase),
                     MUIA_Font                      , C->FixedFontList ? MUIV_NList_Font_Fixed : MUIV_NList_Font,
                     MUIA_NList_Exports             , MUIV_NList_Exports_ColWidth|MUIV_NList_Exports_ColOrder,
                     MUIA_NList_Imports             , MUIV_NList_Imports_ColWidth|MUIV_NList_Imports_ColOrder,
                     MUIA_ObjectID                  , MAKE_ID('N','L','0','2'),
                  End,
               End,
            End,
         End,
      End;

      if (data->GUI.WI)
      {
         MA_MakeFOFormat(data->GUI.NL_FOLDERS);
         MA_MakeMAFormat(data->GUI.NL_MAILS);
         DoMethod(G->App, OM_ADDMEMBER, data->GUI.WI);

         // define the StatusFlag images that should be used
         for (i = 0; i < 17; i++) DoMethod(data->GUI.NL_MAILS, MUIM_NList_UseImage, data->GUI.BC_STAT[i], i, MUIF_NONE);

         // Define the Images the FolderListtree that can be used
         for (i = 0; i < MAXBCSTDIMAGES; i++) DoMethod(data->GUI.NL_FOLDERS, MUIM_NList_UseImage, data->GUI.BC_FOLDER[i], i, MUIF_NONE);

         // Now we need the XPK image also in the folder list
         DoMethod(data->GUI.NL_FOLDERS, MUIM_NList_UseImage, data->GUI.BC_STAT[15], MAXBCSTDIMAGES, MUIF_NONE);

         set(data->GUI.WI,MUIA_Window_DefaultObject,data->GUI.NL_MAILS);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_ABOUT     ,G->AY_Win,3,MUIM_Set                ,MUIA_Window_Open,TRUE);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_VERSION   ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&MA_CheckVersionHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_ERRORS    ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&MA_ShowErrorsHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_LOGIN     ,MUIV_Notify_Application  ,2,MUIM_Application_ReturnID,ID_RESTART);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_HIDE      ,MUIV_Notify_Application  ,3,MUIM_Set                 ,MUIA_Application_Iconified,TRUE);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_QUIT      ,MUIV_Notify_Application  ,2,MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_NEWF      ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&FO_NewFolderHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_NEWFG     ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&FO_NewFolderGroupHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_EDITF     ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&FO_EditFolderHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_DELETEF   ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&FO_DeleteFolderHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_OSAVE     ,MUIV_Notify_Application  ,3,MUIM_CallHook            ,&FO_SetOrderHook,SO_SAVE);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_ORESET    ,MUIV_Notify_Application  ,3,MUIM_CallHook            ,&FO_SetOrderHook,SO_RESET);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_SELALL    ,data->GUI.NL_MAILS,4,MUIM_NList_Select,MUIV_NList_Select_All,MUIV_NList_Select_On,NULL);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_SELNONE   ,data->GUI.NL_MAILS,4,MUIM_NList_Select,MUIV_NList_Select_All,MUIV_NList_Select_Off,NULL);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_SELTOGG   ,data->GUI.NL_MAILS,4,MUIM_NList_Select,MUIV_NList_Select_All,MUIV_NList_Select_Toggle,NULL);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_SEARCH    ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&FI_OpenHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_FILTER    ,MUIV_Notify_Application  ,5,MUIM_CallHook            ,&MA_ApplyRulesHook,APPLY_USER,0,FALSE);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_DELDEL    ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&MA_DeleteDeletedHook, FALSE);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_INDEX     ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&MA_RescanIndexHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_FLUSH     ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&MA_FlushIndexHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_ABOOK     ,MUIV_Notify_Application  ,3,MUIM_CallHook            ,&AB_OpenHook,ABM_EDIT);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_EXPORT    ,MUIV_Notify_Application  ,3,MUIM_CallHook            ,&MA_ExportMessagesHook,TRUE);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_IMPORT    ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&MA_ImportMessagesHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_GETMAIL   ,MUIV_Notify_Application  ,6,MUIM_CallHook            ,&MA_PopNowHook,POP_USER,-1,0,FALSE);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_SENDMAIL  ,MUIV_Notify_Application  ,3,MUIM_CallHook            ,&MA_SendHook,SEND_ALL);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_EXMAIL    ,MUIV_Notify_Application  ,6,MUIM_CallHook            ,&MA_PopNowHook,POP_USER,-1,IEQUALIFIER_LSHIFT,FALSE);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_READ      ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&MA_ReadMessageHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_EDIT      ,MUIV_Notify_Application  ,5,MUIM_CallHook            ,&MA_NewMessageHook,NEW_EDIT,0,FALSE);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_MOVE      ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&MA_MoveMessageHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_COPY      ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&MA_CopyMessageHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_DELETE    ,MUIV_Notify_Application  ,4,MUIM_CallHook            ,&MA_DeleteMessageHook,0,FALSE);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_PRINT     ,MUIV_Notify_Application  ,3,MUIM_CallHook            ,&MA_SavePrintHook,TRUE);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_SAVE      ,MUIV_Notify_Application  ,3,MUIM_CallHook            ,&MA_SavePrintHook,FALSE);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_DETACH    ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&MA_SaveAttachHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_CROP      ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&MA_RemoveAttachHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_EXPMSG    ,MUIV_Notify_Application  ,3,MUIM_CallHook            ,&MA_ExportMessagesHook,FALSE);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_NEW       ,MUIV_Notify_Application  ,5,MUIM_CallHook            ,&MA_NewMessageHook,NEW_NEW,0,FALSE);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_REPLY     ,MUIV_Notify_Application  ,5,MUIM_CallHook            ,&MA_NewMessageHook,NEW_REPLY,0,FALSE);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_FORWARD   ,MUIV_Notify_Application  ,5,MUIM_CallHook            ,&MA_NewMessageHook,NEW_FORWARD,0,FALSE);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_BOUNCE    ,MUIV_Notify_Application  ,5,MUIM_CallHook            ,&MA_NewMessageHook,NEW_BOUNCE,0,FALSE);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_SAVEADDR  ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&MA_GetAddressHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_CHSUBJ    ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&MA_ChangeSubjectHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_SEND      ,MUIV_Notify_Application  ,3,MUIM_CallHook            ,&MA_SendHook,SEND_ACTIVE);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_TOREAD    ,MUIV_Notify_Application  ,3,MUIM_CallHook            ,&MA_SetStatusToHook,STATUS_OLD);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_TOUNREAD  ,MUIV_Notify_Application  ,3,MUIM_CallHook            ,&MA_SetStatusToHook,STATUS_UNR);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_TOHOLD    ,MUIV_Notify_Application  ,3,MUIM_CallHook            ,&MA_SetStatusToHook,STATUS_HLD);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_TOQUEUED  ,MUIV_Notify_Application  ,3,MUIM_CallHook            ,&MA_SetStatusToHook,STATUS_WFS);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_CONFIG    ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&CO_OpenHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_USER      ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&US_OpenHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_MUI       ,MUIV_Notify_Application  ,2,MUIM_Application_OpenConfigWindow,0);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_ABOUTMUI  ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&MA_AboutMUIHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_MenuAction   ,MMEN_SCRIPT    ,MUIV_Notify_Application  ,3,MUIM_CallHook            ,&MA_CallRexxHook,-1);
         for (i = 0; i < 10; i++) DoMethod(data->GUI.WI,MUIM_Notify,MUIA_Window_MenuAction,MMEN_MACRO+i,MUIV_Notify_Application,3,MUIM_CallHook        ,&MA_CallRexxHook, i);
         for (i = 0; i < MAXP3; i++) DoMethod(data->GUI.WI,MUIM_Notify,MUIA_Window_MenuAction,MMEN_POPHOST+i,MUIV_Notify_Application,6,MUIM_CallHook   ,&MA_PopNowHook,POP_USER,i,0,FALSE);
         if (data->GUI.TO_TOOLBAR)
         {
            DoMethod(data->GUI.TO_TOOLBAR     ,MUIM_Toolbar_Notify, 0, MUIV_Toolbar_Notify_Pressed,FALSE,MUIV_Notify_Application,2,MUIM_CallHook,&MA_ReadMessageHook);
            DoMethod(data->GUI.TO_TOOLBAR     ,MUIM_Toolbar_Notify, 1, MUIV_Toolbar_Notify_Pressed,MUIV_EveryTime,MUIV_Notify_Application,5,MUIM_CallHook,&MA_NewMessageHook,NEW_EDIT,MUIV_Toolbar_Qualifier,MUIV_TriggerValue);
            DoMethod(data->GUI.TO_TOOLBAR     ,MUIM_Toolbar_Notify, 2, MUIV_Toolbar_Notify_Pressed,FALSE,MUIV_Notify_Application,2,MUIM_CallHook,&MA_MoveMessageHook);
            DoMethod(data->GUI.TO_TOOLBAR     ,MUIM_Toolbar_Notify, 3, MUIV_Toolbar_Notify_Pressed,MUIV_EveryTime,MUIV_Notify_Application,4,MUIM_CallHook,&MA_DeleteMessageHook,MUIV_Toolbar_Qualifier,MUIV_TriggerValue);
            DoMethod(data->GUI.TO_TOOLBAR     ,MUIM_Toolbar_Notify, 4, MUIV_Toolbar_Notify_Pressed,FALSE,MUIV_Notify_Application,2,MUIM_CallHook,&MA_GetAddressHook);
            DoMethod(data->GUI.TO_TOOLBAR     ,MUIM_Toolbar_Notify, 6, MUIV_Toolbar_Notify_Pressed,MUIV_EveryTime,MUIV_Notify_Application,5,MUIM_CallHook,&MA_NewMessageHook,NEW_NEW,MUIV_Toolbar_Qualifier,MUIV_TriggerValue);
            DoMethod(data->GUI.TO_TOOLBAR     ,MUIM_Toolbar_Notify, 7, MUIV_Toolbar_Notify_Pressed,MUIV_EveryTime,MUIV_Notify_Application,5,MUIM_CallHook,&MA_NewMessageHook,NEW_REPLY,MUIV_Toolbar_Qualifier,MUIV_TriggerValue);
            DoMethod(data->GUI.TO_TOOLBAR     ,MUIM_Toolbar_Notify, 8, MUIV_Toolbar_Notify_Pressed,MUIV_EveryTime,MUIV_Notify_Application,5,MUIM_CallHook,&MA_NewMessageHook,NEW_FORWARD,MUIV_Toolbar_Qualifier,MUIV_TriggerValue);
            DoMethod(data->GUI.TO_TOOLBAR     ,MUIM_Toolbar_Notify,10, MUIV_Toolbar_Notify_Pressed,MUIV_EveryTime,MUIV_Notify_Application,9,MUIM_Application_PushMethod,G->App,6,MUIM_CallHook,&MA_PopNowHook,POP_USER,-1,MUIV_Toolbar_Qualifier,MUIV_TriggerValue);
            DoMethod(data->GUI.TO_TOOLBAR     ,MUIM_Toolbar_Notify,11, MUIV_Toolbar_Notify_Pressed,FALSE,MUIV_Notify_Application,7,MUIM_Application_PushMethod,G->App,3,MUIM_CallHook,&MA_SendHook,SEND_ALL);
            DoMethod(data->GUI.TO_TOOLBAR     ,MUIM_Toolbar_Notify,13, MUIV_Toolbar_Notify_Pressed,MUIV_EveryTime,MUIV_Notify_Application,5,MUIM_CallHook,&MA_ApplyRulesHook,APPLY_USER,MUIV_Toolbar_Qualifier,MUIV_TriggerValue);
            DoMethod(data->GUI.TO_TOOLBAR     ,MUIM_Toolbar_Notify,14, MUIV_Toolbar_Notify_Pressed,FALSE,MUIV_Notify_Application,2,MUIM_CallHook,&FI_OpenHook);
            DoMethod(data->GUI.TO_TOOLBAR     ,MUIM_Toolbar_Notify,15, MUIV_Toolbar_Notify_Pressed,FALSE,MUIV_Notify_Application,3,MUIM_CallHook,&AB_OpenHook,ABM_EDIT);
            DoMethod(data->GUI.TO_TOOLBAR     ,MUIM_Toolbar_Notify,16, MUIV_Toolbar_Notify_Pressed,FALSE,MUIV_Notify_Application,2,MUIM_CallHook,&CO_OpenHook);
         }
         DoMethod(data->GUI.NL_MAILS       ,MUIM_Notify,MUIA_NList_DoubleClick   ,MUIV_EveryTime,MUIV_Notify_Application  ,3,MUIM_CallHook            ,&MA_ReadMessageHook,FALSE);
         DoMethod(data->GUI.NL_MAILS       ,MUIM_Notify,MUIA_NList_TitleClick    ,MUIV_EveryTime,MUIV_Notify_Self         ,4,MUIM_NList_Sort3         ,MUIV_TriggerValue,MUIV_NList_SortTypeAdd_2Values,MUIV_NList_Sort3_SortType_Both);
         DoMethod(data->GUI.NL_MAILS       ,MUIM_Notify,MUIA_NList_TitleClick2   ,MUIV_EveryTime,MUIV_Notify_Self         ,4,MUIM_NList_Sort3         ,MUIV_TriggerValue,MUIV_NList_SortTypeAdd_2Values,MUIV_NList_Sort3_SortType_2);
         DoMethod(data->GUI.NL_MAILS       ,MUIM_Notify,MUIA_NList_SortType      ,MUIV_EveryTime,MUIV_Notify_Self         ,3,MUIM_Set                 ,MUIA_NList_TitleMark,MUIV_TriggerValue);
         DoMethod(data->GUI.NL_MAILS       ,MUIM_Notify,MUIA_NList_SortType2     ,MUIV_EveryTime,MUIV_Notify_Self         ,3,MUIM_Set                 ,MUIA_NList_TitleMark2,MUIV_TriggerValue);
         DoMethod(data->GUI.NL_MAILS       ,MUIM_Notify,MUIA_NList_SelectChange  ,TRUE          ,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&MA_ChangeSelectedHook);
         DoMethod(data->GUI.NL_MAILS       ,MUIM_Notify,MUIA_NList_Active        ,MUIV_EveryTime,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&MA_ChangeSelectedHook);
         DoMethod(data->GUI.NL_MAILS       ,MUIM_Notify,MUIA_NList_Active        ,MUIV_EveryTime,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&MA_SetMessageInfoHook);
         DoMethod(data->GUI.NL_FOLDERS     ,MUIM_Notify,MUIA_NList_DoubleClick   ,MUIV_EveryTime,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&FO_EditFolderHook);
//         DoMethod(data->GUI.NL_FOLDERS     ,MUIM_Notify,MUIA_NList_TitleClick    ,MUIV_EveryTime,MUIV_Notify_Self         ,3,MUIM_NList_Sort2         ,MUIV_TriggerValue,MUIV_NList_SortTypeAdd_2Values);
//         DoMethod(data->GUI.NL_FOLDERS     ,MUIM_Notify,MUIA_NList_SortType      ,MUIV_EveryTime,MUIV_Notify_Self         ,3,MUIM_Set                 ,MUIA_NList_TitleMark,MUIV_TriggerValue);
         DoMethod(data->GUI.NL_FOLDERS     ,MUIM_Notify,MUIA_NListtree_Active    ,MUIV_EveryTime,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&MA_ChangeFolderHook);
         DoMethod(data->GUI.NL_FOLDERS     ,MUIM_Notify,MUIA_NListtree_Active    ,MUIV_EveryTime,MUIV_Notify_Application  ,2,MUIM_CallHook            ,&MA_SetFolderInfoHook);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_CloseRequest ,TRUE          ,MUIV_Notify_Application  ,2,MUIM_Application_ReturnID,ID_CLOSEALL);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_InputEvent   ,"-repeat del" ,MUIV_Notify_Application  ,3,MUIM_CallHook            ,&MA_DelKeyHook,FALSE);
         DoMethod(data->GUI.WI             ,MUIM_Notify,MUIA_Window_InputEvent   ,"-repeat shift del" ,MUIV_Notify_Application  ,3,MUIM_CallHook      ,&MA_DelKeyHook,TRUE);
//       DoMethod(G->App                   ,MUIM_Notify,MUIA_Application_Iconified,FALSE        ,data->GUI.WI             ,3,MUIM_Set                 ,MUIA_Window_Open,TRUE);

         // Define Notifies for ShortcutFolderKeys
         for (i = 0; i < 10; i++)
         {
            key[8] = '0'+i;
            DoMethod(data->GUI.WI, MUIM_Notify, MUIA_Window_InputEvent, key, MUIV_Notify_Application, 3, MUIM_CallHook, &MA_FolderKeyHook, i);
         }
         return data;
      }
      free(data);
   }
   return NULL;
}
///
