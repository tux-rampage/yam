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

#include "YAM.h"
#include "YAM_folderconfig.h"
#include "YAM_hook.h"
#include "YAM_mainFolder.h"
#include "YAM_read.h"
#include "YAM_rexx.h"

/* local protos */
static void MA_ValidateStatus(struct Folder*);
static char *MA_IndexFileName(struct Folder*);
static BOOL MA_DetectUUE(FILE*);
static void MA_GetRecipients(char*, struct Person**, int*);

/***************************************************************************
 Module: Main - Folder handling
***************************************************************************/

/*** Index Maintenance ***/

/// MA_PromptFolderPassword
//  Asks user for folder password
BOOL MA_PromptFolderPassword(struct Folder *fo, APTR win)
{
   char passwd[SIZE_PASSWORD], prompt[SIZE_LARGE];
   struct User *user = US_GetCurrentUser();

   if (fo->Flags&FOFL_FREEXS) return TRUE;
   if (!Stricmp(fo->Password, user->Password)) return TRUE;
   sprintf(prompt, GetStr(MSG_MA_GetFolderPass), fo->Name);
   do {
      *passwd = 0;
      if (!StringRequest(passwd, SIZE_PASSWORD, GetStr(MSG_Folder), prompt, GetStr(MSG_Okay), NULL, GetStr(MSG_Cancel), TRUE, win)) return FALSE;
   } while (Stricmp(passwd, fo->Password));
   return TRUE;
}

///
/// MA_ValidateStatus
//  Avoids invalid status values
static void MA_ValidateStatus(struct Folder *folder)
{
   struct Mail *mail;

   for (mail = folder->Messages; mail; mail = mail->Next)
      if (mail->Status == STATUS_NEW)
         if (folder->Type == FT_OUTGOING) MA_SetMailStatus(mail, STATUS_WFS);
         else if (folder->Type == FT_SENT) MA_SetMailStatus(mail, STATUS_SNT);
         else if (C->UpdateNewMail) MA_SetMailStatus(mail, STATUS_UNR);
}

///
/// MA_IndexFileName
//  Returns file name of folder index
static char *MA_IndexFileName(struct Folder *folder)
{
   static char buffer[SIZE_PATHFILE];
   strcpy(buffer, GetFolderDir(folder));
   AddPart(buffer, ".index", SIZE_PATHFILE);
   return buffer;
}

///
/// MA_LoadIndex
//  Loads a folder index from disk
int MA_LoadIndex(struct Folder *folder, BOOL full)
{
   FILE *fh;
   int indexloaded = 0;
   char buf[SIZE_LARGE];

   DB( kprintf("Loading index for folder %s\n", folder->Name); )

   if (fh = fopen(MA_IndexFileName(folder), "r"))
   {
      struct FIndex fi;
      Busy(GetStr(MSG_BusyLoadingIndex), folder->Name, 0, 0);
      fread(&fi, sizeof(struct FIndex), 1, fh);
      if (fi.ID == MAKE_ID('Y','I','N','3'))
      {
         BOOL corrupt = FALSE;

         folder->Total  = fi.Total;
         folder->New    = fi.New;
         folder->Unread = fi.Unread;
         folder->Size   = fi.Size;
         indexloaded++;

         if (full)
         {
            ClearMailList(folder, TRUE);
            for (;;)
            {
               struct Mail mail;
               struct ComprMail cmail;
               clear(&mail, sizeof(struct Mail));               
               if (fread(&cmail, sizeof(struct ComprMail), 1, fh) != 1) break;
               if (cmail.MoreBytes > SIZE_LARGE)
               {
                  fpos_t pos;
                  printf("WARNING: Index of folder '%s' CORRUPTED near mailfile '%s' (MoreBytes: 0x%x) - aborting!\n", folder->Name, cmail.MailFile, cmail.MoreBytes);
                  if (!fgetpos(fh, &pos)) printf("File position: %ld\n", pos);

                  corrupt = TRUE;
                  break;
               }

               fread(buf, 1, cmail.MoreBytes, fh);
               strcpy(mail.Subject, GetNextLine(buf));
               strcpy(mail.From.Address, GetNextLine(NULL));
               strcpy(mail.From.RealName, GetNextLine(NULL));
               strcpy(mail.To.Address, GetNextLine(NULL));
               strcpy(mail.To.RealName, GetNextLine(NULL));
               strcpy(mail.ReplyTo.Address, GetNextLine(NULL));
               strcpy(mail.ReplyTo.RealName, GetNextLine(NULL));
               mail.Folder = folder;
               mail.Flags = (cmail.Flags&0x7ff);
               strcpy(mail.MailFile, cmail.MailFile);
               mail.Date = cmail.Date;
               mail.Status = cmail.Status;
               mail.Importance = cmail.Importance;
               mail.cMsgID = cmail.cMsgID;
               mail.cIRTMsgID = cmail.cIRTMsgID;
               mail.Size = cmail.Size;
               AddMailToList(&mail, folder);
            }
            if (corrupt) {
               ClearMailList(folder, TRUE);
/*
If/when this is enabled, remove the "else".
               MA_ScanMailBox(folder);
               MA_SaveIndex(folder);
*/
            }
            else {
               indexloaded++;
               folder->Flags &= ~FOFL_MODIFY;
            }
         }
      }
      BusyEnd;
      fclose(fh);
   }
   return indexloaded;
}

///
/// MA_SaveIndex
//  Saves a folder index to disk
BOOL MA_SaveIndex(struct Folder *folder)
{
   struct Mail *mail;
   FILE *fh;
   struct ComprMail cmail;
   char buf[SIZE_LARGE];
   struct FIndex fi;

   if (!(fh = fopen(MA_IndexFileName(folder), "w"))) return FALSE;
   Busy(GetStr(MSG_BusySavingIndex), folder->Name, 0, 0);
   fi.ID = MAKE_ID('Y','I','N','3');
   fi.Total = folder->Total; fi.New = folder->New; fi.Unread = folder->Unread; fi.Size = folder->Size;
   fwrite(&fi, sizeof(struct FIndex), 1, fh);
   for (mail = folder->Messages; mail; mail = mail->Next)
   {
      clear(&cmail, sizeof(struct ComprMail));
      sprintf(buf, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
         mail->Subject,
         mail->From.Address, mail->From.RealName,
         mail->To.Address, mail->To.RealName,
         mail->ReplyTo.Address, mail->ReplyTo.RealName);
      cmail.Flags = mail->Flags;
      strcpy(cmail.MailFile, mail->MailFile);
      cmail.Date = mail->Date;
      cmail.Status = mail->Status;
      cmail.Importance = mail->Importance;
      cmail.cMsgID = mail->cMsgID;
      cmail.cIRTMsgID = mail->cIRTMsgID;
      cmail.Size = mail->Size;
      cmail.MoreBytes = strlen(buf);
      fwrite(&cmail, sizeof(struct ComprMail), 1, fh);
      fwrite(buf, 1, cmail.MoreBytes, fh);
   }
   fclose(fh);
   folder->Flags &= ~FOFL_MODIFY;
   BusyEnd;
   return TRUE;
}

///
/// MA_GetIndex
//  Opens/unlocks a folder
BOOL MA_GetIndex(struct Folder *folder)
{
   if (!folder) return FALSE;
   if (folder->Type == FT_GROUP) return FALSE;
   if (folder->LoadedMode != 2)
   {
      if (*folder->Password && (folder->XPKType&1))
         if (!MA_PromptFolderPassword(folder, G->MA->GUI.WI)) return FALSE;
      if (!MA_LoadIndex(folder, TRUE))
      {
         MA_ScanMailBox(folder);
         MA_SaveIndex(folder);
      }
      MA_ValidateStatus(folder);
      folder->LoadedMode = 2;
   }
   return TRUE;
}

///
/// MA_ExpireIndex
//  Invalidates a folder index
void MA_ExpireIndex(struct Folder *folder)
{
   if (!(folder->Flags&FOFL_MODIFY)) DeleteFile(MA_IndexFileName(folder));
   folder->Flags |= FOFL_MODIFY;
}

///
/// MA_UpdateIndexes
//  Updates indices of all folders
void MA_UpdateIndexes(BOOL initial)
{
   int i;
   struct Folder **flist;

   if (flist = FO_CreateList())
   {
      for (i = 1; i <= (int)*flist; i++) if (flist[i]->Type != FT_GROUP)
         if (initial)
         {
            long dirdate = getft(GetFolderDir(flist[i]));
            long inddate = getft(MA_IndexFileName(flist[i]));
            if (dirdate > inddate+30 && inddate != -1)
            {
               DeleteFile(MA_IndexFileName(flist[i]));
               MA_GetIndex(flist[i]);
            }
         }
         else
         {
            if (flist[i]->LoadedMode == 2 && (flist[i]->Flags&FOFL_MODIFY)) MA_SaveIndex(flist[i]);
         }
      free(flist);
   }
}

///
/// MA_FlushIndexes
//  Removes loaded folder indices from memory and closes folders
void MA_FlushIndexes(BOOL all)
{
   int i;
   struct Folder *fo, **flist, *actfo = FO_GetCurrentFolder();

   if (flist = FO_CreateList())
   {
      for (i = 1; i <= (int)*flist; i++)
      {
         fo = flist[i];
         if ((fo->Type == FT_SENT || fo->Type == FT_CUSTOM || fo->Type == FT_CUSTOMSENT) && fo != actfo  && fo->LoadedMode == 2 && (all || (fo->Flags&FOFL_FREEXS)))
         {
            if (fo->Flags&FOFL_MODIFY) MA_SaveIndex(fo);
            ClearMailList(fo, FALSE);
            fo->LoadedMode = 1;
            fo->Flags &= ~FOFL_FREEXS;
         }
      }
      free(flist);
      DoMethod(G->MA->GUI.LV_FOLDERS, MUIM_NList_Redraw, MUIV_NList_Redraw_All);
   }
}

HOOKPROTONHNONP(MA_FlushIndexFunc, void)
{
   MA_FlushIndexes(TRUE);
}
MakeHook(MA_FlushIndexHook, MA_FlushIndexFunc);
///

/*** Private functions ***/
/// MA_ChangeFolder
//  Changes to another folder
void MA_ChangeFolder(struct Folder *folder)
{
   BOOL folderopen = TRUE;
   int i, pos = -1;
   struct MA_GUIData *gui = &G->MA->GUI;

   set(gui->NL_MAILS, MUIA_ShortHelp, NULL);

   if(!folder)
   {
      folder = FO_GetCurrentFolder();
   }
   else if(FO_GetCurrentFolder() == folder) return;

   if (folder->Type == FT_GROUP) folderopen = FALSE;
   else if (!MA_GetIndex(folder)) folderopen = FALSE;

   if(!folder->FImage) FO_LoadTreeImage(folder); // experimental !!!

   if (folderopen)
   {
      MA_SetSortFlag();
      DisplayMailList(folder, gui->NL_MAILS);

      if (C->JumpToNewMsg)
      {
         // jump to first or last unread mail in folder,
         // depending on sort order

         int incr;

         if (folder->Sort[0] < 0 || folder->Sort[1] < 0)
         {
            get(gui->NL_MAILS, MUIA_NList_Entries, &i);
            i--;
            incr = -1;
         }
         else
         {
            i = 0;
            incr = 1;
         }

         while (1)
         {
            struct Mail *mail;
            DoMethod(gui->NL_MAILS, MUIM_NList_GetEntry, i, &mail, TAG_DONE);
            if (!mail)
            {
               pos = -1;
               break;
            }

            if (mail->Status == STATUS_NEW || mail->Status == STATUS_UNR)
            {
               pos = i;
               break;
            }

            i += incr;
         }
      }

      set(gui->NL_MAILS, MUIA_NList_Active, pos >= 0 ? pos : folder->LastActive);
   }
   set(gui->LV_MAILS, MUIA_Disabled, !folderopen);
}

HOOKPROTONHNONP(MA_ChangeFolderFunc, void)
{
   MA_ChangeFolder(NULL);
}
MakeHook(MA_ChangeFolderHook, MA_ChangeFolderFunc);
///
/// MA_FolderContextMenu
//  Creates a ContextMenu for the folder Listtree
ULONG MA_FolderContextMenu(struct MUIP_ContextMenuBuild *msg)
{
  struct MUI_NListtree_TestPos_Result r;
  struct MUI_NListtree_TreeNode *tn;
  struct Folder *folder;
  struct PopupMenu *pop_menu;
  struct Window *win;
  struct MA_GUIData *gui = &G->MA->GUI;
  ULONG ret = 0;
  BOOL tmp_dis = TRUE;

  enum{ PMN_EDITF=1, PMN_DELETEF, PMN_INDEX, PMN_FLUSH, PMN_NEWF, PMN_NEWFG, PMN_SNAPS };

  // Now lets find out which entry is under the mouse pointer
  DoMethod(gui->NL_FOLDERS, MUIM_NListtree_TestPos, msg->mx, msg->my, &r);

  tn = r.tpr_TreeNode;
  if(!tn)  return(0);

  folder = (struct Folder *)tn->tn_User;
  if(!folder)  return(0);

  // Set this Treenode as activ
  nnset(gui->NL_FOLDERS, MUIA_NListtree_Active, tn);

  // Get the window structure of the window which this listtree belongs to
  get(_win(gui->NL_FOLDERS), MUIA_Window_Window, &win);
  if(!win) return(0);

  // Now we are going to add the MenuItems to this PopupMenu
  if(folder->Type != FT_GROUP) tmp_dis = FALSE;

  // We create the PopupMenu now
  pop_menu =   PMMenu(FolderName(folder)),
                 PMItem(GetStripStr(MSG_FOLDER_EDIT)),           PM_UserData, PMN_EDITF,     End,
                 PMItem(GetStripStr(MSG_FOLDER_DELETE)),         PM_UserData, PMN_DELETEF,   End,
                 PMItem(GetStripStr(MSG_MA_UpdateIndex)),        PM_Disabled, tmp_dis,       PM_UserData, PMN_INDEX, End,
                 PMItem(GetStripStr(MSG_MA_FlushIndices)),       PM_Disabled, tmp_dis,       PM_UserData, PMN_FLUSH,  End,
                 PMBar, End,
                 PMItem(GetStripStr(MSG_FOLDER_NEWFOLDER)),      PM_UserData, PMN_NEWF,      End,
                 PMItem(GetStripStr(MSG_FOLDER_NEWFOLDERGROUP)), PM_UserData, PMN_NEWFG,     End,
                 PMBar, End,
                 PMItem(GetStripStr(MSG_FOLDER_SNAPSHOT)),       PM_UserData, PMN_SNAPS,     End,
               End;

  ret = (ULONG)(PM_OpenPopupMenu(win, PM_Menu,pop_menu, TAG_DONE));

  PM_FreePopupMenu(pop_menu);

  switch(ret)
  {
    case PMN_EDITF:   DoMethod(G->App, MUIM_CallHook, &FO_EditFolderHook,     TAG_DONE);           break;
    case PMN_DELETEF: DoMethod(G->App, MUIM_CallHook, &FO_DeleteFolderHook,   TAG_DONE);           break;
    case PMN_INDEX:   DoMethod(G->App, MUIM_CallHook, &MA_RescanIndexHook,    TAG_DONE);           break;
    case PMN_FLUSH:   DoMethod(G->App, MUIM_CallHook, &MA_FlushIndexHook,     TAG_DONE);           break;
    case PMN_NEWF:    DoMethod(G->App, MUIM_CallHook, &FO_NewFolderHook,      TAG_DONE);           break;
    case PMN_NEWFG:   DoMethod(G->App, MUIM_CallHook, &FO_NewFolderGroupHook, TAG_DONE);           break;
    case PMN_SNAPS:   DoMethod(G->App, MUIM_CallHook, &FO_SetOrderHook,       SO_SAVE, TAG_DONE);  break;
  }

  return(0);
}

///

/*** Mail header scanning ***/
/// MA_NewMailFile
//  Returns an unique name for a new mail file
char *MA_NewMailFile(struct Folder *folder, char *mailfile, int daynumber)
{
   static char buffer[SIZE_PATHFILE];
   char mfile[SIZE_MFILE];
   struct Mail *mail;
   int cnt, mcnt = 0;
   
   if (!mailfile) mailfile = mfile;
   if (!daynumber) { struct DateStamp ds; DateStamp(&ds); daynumber = ds.ds_Days; }
   MA_GetIndex(folder);
   for (mail = folder->Messages; mail; mail = mail->Next)
      if (atoi(mail->MailFile) == daynumber)
         if ((cnt = atoi(&(mail->MailFile)[6])) > mcnt) mcnt = cnt;
   do {
      sprintf(mailfile, "%05d.%03d", daynumber, ++mcnt);
      strcpy(buffer, GetFolderDir(folder));
      AddPart(buffer, mailfile, SIZE_PATHFILE);
   } while (access(buffer,F_OK) == 0);
   return buffer;
}

///
/// MA_DetectUUE
//  Checks if message contains an uuencoded file
static BOOL MA_DetectUUE(FILE *fh)
{
   char *buffer;
   BOOL found = FALSE;
   int i;

   buffer = calloc(SIZE_LINE,1);
   for (i = 0; GetLine(fh, buffer, SIZE_LINE) && !found && i < 30; i++)
      if (!strncmp(buffer, "begin ", 6) && isdigit((int)buffer[6])) found = TRUE;
   free(buffer);
   return found;
}

///
/// MA_ReadHeader
//  Reads header lines of a message into memory
struct Data2D Header = { 0, 0, NULL };

BOOL MA_ReadHeader(FILE *fh)
{
   char *buffer, *ptr, *head;
   BOOL success = FALSE;
   char prevcharset[SIZE_DEFAULT];

   buffer = calloc(SIZE_LINE,1);
   head   = calloc(SIZE_LINE,1);
   FreeData2D(&Header);
   while (GetLine(fh, buffer, SIZE_LARGE))
   {
      if (!buffer[0]) { success = TRUE; break; }
      clear(head, SIZE_LINE);
      strcpy(prevcharset, "us-ascii");
      RE_ProcessHeader(prevcharset, buffer, TRUE, head);
      if ((buffer[0] == ' ' || buffer[0] == '\t') && Header.Used)
      {
         for (ptr = head; *ptr && ISpace(*ptr); ptr++);
         ptr = StrBufCat(Header.Data[Header.Used-1], ptr);
      }
      else                      
         ptr = StrBufCpy(AllocData2D(&Header, SIZE_DEFAULT), head);
      Header.Data[Header.Used-1] = ptr;
   }
   free(buffer); free(head);
   return success;
}  

///
/// MA_FreeEMailStruct
//  Frees an extended email structure
void MA_FreeEMailStruct(struct ExtendedMail *email)
{
   if (email->SenderInfo) FreeStrBuf(email->SenderInfo);
   if (email->Headers ) FreeStrBuf(email->Headers );
   if (email->NoSTo)    free(email->STo);
   if (email->NoCC )    free(email->CC );
   if (email->NoBCC)    free(email->BCC);
   clear(email, sizeof(struct ExtendedMail));
}

///
/// MA_GetRecipients
//  Extracts recipients from a header field
static void MA_GetRecipients(char *h, struct Person **per, int *percnt)
{
   int cnt;
   char *p = h, *next;
   for (cnt = 0; *p;)
   {
      cnt++;
      if (p = MyStrChr(p, ',')) p++; else break;
   }
   *percnt = cnt;
   if (cnt)
   {
      *per = calloc(cnt, sizeof(struct Person));
      for (cnt = 0, p = h; *p; cnt++)
      {
         if (next = MyStrChr(p, ',')) *next++ = 0;
         ExtractAddress(p, (struct Person *)((ULONG)*per+cnt*sizeof(struct Person)));
         if (!(p = next)) break;
      }
   }
}

///
/// MA_ExamineMail
//  Parses the header lines of a message and fills email structure
struct ExtendedMail *MA_ExamineMail(struct Folder *folder, char *file, char *statstr, BOOL deep)
{
   static struct ExtendedMail email;
   static struct Person pe;
   struct Mail *mail = (struct Mail *)&email;
   char *p, fullfile[SIZE_PATHFILE];
   int ok, i, j;
   struct DateStamp *foundDate = NULL;
   FILE *fh;

   clear(&email, sizeof(struct ExtendedMail));
   stccpy(mail->MailFile, file, SIZE_MFILE);
   email.DelSend = !C->SaveSent;
   if (fh = fopen(GetMailFile(fullfile, folder, mail), "r"))
   {
      BOOL xpk = FALSE;
      if (fgetc(fh) == 'X') if (fgetc(fh) == 'P') if (fgetc(fh) == 'K') xpk = TRUE;
      if (xpk)
      {
         fclose(fh);
         if (!StartUnpack(GetMailFile(NULL, folder, mail), fullfile, folder)) return NULL;
         fh = fopen(fullfile, "r");
      }
      else rewind(fh);
   }
   if (fh)
   {
      MA_ReadHeader(fh);
      if (MA_DetectUUE(fh)) mail->Flags |= MFLAG_MULTIPART;
      fclose(fh);
      for (ok=i=0; i < Header.Used; i++)
      {
         char *value, *field = Header.Data[i];
         if (value = strchr(field, ':'))
         {
            *value++ = 0;
            if (!stricmp(field, "from"))
            {
               ok |= 1;
               ExtractAddress(value, &pe);
               mail->From =  pe;
            }
            if (!stricmp(field, "reply-to"))
            {
               ok |= 8;
               ExtractAddress(value, &pe);
               mail->ReplyTo = pe;
            }
            if (!stricmp(field, "original-recipient"))
            {
               ExtractAddress(value, &pe);
               email.OriginalRcpt = pe;
            }
            if (!stricmp(field, "disposition-notification-to"))
            {
               ExtractAddress(value, &pe);
               email.ReceiptTo = pe;
               email.ReceiptType = RCPT_TYPE_ALL;
               mail->Flags |= MFLAG_SENDMDN;
            }
            if (!stricmp(field, "return-view-to"))
            {
               ExtractAddress(value, &pe);
               email.ReceiptTo = pe;
               email.ReceiptType = RCPT_TYPE_READ;
            }
            if (!stricmp(field, "return-receipt-to"))
            {
               email.RetRcpt = TRUE;
            }
            if (!stricmp(field, "to") && !(ok & 2))
            {
               ok |= 2;
               if (p = MyStrChr(value, ',')) *p++ = 0;
               ExtractAddress(value, &pe);
               mail->To = pe;
               if (p)
               {
                  mail->Flags |= MFLAG_MULTIRCPT;
                  if (deep && !email.NoSTo) MA_GetRecipients(p, &(email.STo), &(email.NoSTo));
               }
            }
            if (!stricmp(field, "cc"))
            {
               mail->Flags |= MFLAG_MULTIRCPT;
               if (deep && !email.NoCC) MA_GetRecipients(value, &(email.CC), &(email.NoCC));
            }
            if (!stricmp(field, "bcc"))
            {
               mail->Flags |= MFLAG_MULTIRCPT;
               if (deep && !email.NoBCC) MA_GetRecipients(value, &(email.BCC), &(email.NoBCC));
            }
            if (!stricmp(field, "subject"))
            {
               ok |= 4;
               stccpy(mail->Subject, Trim(value), SIZE_SUBJECT);
            }
            if (!stricmp(field, "message-id"))
            {
               mail->cMsgID = CompressMsgID(p = Trim(value));
               stccpy(email.MsgID, p, SIZE_MSGID);
            }
            if (!stricmp(field, "in-reply-to"))
            {
               mail->cIRTMsgID = CompressMsgID(p = Trim(value));
               stccpy(email.IRTMsgID, p, SIZE_MSGID);
            }
            if (!stricmp(field, "date"))
            {
               foundDate = ScanDate(value);
            }
            if (!stricmp(field, "importance"))
            {
               p = Trim(value);
               if (!stricmp(p, "high")) mail->Importance = 1;
               if (!stricmp(p, "low")) mail->Importance = -1;
            }
            if (!stricmp(field, "priority") && !mail->Importance)
            {
               p = Trim(value);
               if (!stricmp(p, "urgent")) mail->Importance = 1;
               if (!stricmp(p, "non-urgent")) mail->Importance = -1;
            }
            if (!stricmp(field, "content-type"))
            {
               p = Trim(value);
               if (!strnicmp(p, "multipart/mixed", 15)) mail->Flags |= MFLAG_MULTIPART;
               if (!strnicmp(p, "multipart/report", 16)) mail->Flags |= MFLAG_REPORT;
               if (!strnicmp(p, "multipart/encrypted", 19)) mail->Flags |= MFLAG_CRYPT;
               if (!strnicmp(p, "multipart/signed", 16)) mail->Flags |= MFLAG_SIGNED;
            }
            if (!stricmp(field, "x-senderinfo"))
            {
               mail->Flags |= MFLAG_SENDERINFO;
               if (deep) email.SenderInfo = StrBufCpy(email.SenderInfo, value);
            }
            if (deep)
            {
               if (!stricmp(field, "x-yam-options"))
               {
                  if (strstr(value, "delsent")) email.DelSend = TRUE;
                  if (p = strstr(value, "sigfile")) email.Signature = p[7]-'0'+1;
                  for (j = SEC_SIGN; j <= SEC_SENDANON; j++) if (strstr(value, SecCodes[j])) email.Security = j;
               }
               if (!strnicmp(field, "x-yam-header-", 13))
               {
                  email.Headers = StrBufCat(StrBufCat(email.Headers, &field[13]), ":");
                  email.Headers = StrBufCat(StrBufCat(email.Headers, value), "\\n");
               }
            }
         }
      }
      FreeData2D(&Header);
      if ((ok & 8) && !mail->ReplyTo.RealName[0] && !stricmp(mail->ReplyTo.Address, mail->From.Address)) strcpy(mail->ReplyTo.RealName, mail->From.RealName);
      mail->Date.ds_Days   = foundDate ? foundDate->ds_Days   : atol(mail->MailFile);
      mail->Date.ds_Minute = foundDate ? foundDate->ds_Minute : 0;
      mail->Date.ds_Tick   = foundDate ? foundDate->ds_Tick   : 0;
      mail->Size = FileSize(fullfile);
      if (statstr)
         for (mail->Status = STATUS_NEW, i = 0; i < STATUS_NEW; i++)
            if (*statstr == *Status[i]) mail->Status = i;
      FinishUnpack(fullfile);
      return &email;
   }
   FinishUnpack(fullfile);
   return NULL;
}

///
/// MA_ScanMailBox
//  Scans for message files in a folder directory
void MA_ScanMailBox(struct Folder *folder)
{
   struct ExtendedMail *mail;
   struct FileInfoBlock *fib;
   BPTR lock;

   Busy(GetStr(MSG_BusyScanning), folder->Name, 0, 0);
   ClearMailList(folder, TRUE);
   fib = AllocDosObject(DOS_FIB,NULL);
   if (lock = Lock(GetFolderDir(folder), ACCESS_READ))
   {
      Examine(lock, fib);
      while (ExNext(lock,fib) && (IoErr() != ERROR_NO_MORE_ENTRIES))
      {
         DoMethod(G->App,MUIM_Application_InputBuffered);
         if (IsValidMailFile(fib->fib_FileName))
            if (fib->fib_Size)
            {
               if (mail = MA_ExamineMail(folder,fib->fib_FileName,fib->fib_Comment,FALSE))
               {
                  AddMailToList((struct Mail *)mail, folder);
                  MA_FreeEMailStruct(mail);
               }
            }
            else
            {
               char path[SIZE_PATHFILE];
               NameFromLock(lock, path, SIZE_PATHFILE);
               AddPart(path, fib->fib_FileName, SIZE_PATHFILE);
               DeleteFile(path);
            }
      }
      UnLock(lock);
   }
   FreeDosObject(DOS_FIB,fib);
   BusyEnd;
}
///

/*** Hooks ***/
/// PO_InitFolderList
//  Creates a popup list of all folders
HOOKPROTONHNP(PO_InitFolderList, long, Object *pop)
{  
   int i;
   struct Folder **flist;

   DoMethod(pop, MUIM_List_Clear);
   DoMethod(pop, MUIM_List_InsertSingle, GetStr(MSG_MA_Cancel), MUIV_List_Insert_Bottom);
   if (flist = FO_CreateList())
   {
      for (i = 1; i <= (int)*flist; i++) if (flist[i]->Type != FT_GROUP)
         DoMethod(pop, MUIM_List_InsertSingle, flist[i]->Name, MUIV_List_Insert_Bottom);
      free(flist);
   }
   return TRUE;
}
MakeHook(PO_InitFolderListHook, PO_InitFolderList);

///
/// MA_LV_FDspFunc
//  Folder listview display hook
HOOKPROTONHNO(MA_LV_FDspFunc, long, struct MUIP_NListtree_DisplayMessage *msg)
{
   if (msg != NULL && msg->TreeNode != NULL)
   {
      static char dispfold[SIZE_DEFAULT], disptot[SIZE_SMALL], dispunr[SIZE_SMALL], dispnew[SIZE_SMALL], dispsiz[SIZE_SMALL];

      struct Folder *entry = (struct Folder *)msg->TreeNode->tn_User;

      msg->Array[0] = msg->Array[1] = msg->Array[2] = msg->Array[3] = msg->Array[4] = "";
      *dispsiz = 0;

      switch(entry->Type)
      {
        case FT_GROUP:
        {
          msg->Array[0] = entry->Name;
          msg->Preparse[0] = MUIX_PH;
        }
        break;

        default:
        {
          if (entry->FImage) sprintf(msg->Array[0] = dispfold, "\033o[%d]", entry->SortIndex+1);
          else strcpy(msg->Array[0] = dispfold, "");

          if (strlen(entry->Name) > 0) strcat(dispfold, entry->Name);
          else sprintf(dispfold, "(%s)", FilePart(entry->Path));

          if (entry->XPKType&1) strcat(dispfold, " \033o[0]");

          if (entry->LoadedMode)
          {
            if (entry->New+entry->Unread) msg->Preparse[0] = MUIX_PH;
            sprintf(msg->Array[1] = disptot, "%d", entry->Total);
            sprintf(msg->Array[2] = dispunr, "%d", entry->Unread);
            sprintf(msg->Array[3] = dispnew, "%d", entry->New);
            FormatSize(entry->Size, msg->Array[4] = dispsiz);
          }
        }
      }
   }
   else 
   {
      msg->Array[0] = GetStr(MSG_Folder);
      msg->Array[1] = GetStr(MSG_Total);
      msg->Array[2] = GetStr(MSG_Unread);
      msg->Array[3] = GetStr(MSG_New);
      msg->Array[4] = GetStr(MSG_Size);
   }
   return 0;
}
MakeHook(MA_LV_FDspFuncHook,MA_LV_FDspFunc);

///
/// MA_MakeFOFormat
//  Creates format definition for folder listview
void MA_MakeFOFormat(APTR lv)
{
   int i;
   int defwidth[FOCOLNUM] = { -1,-1,-1,-1,-1 };
   char format[SIZE_LARGE];
   BOOL first = TRUE;
   *format = 0;
   for (i = 0; i < FOCOLNUM; i++) if (C->FolderCols & (1<<i))
   {
      if (first) first = FALSE; else strcat(format, " BAR,");
      sprintf(&format[strlen(format)], "COL=%ld W=%ld", i, defwidth[i]);
      if (i) strcat(format, " P=\033r");
   }
   strcat(format, " BAR");
   set(lv, MUIA_NListtree_Format, format);
}
///
