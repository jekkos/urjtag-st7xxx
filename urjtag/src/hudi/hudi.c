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
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <err.h>

#include <urjtag/pod.h>
#include <urjtag/chain.h>
#include <urjtag/cable.h>
#include <urjtag/tap.h>
#include <urjtag/tap_register.h>

#include <elfutils/libdw.h>
#include <dwarf.h>
#include <libelf.h>
#include <gelf.h>

static int stmmxmode          = 0;
static int hudi_sdmode        = 0;
static int hudi_sdmode_saved  = 0;
static int hudi_sdmode_locked = 0;

static uint32_t hudi_status[16];

typedef enum HUDI_STATUS_REGISTER_ID
{
  SR      = 0,
  FPSCR   = 1,
  CCR     = 2,
  FRQCR   = 3,
  EXPEVT  = 4,
  INTEVT  = 5,
  EBUS    = 6,
  IBUS    = 7,
  SBUS    = 8,
  EBTYPE  = 9,
  SBTYPE  = 10,
  CMF     = 11,
  SCMF    = 12,
  MMUCRAT = 13,
  PTEH    = 14,
  STATUS  = 15,
} hudi_status_id_t;

#define HUDI_CODE_IBUS   (0x08 << 3)
#define HUDI_CODE_SR     (0x04 << 3)
#define HUDI_CODE_FPSCR  (0x0c << 3)
#define HUDI_CODE_CMF    (0x02 << 3)
#define HUDI_CODE_PTEH   (0x0a << 3)
#define HUDI_CODE_CCR    (0x06 << 3)
#define HUDI_CODE_SBTYPE (0x0e << 3)
#define HUDI_CODE_SBUS   (0x01 << 3)
#define HUDI_CODE_EBUS   (0x09 << 3)
#define HUDI_CODE_FRQCR  (0x00 << 3)

#define HUDI_RSDIR_LEN 32
#define HUDI_WSDIR_LEN 8

static urj_tap_register_t *rsdir  = NULL;
static urj_tap_register_t *wsdir  = NULL;

void hudi_init(void)
{
  if (rsdir  == NULL) rsdir  = urj_tap_register_fill(urj_tap_register_alloc(HUDI_RSDIR_LEN), 0);
  if (wsdir  == NULL) wsdir  = urj_tap_register_fill(urj_tap_register_alloc(HUDI_WSDIR_LEN), 0);
}

void hudi_free(void)
{
  if (rsdir  != NULL) urj_tap_register_free(rsdir);
  if (wsdir  != NULL) urj_tap_register_free(wsdir);
  rsdir = wsdir = NULL;
}

static void hudi_test_logic_reset(urj_chain_t *chain)
{
  urj_tap_chain_clock (chain, 1, 0, 5);
  hudi_sdmode_locked = 1;
  hudi_sdmode = 1;
}

void hudi_run_test_idle(urj_chain_t *chain)
{
  hudi_test_logic_reset(chain);
  urj_tap_chain_clock (chain, 0, 0, 1);
}

uint32_t hudi_readSDIR(urj_chain_t *chain)
{
  urj_tap_register_t *ones = urj_tap_register_fill(urj_tap_register_alloc(HUDI_RSDIR_LEN), 1);
  urj_tap_capture_ir(chain);
  urj_tap_shift_register(chain, ones, rsdir, URJ_CHAIN_EXITMODE_IDLE);
  hudi_sdmode_locked = 1;
  hudi_sdmode = 1;
  return urj_tap_register_get_value(rsdir);
}

void hudi_writeSDIR(urj_chain_t *chain, uint32_t wdata)
{
  urj_tap_capture_ir(chain);
  urj_tap_register_set_value(wsdir, wdata);
  urj_tap_shift_register(chain, wsdir, NULL, URJ_CHAIN_EXITMODE_IDLE);
}

void hudi_writeSDIR_Extest(urj_chain_t *chain)
{
  hudi_writeSDIR(chain, 0x00);
  hudi_sdmode_locked = 1;
  hudi_sdmode = 1;
}

void hudi_writeSDIR_SamplePreLoad(urj_chain_t *chain)
{
  hudi_writeSDIR(chain, 0x04);
  hudi_sdmode_locked = 1;
  hudi_sdmode = 1;
}

void hudi_writeSDIR_InternalStatusReadStart(urj_chain_t *chain)
{
  hudi_writeSDIR(chain, 0x20);
  hudi_sdmode_locked = 1;
  hudi_sdmode_saved = hudi_sdmode;
  hudi_sdmode = 1;
}

void hudi_writeSDIR_InternalStatusReadEnd(urj_chain_t *chain)
{
  hudi_writeSDIR(chain, 0x21);
  hudi_sdmode_locked = 0;
  hudi_sdmode = hudi_sdmode_saved;
}

void hudi_writeSDIR_TDOTimingChange(urj_chain_t *chain)
{
  hudi_writeSDIR(chain, 0x22);
  hudi_sdmode_locked = 0;
}

void hudi_writeSDIR_WaitCancel(urj_chain_t *chain)
{
  hudi_writeSDIR(chain, 0x23);
  hudi_sdmode_locked = 0;
}

void hudi_writeSDIR_AseramWrite(urj_chain_t *chain)
{
  hudi_writeSDIR(chain, 0x50);
  hudi_sdmode_locked = 0;
}

void hudi_writeSDIR_ResetNegate(urj_chain_t *chain)
{
  hudi_writeSDIR(chain, 0x60);
  hudi_sdmode_locked = 0;
}

void hudi_writeSDIR_ResetAssert(urj_chain_t *chain)
{
  hudi_writeSDIR(chain, 0x70);
  hudi_sdmode_locked = 0;
}

void hudi_writeSDIR_Boot(urj_chain_t *chain)
{
  hudi_writeSDIR(chain, 0x80);
  hudi_sdmode_locked = 0;
}

void hudi_writeSDIR_Interrupt(urj_chain_t *chain)
{
  hudi_writeSDIR(chain, 0xa0);
  hudi_sdmode_locked = 1;
  hudi_sdmode = 1;
}

void hudi_writeSDIR_Break(urj_chain_t *chain)
{
  hudi_writeSDIR(chain, 0xc0);
  hudi_sdmode_locked = 0;
}

void hudi_writeSDIR_Bypass(urj_chain_t *chain)
{
  hudi_writeSDIR(chain, 0xff);
  hudi_sdmode_locked = 1;
  hudi_sdmode = 1;
}

void hudi_initialState(urj_chain_t *chain)
{
  hudi_run_test_idle(chain);
  hudi_writeSDIR_ResetNegate(chain);
}

static uint32_t hudi_readSDDRorSDSR(urj_chain_t *chain, uint32_t wdata)
{
  urj_tap_register_t *rwr = urj_tap_register_alloc(32);
  urj_tap_register_t *rrd = urj_tap_register_fill(urj_tap_register_alloc(32), 0);
  uint32_t rdata;
  urj_tap_register_set_value(rwr,wdata);
  urj_tap_capture_dr(chain);
  urj_tap_shift_register(chain, rwr, rrd, URJ_CHAIN_EXITMODE_IDLE);
  rdata = urj_tap_register_get_value(rrd);
  urj_tap_register_free(rwr);
  urj_tap_register_free(rrd);
  return rdata;
}

static void hudi_writeSDDRorSDSR(urj_chain_t *chain, uint32_t wdata)
{
  urj_tap_register_t *rwr = urj_tap_register_alloc(32);
  urj_tap_register_set_value(rwr, wdata);
  urj_tap_capture_dr(chain);
  urj_tap_shift_register(chain, rwr, NULL, URJ_CHAIN_EXITMODE_IDLE);
  urj_tap_register_free(rwr);
  if (hudi_sdmode_locked == 0) {
    if (hudi_sdmode == 1) hudi_sdmode = 0;
    else hudi_sdmode = (wdata & 1);
  }
}

uint32_t hudi_readSDDR(urj_chain_t *chain, uint32_t wdata)
{
  if (hudi_sdmode == 0) hudi_initialState(chain);
  return hudi_readSDDRorSDSR(chain,wdata);
}

uint32_t hudi_readSDSR(urj_chain_t *chain)
{
  if (hudi_sdmode == 1) {
    hudi_initialState(chain);
    hudi_readSDDRorSDSR(chain,0);
  }
  return hudi_readSDDRorSDSR(chain,0);
}

void hudi_writeSDDR(urj_chain_t *chain, uint32_t wdata)
{
  if (hudi_sdmode == 0) hudi_initialState(chain);
  hudi_writeSDDRorSDSR(chain, wdata);
}

#define hudi_printSDIR(chain)     printf("%s(%d) = 0x%08x\n", "SDIR", 32, hudi_readSDIR(chain))
#define hudi_printSDDR(chain)     printf("%s(%d) = 0x%08x\n", "SDDR", 32, hudi_readSDDR(chain,0))
#define hudi_printSDSR(chain)     printf("%s(%d) = 0x%08x\n", "SDSR", 32, hudi_readSDSR(chain))

void hudi_hardReset(urj_chain_t *chain)
{
  tmc_jtag_manual(chain, URJ_POD_CS_TRST, 0, 0, 0, 0, 0);
  usleep(20);
  tmc_jtag_manual(chain, URJ_POD_CS_TRST, 0, 1, 0, 0, 0);
}

void hudi_softReset(urj_chain_t *chain)
{
  hudi_writeSDIR_ResetAssert(chain);
  usleep(20);
  hudi_writeSDIR_ResetNegate(chain);
}

void hudi_bootHardReset(urj_chain_t *chain)
{
  tmc_jtag_manual(chain, URJ_POD_CS_TRST,   0, 0, 0, 0, 0);
  usleep(20);
  tmc_jtag_manual(chain, URJ_POD_CS_ASEBRK, 0, 0, 0, 0, 0);
  usleep(20);
  tmc_jtag_manual(chain, URJ_POD_CS_ASEBRK, 0, 0, 0, 0, 1);
  usleep(20);
  tmc_jtag_manual(chain, URJ_POD_CS_TRST,   0, 1, 0, 0, 0);
  usleep(20);
}

void hudi_bootSoftReset(urj_chain_t *chain)
{
  hudi_writeSDIR_ResetAssert(chain);
  usleep(20);
}

void hudi_readInternalStatus(urj_chain_t *chain, uint32_t *data)
{
  uint32_t temp;
  
  hudi_writeSDIR_InternalStatusReadStart(chain);
 
  data[IBUS]    = hudi_readSDDR(chain, HUDI_CODE_IBUS   );
  data[SR]      = hudi_readSDDR(chain, HUDI_CODE_SR     );
  data[FPSCR]   = hudi_readSDDR(chain, HUDI_CODE_FPSCR  );
  temp          = hudi_readSDDR(chain, HUDI_CODE_CMF    );
  data[CMF]     = temp & 0x3f;
  data[SCMF]    = (temp >> 6) & 0xf;
  temp          = hudi_readSDDR(chain, HUDI_CODE_PTEH   );
  data[PTEH]    = temp & 0xff;
  data[EXPEVT]  = (temp >>  8) & 0xfff;
  data[INTEVT]  = ((temp >> 20) & 0xfff) << 2;
  temp          = hudi_readSDDR(chain, HUDI_CODE_CCR    );
  data[CCR]     = temp & 0x7f;
  data[MMUCRAT] = (temp >> 7) & 0x1;
  temp          = hudi_readSDDR(chain, HUDI_CODE_SBTYPE );
  data[SBTYPE]  = temp & 0xf;
  data[EBTYPE]  = (temp >> 4) & 0x7f;
  data[SBUS]    = hudi_readSDDR(chain, HUDI_CODE_SBUS   );
  temp          = hudi_readSDDR(chain, HUDI_CODE_EBUS   );
  data[EBUS]    = temp & 0x1fffffff;
  temp          = hudi_readSDDR(chain, HUDI_CODE_FRQCR  );
  data[FRQCR]   = temp & 0xfff;
  data[STATUS]  = (temp >> 12) & 0x3;

  hudi_writeSDIR_InternalStatusReadEnd(chain);
}

void hudi_printInternalStatus(urj_chain_t *chain)
{
  hudi_readInternalStatus(chain, hudi_status);
  printf("========================================\n");
  printf("HUDI Internal Status\n");
  printf("----------------------------------------\n");
  printf("SR       = 0x%08x\n",   hudi_status[SR]);
  printf("FPSCR    = 0x%08x\n\n", hudi_status[FPSCR]);
  printf("CCR      = 0x%02x\n",   hudi_status[CCR]);
  printf("FRQCR    = 0x%04x\n",   hudi_status[FRQCR]);
  printf("EXPEVT   = 0x%04x\n",   hudi_status[EXPEVT]);
  printf("INTEVT   = 0x%04x\n\n", hudi_status[INTEVT]);
  printf("EBUS     = 0x%08x\n",   hudi_status[EBUS]);
  printf("IBUS     = 0x%08x\n",   hudi_status[IBUS]);
  printf("SBUS     = 0x%08x\n",   hudi_status[SBUS]);
  printf("EBTYPE   = 0x%02x\n",   hudi_status[EBTYPE]);
  printf("SBTYPE   = 0x%02x\n\n", hudi_status[SBTYPE]);
  printf("CMF      = 0x%02x\n",   hudi_status[CMF]);
  printf("SCMF     = 0x%02x\n",   hudi_status[SCMF]);
  printf("MMUCR.AT = 0x%02x\n",   hudi_status[MMUCRAT]);
  printf("PTEH     = 0x%02x\n\n", hudi_status[PTEH]);
  printf("STATUS   = 0x%02x\n",   hudi_status[STATUS]);
  printf("========================================\n");
}

//
//
//

#define EM_ST200 0x64

struct overlay_segment {
  uint32_t  boot;
  uint32_t  size;
  uint32_t *code;
};

#define STDI_OVERLAY_BOOT_RESET      1
#define STDI_OVERLAY_BOOT_INIT       2
#define STDI_OVERLAY_PEEK_BYTE       3
#define STDI_OVERLAY_PEEK_BYTES      4
#define STDI_OVERLAY_PEEK_WORD       5
#define STDI_OVERLAY_PEEK_WORDS      6
#define STDI_OVERLAY_PEEK_LONG       7
#define STDI_OVERLAY_PEEK_LONGS      8
#define STDI_OVERLAY_POKE_BYTE       9
#define STDI_OVERLAY_POKE_BYTES      10
#define STDI_OVERLAY_POKE_WORD       11
#define STDI_OVERLAY_POKE_WORDS      12
#define STDI_OVERLAY_POKE_LONG       13
#define STDI_OVERLAY_POKE_LONGS      14
#define STDI_OVERLAY_GET_CPU_REGS    17
#define STDI_OVERLAY_GET_FPU_REGS    18
#define STDI_OVERLAY_SET_CPU_REGS    19
#define STDI_OVERLAY_SET_FPU_REGS    20
#define STDI_OVERLAY_CONTINUE        21
#define STDI_OVERLAY_CALL            22
#define STDI_OVERLAY_SINGLE_STEP     23
#define STDI_OVERLAY_MMU_MAP         24
#define STDI_OVERLAY_DETACH          25
#define STDI_OVERLAY_L2CACHE_PURGE   26

static char* hudi_stdi_overlay_name(int n)
{
  switch(n) {
    case  0: return "none";           break;
    case  1: return "boot/reset";     break;
    case  2: return "boot/init";      break;
    case  3: return "peek byte";      break;
    case  4: return "peek bytes";     break;
    case  5: return "peek word";      break;
    case  6: return "peek words";     break;
    case  7: return "peek long";      break;
    case  8: return "peek longs";     break;
    case  9: return "poke byte";      break;
    case 10: return "poke bytes";     break;
    case 11: return "poke word";      break;
    case 12: return "poke words";     break;
    case 13: return "poke long";      break;
    case 14: return "poke longs";     break;
    case 15: return "unknown";        break;
    case 16: return "unknown";        break;
    case 17: return "get cpu regs";   break;
    case 18: return "get fpu regs";   break;
    case 19: return "set cpu regs";   break;
    case 20: return "set fpu regs";   break;
    case 21: return "continue";       break;
    case 22: return "call";           break;
    case 23: return "single step";    break;
    case 24: return "mmu map";        break;
    case 25: return "detach";         break;
    case 26: return "l2 cache purge"; break;
  }
  return "unknown";
}

static int                    stdi_current_overlay = 0;
static int                    stdi_attached        = 0;
static int                    stdi_overlays_loaded = 0;
static struct overlay_segment stdi_overlays[0x1a];

static int hudi_stdi_interrupt_target(urj_chain_t *chain)
{
  printf("%s :: not yet!\n", __FUNCTION__);
  return 0;
}

static int hudi_stdi_wait_asemode(urj_chain_t *chain)
{
  printf("%s :: not yet!\n", __FUNCTION__);
  return 0;
}

static int hudi_stdi_init_asemode(urj_chain_t *chain)
{
  printf("%s :: not yet!\n", __FUNCTION__);
  return 0;
}

static int hudi_stdi_service_asemode(urj_chain_t *chain, int n)
{
  uint32_t data;
  data = hudi_readSDDRorSDSR(chain, 0x0);
  printf("%s :: data = %08x\n", __FUNCTION__, data);
  data = hudi_readSDDRorSDSR(chain, 0x2);
  printf("%s :: data = %08x\n", __FUNCTION__, data);

  printf("%s :: not yet!\n", __FUNCTION__);
  return 0;
}

static int hudi_stdi_write_aseram(urj_chain_t *chain, uint32_t boot, uint32_t size, const uint32_t* code)
{
  uint32_t sdar, start, end;
  start = boot & 0x3ff;
  end   = start + (size << 2) -2;
  sdar  = (end << 16) | start;
  hudi_writeSDIR_AseramWrite(chain);

  hudi_writeSDDR(chain, sdar);
  for(;size--;)
    hudi_writeSDDR(chain, *code++);

  hudi_writeSDIR_Bypass(chain);
  return 0;
}

static int hudi_stdi_load_overlay(urj_chain_t *chain, int n)
{
  if (n == stdi_current_overlay)
    return 0;

  stdi_current_overlay = 0;

  if (n == 0 || stdi_overlays[n].code == NULL)
    return 1;

  if (hudi_stdi_write_aseram(chain, stdi_overlays[n].boot, stdi_overlays[n].size, stdi_overlays[n].code)) {
    urj_error_set (URJ_ERROR_SYNTAX, "unable write aseram with overlay %s", hudi_stdi_overlay_name(n));
    return 1;
  }

  urj_log(URJ_LOG_LEVEL_NORMAL,"loaded overlay %s to st40\n", hudi_stdi_overlay_name(n));

  stdi_current_overlay = n;

  return 0;
}

static int hudi_stdi_start_overlay(urj_chain_t *chain, int n, uint32_t sig, uint32_t size, uint32_t *buf)
{
  if (hudi_stdi_load_overlay(chain, n)) {
    urj_error_set (URJ_ERROR_SYNTAX, "unable to send overlay %s to st40", hudi_stdi_overlay_name(n));
    return 1;
  }

  if (size && buf) {
    if (hudi_stdi_write_aseram(chain, 0xfc0002a0, size, buf)) {
      urj_error_set (URJ_ERROR_SYNTAX, "unable to send buffer %p/%d to st40", buf, size);
      return 1;
    }
  }

  if (hudi_stdi_write_aseram(chain, 0xfc0003fc, 1, &sig)) {
    urj_error_set (URJ_ERROR_SYNTAX, "unable to send signal %08x to st40", sig);
    return 1;
  }
  
  hudi_writeSDIR_ResetNegate(chain);
  return 0;
}

int hudi_stdi_detach(urj_chain_t *chain)
{
  printf("%s :: not yet!\n", __FUNCTION__);
  return URJ_STATUS_OK;
}

int hudi_stdi_attach(urj_chain_t *chain)
{
  if (stdi_overlays_loaded == 0) {
    urj_error_set (URJ_ERROR_SYNTAX, "overlays not loaded. run stdifile first.");
    return URJ_STATUS_FAIL;
  }

  if (stdi_attached)
    hudi_stdi_detach(chain);

  hudi_stdi_start_overlay(chain, STDI_OVERLAY_BOOT_INIT, 0, 0, NULL);

  hudi_stdi_service_asemode(chain, STDI_OVERLAY_BOOT_INIT);

  hudi_stdi_load_overlay(chain, STDI_OVERLAY_BOOT_RESET);

  hudi_writeSDIR_ResetNegate(chain);

  hudi_stdi_interrupt_target(chain);

  // max 5x sdiSleep

  hudi_writeSDIR_Boot(chain);

  hudi_stdi_init_asemode(chain);

  hudi_stdi_wait_asemode(chain);

  // max 2x sdiSleep
  
  hudi_writeSDIR_ResetAssert(chain);

  return URJ_STATUS_OK;
}

static void hudi_free_stdi_file(void)
{
  int i;
  for(i=0; i <= 0x1a; i++)
    if (stdi_overlays[i].code) free(stdi_overlays[i].code);
  stdi_overlays_loaded = 0;
}

int hudi_load_stdi_file(const char* filename, size_t offset)
{
  Elf      *elf;
  Elf_Scn  *elfsection;
  GElf_Ehdr elfhdr;
  GElf_Shdr elfsechdr;
  int       fd, i, j;
  char     *secname;
  size_t    elfsecidx;
  uint32_t  dsec_addr, dsec_offs, dsec_size;
  char     *buf;
  uint32_t *ovldesc;

  if (stdi_overlays_loaded)
    hudi_free_stdi_file();

  if (elf_version(EV_CURRENT) == EV_NONE) {
    urj_error_set (URJ_ERROR_SYNTAX, "failed to initialize ELF library (%s)", elf_errmsg(-1));
    return URJ_STATUS_FAIL;
  }
    
  fd = open(filename, O_RDONLY, 0);
  if (fd < 0) {
    urj_error_set (URJ_ERROR_SYNTAX, "failed to open stmc2 stdi code for st40 from %s (%d,%s)", filename, errno, strerror(errno));
    return URJ_STATUS_FAIL;
  }

  elf = elf_begin(fd, ELF_C_READ, NULL);
  if (elf == NULL) {
    urj_error_set (URJ_ERROR_SYNTAX, "elf_begin failed (%s)", elf_errmsg(-1));
    return URJ_STATUS_FAIL;
  }

  if (elf_kind(elf) != ELF_K_ELF) {
    urj_error_set(URJ_ERROR_SYNTAX, "stdi file is not an elf object");
    return URJ_STATUS_FAIL;
  }

  //
  //

  if (gelf_getclass(elf) != ELFCLASS32) {
    urj_error_set(URJ_ERROR_SYNTAX,"unexpected ELF class, should be 32-bit ELF");
    return URJ_STATUS_FAIL;
  }

  if (gelf_getehdr(elf, &elfhdr) == NULL) {
    urj_error_set(URJ_ERROR_SYNTAX,"failed to get ELF header (%s)", elf_errmsg(-1));
    return URJ_STATUS_FAIL;
  }

  if (elfhdr.e_machine != EM_ST200) {
    urj_error_set(URJ_ERROR_SYNTAX,"unexpected ELF machine %d, should be EM_ST200 (%d)", elfhdr.e_machine, EM_ST200);
    return URJ_STATUS_FAIL;
  }

  if (elf_getshdrstrndx(elf, &elfsecidx) != 0) {
    urj_error_set(URJ_ERROR_SYNTAX,"failed to get section index (%s)", elf_errmsg(-1));
    return URJ_STATUS_FAIL;
  }

  dsec_addr = dsec_offs = dsec_size = 0;
  elfsection = NULL ;
  while ((elfsection = elf_nextscn(elf, elfsection)) != NULL) {
    if (gelf_getshdr(elfsection, &elfsechdr) != &elfsechdr) {
      urj_error_set(URJ_ERROR_SYNTAX,"failed to get section (%s)", elf_errmsg(-1));
      return URJ_STATUS_FAIL;
    }

    if ((secname = elf_strptr(elf, elfsecidx, elfsechdr.sh_name)) == NULL ) {
      urj_error_set(URJ_ERROR_SYNTAX,"failed to get section string ptr (%s)", elf_errmsg(-1));
      return URJ_STATUS_FAIL;
    }

    if (strcmp(secname, ".data") == 0) {
      dsec_size = (uint32_t)elfsechdr.sh_size;
      dsec_offs = (uint32_t)elfsechdr.sh_offset;
      dsec_addr = (uint32_t)elfsechdr.sh_addr;
    }
  }

  elf_end(elf);

  buf = malloc(dsec_size);
  if (buf == NULL) {
    urj_error_set(URJ_ERROR_SYNTAX,"failed to allocate memory for data section buffer.");
    goto load_failed;
  }
  
  lseek(fd, dsec_offs, SEEK_SET);
  if (dsec_size != read(fd, buf, dsec_size)) {
    urj_error_set(URJ_ERROR_SYNTAX,"failed to read data section (%d,%s).", errno, strerror(errno));
    goto load_failed;
  }

  close(fd);

  ovldesc = (uint32_t*)(buf+offset-dsec_addr);
  for(i=0; i<=0x1a; i++) {
    uint32_t *segments;
    stdi_overlays[i].boot = 0;
    stdi_overlays[i].size = 0;
    stdi_overlays[i].code = NULL;

    if (ovldesc[i] == 0)
      continue;
    segments = (uint32_t*)(buf + ovldesc[i] - dsec_addr);
    for(j=0; segments[j] != 0; j++) {
      struct overlay_segment *segdesc;
      segdesc = (struct overlay_segment*)(buf + segments[j] - dsec_addr);
      stdi_overlays[i].boot = segdesc->boot;
      stdi_overlays[i].size = segdesc->size;
      stdi_overlays[i].code = malloc(sizeof(uint32_t)*segdesc->size);
      if (stdi_overlays[i].code == NULL) {
	urj_error_set(URJ_ERROR_SYNTAX,"failed to allocate memory for overlay segment (%d,%s).", errno, strerror(errno));
	goto load_failed;
      }
      memcpy(stdi_overlays[i].code, buf+((uint32_t)segdesc->code)-dsec_addr, sizeof(uint32_t)*stdi_overlays[i].size);

      printf("loaded overlay %02d %-16s .boot=%08x .size=%08x\n", i, hudi_stdi_overlay_name(i), stdi_overlays[i].boot, stdi_overlays[i].size);
    }
  }

  stdi_overlays_loaded = 1;

  free(buf);
  return URJ_STATUS_OK;

 load_failed:
  if (buf) free(buf);
  if (fd) close(fd);
  hudi_free_stdi_file();
  return URJ_STATUS_FAIL;
}
