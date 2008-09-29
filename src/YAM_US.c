/***************************************************************************

 YAM - Yet Another Mailer
 Copyright (C) 1995-2000 by Marcel Beck <mbeck@yam.ch>
 Copyright (C) 2000-2008 by YAM Open Source Team

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
#include <stdlib.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <libraries/asl.h>
#include <libraries/iffparse.h>
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "extrasrc.h"

#include "SDI_hook.h"

#include "YAM.h"
#include "YAM_config.h"
#include "YAM_error.h"
#include "YAM_userlist.h"
#include "YAM_utilities.h"
#include "mui/Classes.h"

#include "FileInfo.h"
#include "Locale.h"
#include "MUIObjects.h"
#include "Requesters.h"

#include "Debug.h"

#define UFLAG_LIMITED_USER              (1 << 2)
#define UFLAG_USE_GLOBAL_ADDRESSBOOK    (1 << 1)
#define UFLAG_USE_GLOBAL_DICTIONARY     (1 << 0)

/* local protos */
static struct US_ClassData *US_New(BOOL);

/***************************************************************************
 Module: User list
***************************************************************************/

/// US_GetCurrentUser
//  Gets pointer current user from user database
struct User *US_GetCurrentUser(void)
{
  struct User *user = NULL;
  int i;

  ENTER();

  for(i = 0; i < G->Users.Num; i++)
  {
    if(G->Users.User[i].ID == G->Users.CurrentID)
    {
      user = &G->Users.User[i];
      break;
    }
  }

  RETURN(user);
  return user;
}

///
/// US_SaveUsers
//  Saves user database to .users
static void US_SaveUsers(void)
{
  FILE *fh;
  const char *fname = "PROGDIR:.users";
  int i;

  ENTER();

  // search for the first user with manager privileges
  for(i = 0; i < G->Users.Num; i++)
  {
    if(!G->Users.User[i].Limited)
      break;
  }

  // if no user with manager privileges was found, the first user is forced to that level
  if(i == G->Users.Num)
    G->Users.User[0].Limited = FALSE;

  if((fh = fopen(fname, "w")) != NULL)
  {
    fputs("YUS2 - YAM Users\n", fh);

    for(i = 0; i < G->Users.Num; i++)
    {
      struct User *user = &G->Users.User[i];
      int flags;

      flags = 0;
      if(user->Limited)
        flags |= UFLAG_LIMITED_USER;
      if(user->UseAddr)
        flags |= UFLAG_USE_GLOBAL_ADDRESSBOOK;
      if(user->UseDict)
        flags |= UFLAG_USE_GLOBAL_DICTIONARY;

      if(user->Name != NULL)
        fprintf(fh, "@USER %s\n%s\n%d\n%s\n@ENDUSER\n", user->Name, user->MailDir, flags, Encrypt(user->Password));
    }
    fclose(fh);

    AppendToLogfile(LF_VERBOSE, 62, tr(MSG_LOG_SavingUsers));
  }
  else
    ER_NewError(tr(MSG_ER_CantCreateFile), fname);

  LEAVE();
}

///
/// US_LoadUsers
//  Loads user database from .users
static void US_LoadUsers(void)
{
  BOOL save = FALSE;
  FILE *fh;

  ENTER();

  // reset the user data
  memset(&G->Users, 0, sizeof(struct Users));
  G->Users.Num = 0;

  if((fh = fopen("PROGDIR:.users", "r")) != NULL)
  {
    char buffer[SIZE_LARGE];
    BOOL hasmanager = FALSE;

    GetLine(fh, buffer, sizeof(buffer));
    if(strncmp(buffer,"YUS", 3) == 0)
    {
      int ver = buffer[3] - '0';

      while(GetLine(fh, buffer, sizeof(buffer)) != NULL)
      {
        if(strncmp(buffer, "@USER", 5) == 0)
        {
          struct User *user = &G->Users.User[G->Users.Num];
          int flags;
          enum FType type;

          strlcpy(user->Name, Trim(&buffer[6]), sizeof(user->Name));
          strlcpy(user->MailDir, Trim(GetLine(fh, buffer, sizeof(buffer))), sizeof(user->MailDir));
          if(user->MailDir[0] == '\0')
          {
            strlcpy(user->MailDir, G->MA_MailDir, sizeof(user->MailDir));
            save = TRUE;
          }

          flags = atoi(Trim(GetLine(fh, buffer, sizeof(buffer))));

          user->Limited = isFlagSet(flags, UFLAG_LIMITED_USER);
          user->UseAddr = isFlagSet(flags, UFLAG_USE_GLOBAL_ADDRESSBOOK);
          user->UseDict = isFlagSet(flags, UFLAG_USE_GLOBAL_DICTIONARY);
          if(user->Limited == FALSE)
            hasmanager = TRUE;

          if(ver >= 2)
            strlcpy(user->Password, Decrypt(GetLine(fh, buffer, sizeof(buffer))), sizeof(user->Password));

          user->ID = GetSimpleID();
          G->Users.Num++;

          // check if the user's home directory exists
          if(ObtainFileInfo(user->MailDir, FI_TYPE, &type) == FALSE || type != FIT_DRAWER)
            ER_NewError(tr(MSG_ER_USER_DIR_MISSING), user->MailDir, user->Name);

          // skip all lines until we read the "@ENDUSER"
          while(GetLine(fh, buffer, sizeof(buffer)))
          {
            if(strcmp(buffer, "@ENDUSER") == 0)
              break;
          }
        }
      }
    }
    fclose(fh);

    // if we found no user with manager privileges we give these privilege to the first user
    if(hasmanager == FALSE)
    {
      G->Users.User[0].Limited = FALSE;
      save = TRUE;
    }
  }

  // create a default user if none exists so far
  if(G->Users.Num == 0)
  {
    struct User *user = &G->Users.User[0];

    strlcpy(user->MailDir, G->MA_MailDir, sizeof(user->MailDir));
    user->Limited = FALSE;
    user->UseAddr = TRUE;
    user->UseDict = TRUE;
    user->ID = GetSimpleID();

    // there can only be one :)
    G->Users.Num = 1;
    save = TRUE;
  }

  if(save == TRUE)
    US_SaveUsers();

  LEAVE();
}

///
/// US_Login
//  User login: puts up user list and waits for a selection
BOOL US_Login(const char *username, const char *password, const char *maildir, const char *prefsfile)
{
  int userIndex = -1;
  BOOL loggedin = FALSE;

  ENTER();

  // Load the .users file first
  US_LoadUsers();

  // if a username was given check if we have this username in
  // our loaded user list
  if(username != NULL)
  {
    int i;

    for(i = 0; i < G->Users.Num; i++)
    {
      if(stricmp(G->Users.User[i].Name, username) == 0)
      {
        userIndex = i;
        break;
      }
    }
  }

  // if we didn't find the specified user in our .users file or
  // if no username was given lets query in the splash window
  // for it.
  if(userIndex < 0)
  {
    // erase a possibly given password
    password = NULL;

    if(username != NULL)
    {
      // a user name was given at startup, but we didn't find it in our list
      MUI_Request(G->App, NULL, 0, tr(MSG_ER_USER_UNKNOWN_TITLE),
                                   tr(MSG_ER_USER_UNKNOWN_BUTTON),
                                   tr(MSG_ER_USER_UNKNOWN_ERROR), username);
      userIndex = -1;
    }
    else if(G->Users.Num >= 2)
    {
      // more than one users are available, let the user at the screen choose from these
      userIndex = DoMethod(G->SplashWinObject, MUIM_Splashwindow_SelectUser);
    }
    else
    {
      // no specific name was given, log in the first available user
      userIndex = 0;
    }
  }

  if(userIndex >= 0)
  {
    struct User *user = &G->Users.User[userIndex];

    G->Users.CurrentID = user->ID;
    strlcpy(G->MA_MailDir, maildir ? maildir : user->MailDir, sizeof(G->MA_MailDir));

    if(prefsfile != NULL)
      strlcpy(G->CO_PrefsFile, prefsfile, sizeof(G->CO_PrefsFile));
    else
      AddPath(G->CO_PrefsFile, G->MA_MailDir, ".config", sizeof(G->CO_PrefsFile));

    AddPath(G->AB_Filename, user->UseAddr ? G->ProgDir : G->MA_MailDir, ".addressbook", sizeof(G->AB_Filename));
    AddPath(G->DI_Filename, user->UseDict ? G->ProgDir : G->MA_MailDir, ".glossary", sizeof(G->DI_Filename));

    if(user->Password[0] != '\0')
    {
      // prompt for a password, if none was given by tooltypes/command line
      if(password != NULL)
        loggedin = (strcmp(password, user->Password) == 0 || strcmp(password, "\01") == 0);
      else
        loggedin = DoMethod(G->SplashWinObject, MUIM_Splashwindow_PasswordRequest, user);
    }
    else
    {
      // this user has no password, so we signal success
      loggedin = TRUE;
    }
  }

  RETURN(loggedin);
  return loggedin;
}

///
/// US_DelFunc
//  Removes a user from the user database
HOOKPROTONHNONP(US_DelFunc, void)
{
  Object *lv;
  int index;
  int m;
  struct User *user;

  ENTER();

  lv = G->US->GUI.LV_USERS;
  index = xget(lv, MUIA_NList_Active);
  DoMethod(lv, MUIM_NList_GetEntry, index, &user);

  if(user->MailDir[0] != '\0')
  {
    m = MUI_Request(G->App, G->US->GUI.WI, 0, tr(MSG_MA_ConfirmReq), tr(MSG_US_RemoveReqGads), tr(MSG_US_RemoveReq));

    if(m == 1)
      DeleteMailDir(user->MailDir, TRUE);
  }
  else
  {
    // just remove the user, as this doesn't have an own mail directory
    m = 2;
  }

  if(m != 0)
  {
    DoMethod(lv, MUIM_NList_Remove, index);
    // reactivate the Add/Del buttons
    set(G->US->GUI.BT_ADD, MUIA_Disabled, FALSE);
    set(G->US->GUI.BT_DEL, MUIA_Disabled, xget(lv, MUIA_NList_Entries) == 0);
  }

  LEAVE();
}
MakeStaticHook(US_DelHook, US_DelFunc);

///
/// US_AddFunc
//  Adds a new user to the user database
HOOKPROTONHNONP(US_AddFunc, void)
{
  struct US_GUIData *gui;
  int n;

  ENTER();

  gui = &G->US->GUI;
  n = xget(gui->LV_USERS, MUIA_NList_Entries);

  if(n < MAXUSERS)
  {
    struct User user;

    memset(&user, 0, sizeof(struct User));
    user.Limited = TRUE;
    user.IsNew = TRUE;

    DoMethod(gui->LV_USERS, MUIM_NList_InsertSingle, &user, MUIV_NList_Insert_Bottom);
    set(gui->LV_USERS, MUIA_NList_Active, MUIV_NList_Active_Bottom);
    set(gui->WI, MUIA_Window_ActiveObject, gui->ST_USER);

    // check again if we reached the limit
    if(n + 1 == MAXUSERS)
      set(gui->BT_ADD, MUIA_Disabled, TRUE);
  }

  LEAVE();
}
MakeStaticHook(US_AddHook, US_AddFunc);

///
/// US_SaveUserList
//  Initializes configuration files for new users and saves the user database
static BOOL US_SaveUserList(void)
{
  BOOL result = TRUE;
  int i;

  ENTER();

  get(G->US->GUI.LV_USERS, MUIA_NList_Entries, &G->Users.Num);
  for(i = 0; i < G->Users.Num; i++)
  {
    struct User *user;
    BOOL validUser = FALSE;

    DoMethod(G->US->GUI.LV_USERS, MUIM_NList_GetEntry, i, &user);

    if(user != NULL)
    {
      memcpy(&G->Users.User[i], user, sizeof(G->Users.User[i]));

      if(user->Name[0] != '\0')
      {
        if(user->MailDir[0] != '\0')
        {
          enum FType type;

          if(ObtainFileInfo(user->MailDir, FI_TYPE, &type) == FALSE || type != FIT_DRAWER)
          {
            if(MUI_Request(G->App, G->US->GUI.WI, 0, tr(MSG_MA_MUsers), tr(MSG_YesNoReq), tr(MSG_US_DIRECTORY_DOESNT_EXIST), user->MailDir, user->Name))
            {
               if(CreateDirectory(user->MailDir) == TRUE)
                 validUser = TRUE;
               else
                 ER_NewError(tr(MSG_ER_CantCreateDir), user->MailDir);
            }
          }
          else
            validUser = TRUE;
        }
        else
          ER_NewError(tr(MSG_ER_MissingDirectory));
      }
      else
        ER_NewError(tr(MSG_ER_MissingName));
    }
    else
      break;

    if(validUser == TRUE)
    {
      if(user->Clone == TRUE && user->IsNew == TRUE)
      {
         char dest[SIZE_PATHFILE];

         // clone the
         AddPath(dest, user->MailDir, ".addressbook", sizeof(dest));
         CopyFile(dest, NULL, G->AB_Filename, NULL);

         AddPath(dest, user->MailDir, ".glossary", sizeof(dest));
         CopyFile(dest, NULL, G->DI_Filename, NULL);

         AddPath(dest, user->MailDir, ".config", sizeof(dest));
         CopyFile(dest, NULL, G->CO_PrefsFile, NULL);
      }
    }
    else
    {
      set(G->US->GUI.LV_USERS, MUIA_NList_Active, i);
      result = FALSE;
      // break out of the loop
      break;
    }
  }

  if(result == TRUE)
    US_SaveUsers();

  RETURN(result);
  return result;
}

///
/// US_OpenFunc
//  Opens and initializes user list window
HOOKPROTONHNONP(US_OpenFunc, void)
{
  ENTER();

  if(G->US == NULL)
  {
    struct User *user = US_GetCurrentUser();

    // if the first user doesn't have a real name yet then copy over the name from the configuration
    if(G->Users.User[0].Name[0] == '\0')
      strlcpy(G->Users.User[0].Name, C->RealName, sizeof(G->Users.User[0].Name));

    if((G->US = US_New(!user->Limited)) != NULL)
    {
      if(SafeOpenWindow(G->US->GUI.WI))
      {
        int i;

        // add the users to the listview
        for(i = 0; i < G->Users.Num; i++)
          DoMethod(G->US->GUI.LV_USERS, MUIM_NList_InsertSingle, &G->Users.User[i], MUIV_NList_Insert_Bottom);

        set(G->US->GUI.LV_USERS, MUIA_NList_Active, 0);
        set(G->US->GUI.BT_ADD, MUIA_Disabled, G->Users.Num == MAXUSERS);
        set(G->US->GUI.BT_DEL, MUIA_Disabled, G->Users.Num == 0);
      }
      else
        DisposeModulePush(&G->US);
    }
  }

  LEAVE();
}
MakeHook(US_OpenHook, US_OpenFunc);

///
/// US_CloseFunc
//  Closes user list window
HOOKPROTONHNONP(US_CloseFunc, void)
{
  ENTER();

  if(US_SaveUserList())
    DisposeModulePush(&G->US);

  LEAVE();
}
MakeStaticHook(US_CloseHook, US_CloseFunc);

///
/// US_GetUSEntryFunc
//  Fills form with data from selected list entry
HOOKPROTONHNONP(US_GetUSEntryFunc, void)
{
  struct US_GUIData *gui;
  int act;

  ENTER();

  gui = &G->US->GUI;
  act = xget(gui->LV_USERS, MUIA_NList_Active);

  if(act != MUIV_NList_Active_Off)
  {
    struct User *user;
    BOOL notallowed;
    BOOL iscurrent;
    BOOL limited;

    DoMethod(gui->LV_USERS, MUIM_NList_GetEntry, act, &user);

    limited = !G->US->Supervisor;
    iscurrent = (user->ID == G->Users.CurrentID);
    notallowed = limited && !iscurrent;

    nnset(gui->ST_USER,    MUIA_String_Contents, user->Name);
    nnset(gui->ST_MAILDIR, MUIA_String_Contents, user->MailDir);
    nnset(gui->ST_PASSWD,  MUIA_String_Contents, user->Password);
    nnset(gui->CH_USEADDR, MUIA_Selected, user->UseAddr);
    nnset(gui->CH_USEDICT, MUIA_Selected, user->UseDict);
    nnset(gui->CH_ROOT,    MUIA_Selected, !user->Limited);
    nnset(gui->CH_CLONE,   MUIA_Selected, user->Clone);
    set(gui->ST_USER,    MUIA_Disabled, notallowed);
    set(gui->CH_USEDICT, MUIA_Disabled, notallowed || !act);
    set(gui->CH_USEADDR, MUIA_Disabled, notallowed || !act);
    set(gui->CH_CLONE,   MUIA_Disabled, !user->IsNew || !act);
    set(gui->PO_MAILDIR, MUIA_Disabled, limited || !act);
    set(gui->CH_ROOT,    MUIA_Disabled, limited);
    set(gui->ST_PASSWD,  MUIA_Disabled, notallowed);
    set(gui->BT_DEL,     MUIA_Disabled, !act || iscurrent);
   }
   else
     DoMethod(G->App, MUIM_MultiSet, MUIA_Disabled, TRUE, gui->ST_USER,
                                                          gui->ST_PASSWD,
                                                          gui->PO_MAILDIR,
                                                          gui->CH_USEDICT,
                                                          gui->CH_USEADDR,
                                                          gui->CH_ROOT,
                                                          gui->CH_CLONE,
                                                          gui->BT_DEL,
                                                          NULL);

  LEAVE();
}
MakeStaticHook(US_GetUSEntryHook,US_GetUSEntryFunc);

///
/// US_PutUSEntryFunc
//  Fills form data into selected list entry
HOOKPROTONHNONP(US_PutUSEntryFunc, void)
{
  struct User *user = NULL;
  struct US_GUIData *gui;

  ENTER();

  gui = &G->US->GUI;

  DoMethod(gui->LV_USERS, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &user);
  if(user != NULL)
  {
    GetMUIString(user->Name, gui->ST_USER, sizeof(user->Name));
    GetMUIString(user->MailDir, gui->ST_MAILDIR, sizeof(user->MailDir));
    GetMUIString(user->Password, gui->ST_PASSWD, sizeof(user->Password));
    user->UseAddr =  GetMUICheck(gui->CH_USEADDR);
    user->UseDict =  GetMUICheck(gui->CH_USEDICT);
    user->Limited = !GetMUICheck(gui->CH_ROOT);
    user->Clone   =  GetMUICheck(gui->CH_CLONE);
    DoMethod(gui->LV_USERS, MUIM_NList_Redraw, MUIV_NList_Redraw_Active);
  }

  LEAVE();
}
MakeStaticHook(US_PutUSEntryHook,US_PutUSEntryFunc);
///

/*** GUI ***/
/// US_LV_ConFunc
//  User listview construction hook
HOOKPROTONHNO(US_LV_ConFunc, struct User *, struct User *user)
{
  struct User *entry;

  ENTER();

  entry = memdup(user, sizeof(*user));

  RETURN(entry);
  return entry;
}
MakeStaticHook(US_LV_ConHook, US_LV_ConFunc);

///
/// US_LV_DspFunc
//  User listview display hook
HOOKPROTONH(US_LV_DspFunc, long, char **array, struct User *entry)
{
  ENTER();

  if(entry != NULL)
  {
    array[0] = entry->Name;
    array[1] = entry->MailDir;
    if(entry->ID == G->Users.CurrentID)
      array[DISPLAY_ARRAY_MAX] = (char *)"\0338";
  }
  else
  {
    array[0] = (STRPTR)tr(MSG_US_TitleUserName);
    array[1] = (STRPTR)tr(MSG_US_TitleMailDir);
  }

  RETURN(0);
  return 0;
}
MakeStaticHook(US_LV_DspHook,US_LV_DspFunc);

///
/// US_New
//  Creates user list window
static struct US_ClassData *US_New(BOOL supervisor)
{
  struct US_ClassData *data;

  ENTER();

  if((data = calloc(1, sizeof(struct US_ClassData))) != NULL)
  {
    data->Supervisor = supervisor;
    data->GUI.WI = WindowObject,
       MUIA_Window_Title, tr(MSG_MA_MUsers),
       MUIA_HelpNode, "US_W",
       MUIA_Window_ID, MAKE_ID('U','S','E','R'),
       WindowContents, VGroup,
          Child, NListviewObject,
             MUIA_CycleChain, 1,
             MUIA_NListview_NList, data->GUI.LV_USERS = NListObject,
                InputListFrame,
                MUIA_NList_ConstructHook, &US_LV_ConHook,
                MUIA_NList_DestructHook, &GeneralDesHook,
                MUIA_NList_DisplayHook, &US_LV_DspHook,
            //  MUIA_NList_DisplayHook, &MA_LV_FDspFuncHook,
                MUIA_NList_TitleSeparator, TRUE,
                MUIA_NList_Title, TRUE,
                MUIA_NList_Format, "BAR,",
             End,
          End,
          Child, VGroup, GroupFrameT(tr(MSG_MA_Settings)),
             Child, ColGroup(2),
                Child, Label2(tr(MSG_US_UserName)),
                Child, data->GUI.ST_USER = MakeString(SIZE_NAME, tr(MSG_US_UserName)),
                Child, Label2(tr(MSG_US_Password)),
                Child, data->GUI.ST_PASSWD = MakePassString(tr(MSG_US_Password)),
                Child, Label2(tr(MSG_US_MailDirectory)),
                Child, data->GUI.PO_MAILDIR = PopaslObject,
                   MUIA_Popasl_Type,ASL_FileRequest,
                   MUIA_Popstring_String,data->GUI.ST_MAILDIR = MakeString(SIZE_PATH,tr(MSG_US_MailDirectory)),
                   MUIA_Popstring_Button,PopButton(MUII_PopDrawer),
                   ASLFR_DrawersOnly, TRUE,
                End,
             End,
             Child, VGroup,
                Child, MakeCheckGroup((Object **)&data->GUI.CH_USEADDR, tr(MSG_US_GlobalAddrBook)),
                Child, MakeCheckGroup((Object **)&data->GUI.CH_USEDICT, tr(MSG_US_GlobalDict)),
                Child, MakeCheckGroup((Object **)&data->GUI.CH_ROOT,tr(MSG_US_SuperVisor)),
             End,
             Child, VGroup,
                MUIA_ShowMe, supervisor,
                Child, MakeCheckGroup((Object **)&data->GUI.CH_CLONE, tr(MSG_US_CopyConfig)),
             End,
          End,
          Child, ColGroup(2),
             MUIA_ShowMe, supervisor,
             Child, data->GUI.BT_ADD = MakeButton(tr(MSG_US_AddUser)),
             Child, data->GUI.BT_DEL = MakeButton(tr(MSG_US_DelUser)),
          End,
       End,
    End;

    if(data->GUI.WI != NULL)
    {
      SetHelp(data->GUI.ST_USER   ,MSG_HELP_US_ST_USER);
      SetHelp(data->GUI.ST_MAILDIR,MSG_HELP_US_ST_MAILDIR);
      SetHelp(data->GUI.ST_PASSWD ,MSG_HELP_US_ST_PASSWD);
      SetHelp(data->GUI.CH_USEADDR,MSG_HELP_US_CH_USEADDR);
      SetHelp(data->GUI.CH_USEDICT,MSG_HELP_US_CH_USEDICT);
      SetHelp(data->GUI.CH_CLONE  ,MSG_HELP_US_CH_CLONE);
      SetHelp(data->GUI.CH_ROOT   ,MSG_HELP_US_CH_ROOT);
      SetHelp(data->GUI.BT_ADD    ,MSG_HELP_US_BT_ADD);
      SetHelp(data->GUI.BT_DEL    ,MSG_HELP_US_BT_DEL);
      DoMethod(G->App, OM_ADDMEMBER, data->GUI.WI);
      DoMethod(data->GUI.LV_USERS,  MUIM_Notify,MUIA_NList_Active,       MUIV_EveryTime,MUIV_Notify_Application,2,MUIM_CallHook,&US_GetUSEntryHook);
      DoMethod(data->GUI.ST_USER,   MUIM_Notify,MUIA_String_Contents,    MUIV_EveryTime,MUIV_Notify_Application,2,MUIM_CallHook,&US_PutUSEntryHook);
      DoMethod(data->GUI.ST_MAILDIR,MUIM_Notify,MUIA_String_Contents,    MUIV_EveryTime,MUIV_Notify_Application,2,MUIM_CallHook,&US_PutUSEntryHook);
      DoMethod(data->GUI.ST_PASSWD, MUIM_Notify,MUIA_String_Contents,    MUIV_EveryTime,MUIV_Notify_Application,2,MUIM_CallHook,&US_PutUSEntryHook);
      DoMethod(data->GUI.CH_USEADDR,MUIM_Notify,MUIA_Selected,           MUIV_EveryTime,MUIV_Notify_Application,2,MUIM_CallHook,&US_PutUSEntryHook);
      DoMethod(data->GUI.CH_USEDICT,MUIM_Notify,MUIA_Selected,           MUIV_EveryTime,MUIV_Notify_Application,2,MUIM_CallHook,&US_PutUSEntryHook);
      DoMethod(data->GUI.CH_ROOT,   MUIM_Notify,MUIA_Selected,           MUIV_EveryTime,MUIV_Notify_Application,2,MUIM_CallHook,&US_PutUSEntryHook);
      DoMethod(data->GUI.CH_CLONE,  MUIM_Notify,MUIA_Selected,           MUIV_EveryTime,MUIV_Notify_Application,2,MUIM_CallHook,&US_PutUSEntryHook);
      DoMethod(data->GUI.BT_ADD,    MUIM_Notify,MUIA_Pressed,            FALSE         ,MUIV_Notify_Application,2,MUIM_CallHook,&US_AddHook);
      DoMethod(data->GUI.BT_DEL,    MUIM_Notify,MUIA_Pressed,            FALSE         ,MUIV_Notify_Application,2,MUIM_CallHook,&US_DelHook);
      DoMethod(data->GUI.WI,        MUIM_Notify,MUIA_Window_CloseRequest,TRUE          ,MUIV_Notify_Application,2,MUIM_CallHook,&US_CloseHook);
    }
    else
    {
      free(data);
      data = NULL;
    }
  }

  RETURN(data);
  return data;
}
///

