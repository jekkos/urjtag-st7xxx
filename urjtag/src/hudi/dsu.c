/* Copyright (C) 2010 urjtag.
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
 */

#include <sysdep.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <urjtag/pod.h>
#include <urjtag/chain.h>
#include <urjtag/cable.h>
#include <urjtag/tap_register.h>

#define DSU_IR_LEN 5

static urj_tap_register_t *dsurir = NULL;
static urj_tap_register_t *dsuwir = NULL;

void dsu_init(void)
{
  if (dsurir == NULL) dsurir = urj_tap_register_fill(urj_tap_register_alloc(DSU_IR_LEN), 0);
  if (dsuwir == NULL) dsuwir = urj_tap_register_fill(urj_tap_register_alloc(DSU_IR_LEN), 0);
}

void dsu_free(void)
{
  if (dsurir) urj_tap_register_free(dsurir);
  if (dsuwir) urj_tap_register_free(dsuwir);
  dsurir = dsuwir = NULL;
}

static void dsu_test_logic_reset(urj_chain_t *chain)
{
  urj_tap_chain_clock (chain, 1, 0, 5);
}

static void dsu_run_test_idle(urj_chain_t *chain)
{
  dsu_test_logic_reset(chain);
  urj_tap_chain_clock (chain, 0, 0, 1);
}

static uint32_t dsu_readIR(urj_chain_t *chain)
{
  urj_tap_capture_ir(chain);
  wir = urj_tap_register_fill(wir, 1);
  urj_tap_shift_register(chain, wir, dsurir, URJ_CHAIN_EXITMODE_IDLE);
  return urj_tap_register_get_value(dsurir);
}

static void dsu_writeIR(urj_chain_t *chain, int size, unsigned int val)
{
  urj_tap_register_t *rwr = urj_tap_register_alloc(size);
  urj_tap_register_set_value(rwr, val); 
  urj_tap_capture_ir(chain); 
  urj_tap_shift_register(chain, rwr, NULL, URJ_CHAIN_EXITMODE_IDLE); 
  urj_tap_register_free(rwr);

  /* urj_tap_capture_ir(chain); */
  /* urj_tap_register_set_value(wir, val); */
  /* urj_tap_shift_register(chain, wir, NULL, URJ_CHAIN_EXITMODE_IDLE); */
}

#define dsu_writeIR_DSUWRITE(chain,n) dsu_writeIR(chain,0x1);
#define dsu_writeIR_DPOKE(chain) dsu_writeIR(chain,0x8);

static uint64_t dsu_readDR(urj_chain_t *chain, int size, uint64_t wdata)
{
  urj_tap_register_t *rwr = urj_tap_register_alloc(size);
  urj_tap_register_t *rrd = urj_tap_register_fill(urj_tap_register_alloc(size), 0);
  uint64_t rdata;
  urj_tap_register_set_value(rwr, wdata);
  urj_tap_capture_dr(chain);
  urj_tap_shift_register(chain, rwr, rrd, URJ_CHAIN_EXITMODE_IDLE);
  rdata = urj_tap_register_get_value(rrd);
  urj_tap_register_free(rwr);
  urj_tap_register_free(rrd);
  return rdata;
}

static void dsu_writeDR(urj_chain_t *chain, int size, uint64_t wdata)
{
  urj_tap_register_t *rwr = urj_tap_register_alloc(size);
  urj_tap_register_set_value(rwr, wdata);
  urj_tap_capture_dr(chain);
  urj_tap_shift_register(chain, rwr, NULL, URJ_CHAIN_EXITMODE_IDLE);
  urj_tap_register_free(rwr);
}

uint32_t dsu_dpeek(urj_chain_t *chain, int reg)
{
  int i, n;
  uint64_t cmd = 0, mask = 0, rcmd = 0, rmask = 0;

  dsu_run_test_idle(chain);
  for(n=1;n<=40;n++) {
    dsu_writeIR(chain, n, 1);
    
    cmd  = (reg << 3) | 0x19;
    mask = 1;
    mask <<= 39;
    rmask = mask;
    for(i=0;i<40;i++) {
      rcmd >>= 1;
      if (cmd & mask) rcmd |= rmask;
      mask >>= 1;
    }
    
    dsu_writeDR(chain, 40, cmd);
    printf("n = %2d :: ==> DSU DR = %010Lx\n", n, cmd);
    for(i=0;i<5;i++) {
      printf("DSU DR = %010Lx\n", dsu_readDR(chain, 40, 0));
      usleep(1000);
    }
    printf("DSU DR = %010Lx\n", dsu_readDR(chain, 40, 0));
    
    dsu_writeDR(chain, 40, rcmd);
    printf("n = %2d :: ==> DSU DR = %010Lx\n", n, rcmd);
    for(i=0;i<5;i++) {
      printf("DSU DR = %010Lx\n", dsu_readDR(chain, 40, 0));
      usleep(1000);
    }
    printf("DSU DR = %010Lx\n", dsu_readDR(chain, 40, 0));
  }
  return 0;
}

void dsu_dpoke(urj_chain_t *chain, int reg, uint32_t data)
{
}
