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

void     tmc_init(void);
void     tmc_free(void);
void     tmc_reset_tmc(urj_chain_t *chain);
void     tmc_run_test_idle(urj_chain_t *chain);
void     tmc_reset_and_bypass(urj_chain_t *chain, int channel);
uint32_t tmc_read_device_id(urj_chain_t *chain, int id);
uint32_t tmc_read_chip_id(urj_chain_t *chain);

#define tmc_read_DeviceId(chain)          tmc_read_device_id(chain, 0)
#define tmc_read_PrivateId(chain)         tmc_read_device_id(chain, 4)
#define tmc_read_OptionId(chain)          tmc_read_device_id(chain, 2)
#define tmc_read_ExtraId(chain)           tmc_read_device_id(chain, 6)
