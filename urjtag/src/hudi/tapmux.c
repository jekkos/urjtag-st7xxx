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

#define TMC_IR_IDCODE    0x2
#define TMC_IR_TESTMODE  0x8

#define TMC_IR_LEN       5
#define TMC_TESTMODE_LEN 5

static urj_tap_register_t *rir = NULL;
static urj_tap_register_t *wir = NULL;

static void
tmc_jtag_manual(urj_chain_t *chain, int mask, int tck, int trst, int tms, int tdi, int reset)
{
  int val;

  val = 0;

  if (mask & URJ_POD_CS_TCK) {
    if (tck) val |= URJ_POD_CS_TCK;
  }

  if (mask & URJ_POD_CS_TRST) {
    if (trst) val |= URJ_POD_CS_TRST;
  }

  if (mask & URJ_POD_CS_TMS) {
    if (tms) val |= URJ_POD_CS_TMS;
  }

  if (mask & URJ_POD_CS_TDI) {
    if (tdi) val |= URJ_POD_CS_TDI;
  }

  reset = (reset) ? 1 : 0;
  if (mask & URJ_POD_CS_RESET) {
    if (reset) val |= URJ_POD_CS_RESET;
  }

  if (urj_tap_chain_set_pod_signal (chain, mask, val) == -1) {
    printf("%s : WARNING : failed to set pod signal.\n", __FUNCTION__);
    return;
  }

  usleep(20);

  printf("\n");
}

void tmc_init(void)
{
  if (rir == NULL) rir = urj_tap_register_fill(urj_tap_register_alloc(TMC_IR_LEN), 0);
  if (wir == NULL) wir = urj_tap_register_fill(urj_tap_register_alloc(TMC_IR_LEN), 0);
}

void tmc_free(void)
{
  if (rir) urj_tap_register_free(rir);
  if (wir) urj_tap_register_free(wir);
  rir = wir = NULL;
}

void tmc_reset_tmc(urj_chain_t *chain)
{
  // Reset TMC to bypass to TMC
  // jtag tck=00 ntrst=01 tms=00 tdo=00
  urj_tap_chain_set_trst(chain, 0);
  urj_tap_chain_set_trst(chain, 1);
}

void tmc_reset_and_bypass(urj_chain_t *chain, int channel)
{
  int mask = URJ_POD_CS_TCK | URJ_POD_CS_TMS | URJ_POD_CS_TDI;
  int tmsBP, tdoBP;

  switch(channel) {
  case 0: tmsBP = 0; tdoBP = 0; break;
  case 1: tmsBP = 0; tdoBP = 1; break;
  case 2: tmsBP = 1; tdoBP = 0; break;
  case 3: tmsBP = 1; tdoBP = 1; break;
  default:
    printf("Illegal tapmux channel %d.", channel);
    return;
  }

  urj_tap_chain_set_trst(chain, 0);
  tmc_jtag_manual(chain, mask, 0, 1, 0,     0,     0);
  tmc_jtag_manual(chain, mask, 0, 1, 0,     1,     0);
  tmc_jtag_manual(chain, mask, 1, 1, 0,     1,     0);
  tmc_jtag_manual(chain, mask, 0, 1, 0,     1,     0);
  urj_tap_chain_set_trst(chain, 1);
  tmc_jtag_manual(chain, mask, 0, 0, 0,     1,     0);
  tmc_jtag_manual(chain, mask, 0, 0, tmsBP, tdoBP, 0);
  tmc_jtag_manual(chain, mask, 1, 0, tmsBP, tdoBP, 0);
  tmc_jtag_manual(chain, mask, 0, 0, tmsBP, tdoBP, 0);
  tmc_jtag_manual(chain, mask, 0, 0, 0,     0,     0);
}

static void tmc_bypass_to_st40(urj_chain_t *chain)
{
  int mask = URJ_POD_CS_TCK | URJ_POD_CS_TMS | URJ_POD_CS_TDI;
  // ## Reset TapMux and then bypass to ST40
  // jtag tck=01010 ntrst=00011 tms=00000 tdo=01111
  urj_tap_chain_set_trst(chain, 0);
  tmc_jtag_manual(chain, mask, 0, 1, 0, 0, 0);
  tmc_jtag_manual(chain, mask, 1, 1, 0, 1, 0);
  tmc_jtag_manual(chain, mask, 0, 1, 0, 1, 0);
  urj_tap_chain_set_trst(chain, 1);
  tmc_jtag_manual(chain, mask, 1, 0, 0, 1, 0);
  tmc_jtag_manual(chain, mask, 0, 0, 0, 1, 0);
}

#define URJ_POD_CS_ASEBRK URJ_POD_CS_RESET

static void tmc_soc_reset_hold(urj_chain_t *chain, int reset)
{
  // ## Reset STb7100 leaving ST40 in reset hold
  if (reset) {
    tmc_jtag_manual(chain, URJ_POD_CS_TRST,   0, 0, 0, 0, 1);
    usleep(50);
    tmc_jtag_manual(chain, URJ_POD_CS_ASEBRK, 0, 0, 0, 0, 1);
    usleep(50);
    tmc_jtag_manual(chain, URJ_POD_CS_ASEBRK, 0, 0, 0, 0, 0);
    usleep(50);
    tmc_jtag_manual(chain, URJ_POD_CS_TRST,   0, 1, 0, 0, 0);
    usleep(50);
  } else {
    tmc_jtag_manual(chain, URJ_POD_CS_ASEBRK, 0, 0, 0, 0, 1);
    usleep(50);
    tmc_jtag_manual(chain, URJ_POD_CS_ASEBRK, 0, 0, 0, 0, 0);
    usleep(50);
  }
}

static void tmc_test_logic_reset(urj_chain_t *chain)
{
  urj_tap_chain_clock (chain, 1, 0, 5);
}

void tmc_run_test_idle(urj_chain_t *chain)
{
  tmc_test_logic_reset(chain);
  urj_tap_chain_clock (chain, 0, 0, 1);
}

static uint32_t tmc_readIR(urj_chain_t *chain)
{
  urj_tap_capture_ir(chain);
  wir = urj_tap_register_fill(wir, 1);
  urj_tap_shift_register(chain, wir, rir, URJ_CHAIN_EXITMODE_IDLE);
  return urj_tap_register_get_value(rir);
}

static void tmc_writeIR(urj_chain_t *chain, unsigned int val)
{
  urj_tap_capture_ir(chain);
  urj_tap_register_set_value(wir, val);
  urj_tap_shift_register(chain, wir, NULL, URJ_CHAIN_EXITMODE_IDLE);
}

#define tmc_writeIR_IDCODE(chain)   tmc_writeIR(chain,0x2);
#define tmc_writeIR_TESTMODE(chain) tmc_writeIR(chain,0x8);

static uint32_t tmc_readDR(urj_chain_t *chain, int size, uint32_t wdata)
{
  urj_tap_register_t *rwr = urj_tap_register_alloc(size);
  urj_tap_register_t *rrd = urj_tap_register_fill(urj_tap_register_alloc(size), 0);
  uint32_t rdata;
  urj_tap_register_set_value(rwr, wdata);
  urj_tap_capture_dr(chain);
  urj_tap_shift_register(chain, rwr, rrd, URJ_CHAIN_EXITMODE_IDLE);
  rdata = urj_tap_register_get_value(rrd);
  urj_tap_register_free(rwr);
  urj_tap_register_free(rrd);
  return rdata;
}

static void tmc_writeDR(urj_chain_t *chain, int size, uint32_t wdata)
{
  urj_tap_register_t *rwr = urj_tap_register_alloc(size);
  urj_tap_register_set_value(rwr, wdata);
  urj_tap_capture_dr(chain);
  urj_tap_shift_register(chain, rwr, NULL, URJ_CHAIN_EXITMODE_IDLE);
  urj_tap_register_free(rwr);
}

#define tmc_printIR(chain,name)     printf("%s(%d) = 0x%08x", name, 5,   tmc_readIR(chain))
#define tmc_printDR(chain,name,len) printf("%s(%d) = 0x%08x", name, len, tmc_readDR(chain, size, 0))

static void tmc_select_deviceid(urj_chain_t *chain, int sel, int zerolen)
{
  if (zerolen == 0) zerolen = 512;
  tmc_writeIR_TESTMODE(chain);
  tmc_writeDR(chain, zerolen, 0);
  tmc_writeDR(chain, TMC_TESTMODE_LEN, sel);
}

uint32_t tmc_read_device_id(urj_chain_t *chain, int id)
{
  uint32_t devid;
  tmc_run_test_idle(chain);
  tmc_select_deviceid(chain, id, 0);
  tmc_writeIR_IDCODE(chain);
  devid = tmc_readDR(chain, 32, id);
  tmc_select_deviceid(chain, 0, 0);
  return devid;
}

uint32_t tmc_read_chip_id(urj_chain_t *chain)
{
  uint32_t chipid;
  tmc_run_test_idle(chain);
  tmc_writeIR(chain, 0x4);
  chipid = tmc_readDR(chain, 32, 0);
  return chipid;
}

#define tmc_printRegister(chain,name,len) { tmc_writeIR_##name(chain); tmc_printDR(chain, name, len); }
#define tmc_read_DeviceId(chain)          tmc_read_device_id(chain, 0)
#define tmc_read_PrivateId(chain)         tmc_read_device_id(chain, 4)
#define tmc_read_OptionId(chain)          tmc_read_device_id(chain, 2)
#define tmc_read_ExtraId(chain)           tmc_read_device_id(chain, 6)
