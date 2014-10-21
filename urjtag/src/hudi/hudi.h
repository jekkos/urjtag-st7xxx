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

#include <stdint.h>
#include <urjtag/chain.h>

void     hudi_init(void);
void     hudi_free(void);
void     hudi_run_test_idle(urj_chain_t *chain);
uint32_t hudi_readSDIR(urj_chain_t *chain);
void     hudi_writeSDIR(urj_chain_t *chain, uint32_t wdata);
void     hudi_writeSDIR_Extest(urj_chain_t *chain);
void     hudi_writeSDIR_SamplePreLoad(urj_chain_t *chain);
void     hudi_writeSDIR_InternalStatusReadStart(urj_chain_t *chain);
void     hudi_writeSDIR_InternalStatusReadEnd(urj_chain_t *chain);
void     hudi_writeSDIR_TDOTimingChange(urj_chain_t *chain);
void     hudi_writeSDIR_WaitCancel(urj_chain_t *chain);
void     hudi_writeSDIR_AseramWrite(urj_chain_t *chain);
void     hudi_writeSDIR_ResetNegate(urj_chain_t *chain);
void     hudi_writeSDIR_ResetAssert(urj_chain_t *chain);
void     hudi_writeSDIR_Boot(urj_chain_t *chain);
void     hudi_writeSDIR_Interrupt(urj_chain_t *chain);
void     hudi_writeSDIR_Break(urj_chain_t *chain);
void     hudi_writeSDIR_Bypass(urj_chain_t *chain);
void     hudi_initialState(urj_chain_t *chain);
uint32_t hudi_readSDDR(urj_chain_t *chain, uint32_t wdata);
uint32_t hudi_readSDSR(urj_chain_t *chain);
void     hudi_writeSDDR(urj_chain_t *chain, uint32_t wdata);
void     hudi_hardReset(urj_chain_t *chain);
void     hudi_softReset(urj_chain_t *chain);
void     hudi_bootHardReset(urj_chain_t *chain);
void     hudi_bootSoftReset(urj_chain_t *chain);
void     hudi_readInternalStatus(urj_chain_t *chain, uint32_t *data);
void     hudi_printInternalStatus(urj_chain_t *chain);

int      hudi_load_stdi_file(const char* filename, size_t offset);
int      hudi_stdi_attach(urj_chain_t *chain);
int      hudi_stdi_detach(urj_chain_t *chain);
