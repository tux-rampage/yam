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

#include <proto/exec.h>

#include "extrasrc.h"

#include "YAM.h"

#include "Rexx.h"

#include "Debug.h"

struct rxd_help
{
  long rc, rc2;
  struct {
    char *file;
  } arg;
};

void rx_help(UNUSED struct RexxHost *host, void **rxd, enum RexxAction action, UNUSED struct RexxMsg *rexxmsg)
{
  struct rxd_help *rd = *rxd;

  ENTER();

  switch(action)
  {
    case RXIF_INIT:
    {
      if((*rxd = AllocVecPooled(G->SharedMemPool, sizeof(*rd))) != NULL)
        ((struct rxd_help *)(*rxd))->rc = 0;
    }
    break;

    case RXIF_ACTION:
    {
      struct rxs_command *rxc;
      FILE *fp = NULL;
      FILE *out = stdout;

      if(rd->arg.file && (fp = fopen(rd->arg.file, "w")))
        out = fp;

      fprintf(out, "Commands for application \"YAM\"\n\nCommand              Template\n-------              --------\n");

      for(rxc = rxs_commandlist; rxc->command; rxc++)
      {
        fprintf(out, "%-20s%s%s%s%s%s%s\n", rxc->command,
                                            (rxc->results || rxc->args) ? " " : "", rxc->results ? "VAR/K,STEM/K" : "",
                                            (rxc->results && rxc->args) ? "," : "", rxc->args ? rxc->args : "",
                                            rxc->results ? " => " : "", rxc->results ? rxc->results : "");
      }

      if(fp)
        fclose(fp);
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
