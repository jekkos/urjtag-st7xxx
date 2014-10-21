/*
 * $Id$
 *
 * Copyright (C) 2003 ETC s.r.o.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by Marcel Telka <marcel@telka.sk>, 2003.
 *
 */

#include <sysdep.h>

#include <stdio.h>
#include <string.h>

#include <urjtag/error.h>
#include <urjtag/tap.h>
#include <urjtag/error.h>

#include <urjtag/cmd.h>
#include <urjtag/tap_register.h>

#include "cmd.h"

//
// tapmux.c
//

#include "../hudi/tapmux.h"
#include "../hudi/tapmux.c"

//
// hudi.c
//

#include "../hudi/hudi.h"
#include "../hudi/hudi.c"

//
//
//

#include "../hudi/dsu.h"
#include "../hudi/dsu.c"

//
// cmd_tapmux.c
//

static void
tmc_print_ids(urj_chain_t *chain, int channel)
{
  urj_tap_register_t *reg = urj_tap_register_alloc(32);

  tmc_init();
  hudi_init();

  tmc_reset_tmc(chain);
 
  printf("DeviceId  = 0x%08x\n", tmc_read_DeviceId(chain));
  printf("PrivateId = 0x%08x\n", tmc_read_PrivateId(chain));
  printf("OptionId  = 0x%08x\n", tmc_read_OptionId(chain));
  printf("ExtraId   = 0x%08x\n", tmc_read_ExtraId(chain));
  printf("ChipId    = 0x%08x\n", tmc_read_chip_id(chain));

  tmc_reset_and_bypass(chain, channel);

  hudi_initialState(chain);
  hudi_printInternalStatus(chain);
  printf("SDSR = 0x%08x\n", hudi_readSDSR(chain));

  urj_tap_register_free(reg);
}

static int
stdi_subcommand(urj_chain_t *chain, int n, int m, char *params[])
{
  for(; n<m; n++) {
    if (strncasecmp(params[n], "file", 4) == 0) {
      char*  filename;
      size_t offset;
      if ((n+1) == m || (n+2) == m)
	urj_error_set (URJ_ERROR_SYNTAX, "missing filename for stdifile and/or overlays offset");
      filename = params[++n];
      offset   = strtoul(params[++n], NULL, 16);
      return hudi_load_stdi_file(filename, offset);
    } 
    else if (strncasecmp(params[n], "attach", 6) == 0) {
      return hudi_stdi_attach(chain);
    }
    else if (strncasecmp(params[n], "detach", 6) == 0) {
      return hudi_stdi_detach(chain);
    }
    else {
      urj_error_set (URJ_ERROR_SYNTAX, "illegal stdi subcommand '%s'", params[n]);
      return URJ_STATUS_FAIL;
    }
  }
  return URJ_STATUS_FAIL;
}

static int
dsu_subcommand(urj_chain_t *chain, int n, int m, char *params[])
{

  for(; n<m; n++) {
    if (strncasecmp(params[n], "dbreak", 6) == 0) {
    } 
    else if (strncasecmp(params[n], "dpeek", 5) == 0) {
      int reg, len;
      if ((n+1) == m) {
	reg = 0;
	len = 32;
      } 
      else if ((n+2) == m) {
	reg = atoi(params[++n]);
	len = 1;
      }
      else {
	reg = atoi(params[++n]);
	len = atoi(params[++n]);
      }

      printf("DSU dpeek reg = %d, len = %d\n", reg, len);

      len += reg;
      for(;reg<len;reg++)
	printf("DSU%02d = %08x\n", reg, dsu_dpeek(chain,reg));
      return URJ_STATUS_OK;
    } 
    else if (strncasecmp(params[n], "dpoke", 5) == 0) {
    } 
    else if (strncasecmp(params[n], "flush", 5) == 0) {
    }
    else if (strncasecmp(params[n], "ibreak", 6) == 0) {
    }
    else {
      urj_error_set (URJ_ERROR_SYNTAX, "illegal dsu subcommand '%s'", params[n]);
      return URJ_STATUS_FAIL;
    }
  }

  return URJ_STATUS_FAIL;
}

static int tmc_current_channel = 0;

static int
cmd_tapmux_run (urj_chain_t *chain, char *params[])
{
  int i,j;
  if ((i = urj_cmd_params (params)) < 2)
    {
      urj_error_set (URJ_ERROR_SYNTAX,
		     "%s: #parameters should be >= %d, not %d",
		     params[0], 2, urj_cmd_params (params));
      return URJ_STATUS_FAIL;
    }

  if (urj_cmd_test_cable (chain) != URJ_STATUS_OK)
    return URJ_STATUS_FAIL;

  tmc_init();
  hudi_init();

  for(j = 1; j < i; j++) {
    if (strncasecmp(params[j], "bypass", 6) == 0) {
      if ((j+1) == i)
	urj_error_set (URJ_ERROR_SYNTAX, "missing channel number for bypass");
      tmc_current_channel = atoi(params[++j]);
      tmc_reset_and_bypass(chain, tmc_current_channel);
      return URJ_STATUS_OK;
    } else if (strncasecmp(params[j], "printids", 8) == 0) {
      tmc_print_ids(chain, tmc_current_channel);
      return URJ_STATUS_OK;
    } else if (strncasecmp(params[j], "stdi", 4) == 0) {
      if ((j+1) == i)
	urj_error_set (URJ_ERROR_SYNTAX, "missing subcommand for stdi");
      j++;
      return stdi_subcommand(chain,j,i,params);
    } else if (strncasecmp(params[j], "dsu", 3) == 0) {
      if ((j+1) == i)
	urj_error_set (URJ_ERROR_SYNTAX, "missing subcommand for dsu");
      j++;
      return dsu_subcommand(chain,j,i,params);
    } else {
      urj_error_set (URJ_ERROR_SYNTAX, "illegal subcommand '%s'", params[j]);
      return URJ_STATUS_FAIL;
    }
  }
    
  //  hudi_free();
  //  tmc_free();

  return URJ_STATUS_FAIL;
}

static void
cmd_tapmux_help (void)
{
    urj_log (URJ_LOG_LEVEL_NORMAL,
             _("Usage: tapmux bypass N\n"
	       "Usage: tapmux printids\n"
	       "Usage: tapmux stdi [...]\n"
	       "Usage: tapmux dsu [..]\n"
               "bypass   : bypass tmc to core N (N=0-3).\n"
               "printids : print ids of current core (st40 only).\n"
	       "stdi     : stdi sub commands (st40 only).\n"
	       " stdi file FILE OFFSET : load stmc stdi file w/ given overlay OFFSET.\n"
	       " stdi attach           : attach to st40.\n"
	       " stdi detach           : detach from st40.\n"
               "dsu      : dsu sub commands (st231 only).\n"
	       " dsu dpeek             : read and print all dsu registers.\n"
	       " dsu dpeek REG         : read and print dsu register REG 0-31.\n"
	       " dsu dpeek REG NUMREGS : read and print NUMREG dsu registers starting from REG.\n"
               "\n"));
}

const urj_cmd_t urj_cmd_tapmux = {
    "tapmux",
    N_("control TAPmux unit"),
    cmd_tapmux_help,
    cmd_tapmux_run
};
