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

#include <libraries/mui.h>
#include <proto/exec.h>

#include "extrasrc.h"

#include "YAM.h"
#include "YAM_write.h"

#include "FileInfo.h"
#include "Rexx.h"

#include "Debug.h"

struct rxd_writeattach
{
  long rc, rc2;
  struct {
    char *file;
    char *desc;
    char *encmode;
    char *ctype;
  } arg;
};

void rx_writeattach(UNUSED struct RexxHost *host, void **rxd, enum RexxAction action, UNUSED struct RexxMsg *rexxmsg)
{
  struct rxd_writeattach *rd = *rxd;

  ENTER();

  switch(action)
  {
    case RXIF_INIT:
    {
      if((*rxd = AllocVecPooled(G->SharedMemPool, sizeof(*rd))) != NULL)
        ((struct rxd_writeattach *)(*rxd))->rc = 0;
    }
    break;

    case RXIF_ACTION:
    {
      if(FileExists(rd->arg.file) == TRUE && G->WR[G->ActiveWriteWin])
      {
        WR_AddFileToList((int)G->ActiveWriteWin, rd->arg.file, NULL, FALSE);

        if(rd->arg.desc)    setstring(G->WR[G->ActiveWriteWin]->GUI.ST_DESC, rd->arg.desc);
        if(rd->arg.encmode) setmutex (G->WR[G->ActiveWriteWin]->GUI.RA_ENCODING, !strnicmp(rd->arg.encmode, "uu", 2) ? 1 : 0);
        if(rd->arg.ctype)   setstring(G->WR[G->ActiveWriteWin]->GUI.ST_CTYPE, rd->arg.ctype);
      }
      else
        rd->rc = RETURN_ERROR;
    }
    break;

    case RXIF_FREE:
    {
      FreeVecPooled(G->SharedMemPool, rd);
    }
    break;
  }

  LEAVE();
}
