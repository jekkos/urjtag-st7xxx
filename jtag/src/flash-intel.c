/*
 * $Id$
 *
 * Copyright (C) 2002 ETC s.r.o.
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
 * Written by Marcel Telka <marcel@telka.sk>, 2002.
 * Changed by August Hörandl, 2003
 *
 * Documentation:
 * [1] Advanced Micro Devices, "Common Flash Memory Interface Specification Release 2.0",
 *     December 1, 2001
 * [2] Intel Corporation, "Intel PXA250 and PXA210 Application Processors
 *     Developer's Manual", February 2002, Order Number: 278522-001
 * [3] Intel Corporation, "Common Flash Interface (CFI) and Command Sets
 *     Application Note 646", April 2000, Order Number: 292204-004
 * [4] Advanced Micro Devices, "Common Flash Memory Interface Publication 100 Vendor & Device
 *     ID Code Assignments", December 1, 2001, Volume Number: 96.1
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <flash/cfi.h>
#include <flash/intel.h>
#include <std/mic.h>

#include "flash.h"
#include "chain.h"

static int intel_flash_erase_block( chain_t *chain, uint32_t adr );
static int intel_flash_unlock_block( chain_t *chain, uint32_t adr );
static int intel_flash_program( chain_t *chain, uint32_t adr, uint32_t data );
static int intel_flash_erase_block32( chain_t *chain, uint32_t adr );
static int intel_flash_unlock_block32( chain_t *chain, uint32_t adr );
static int intel_flash_program32( chain_t *chain, uint32_t adr, uint32_t data );

/* autodetect, we can handle this chip */
static int 
intel_flash_autodetect32( chain_t *chain, cfi_query_structure_t *cfi )
{
	return (cfi->identification_string.pri_id_code == CFI_VENDOR_INTEL_ECS) && (bus_width( chain ) == 32);
}

static int 
intel_flash_autodetect( chain_t *chain, cfi_query_structure_t *cfi )
{
	return (cfi->identification_string.pri_id_code == CFI_VENDOR_INTEL_ECS) && (bus_width( chain ) == 16);
}

static
void _intel_flash_print_info( chain_t *chain, int o )
{
	uint32_t mid, cid;

	mid = (bus_read( chain, 0x00 << o ) & 0xFF);
	switch (mid) {
		case STD_MIC_INTEL:
			printf( "Manufacturer: %s\n", STD_MICN_INTEL );
			break;
		default:
			printf( "Unknown manufacturer (0x%04X)!\n", mid);
			break;
	}

	printf( "Chip: " );
	cid = (bus_read( chain, 0x01 << o ) & 0xFFFF);
	switch (cid) {
		case 0x0016:
			printf( "28F320J3A\n" );
			break;
		case 0x0017:
			printf( "28F640J3A\n" );
			break;
		case 0x0018:
			printf( "28F128J3A\n" );
			break;
		case 0x8801:
			printf( "28F640K3\n" );
			break;
		case 0x8802:
			printf( "28F128K3\n" );
			break;
		case 0x8803:
			printf( "28F256K3\n" );
			break;
		case 0x8805:
			printf( "28F640K18\n" );
			break;
		case 0x8806:
			printf( "28F128K18\n" );
			break;
		case 0x8807:
			printf( "28F256K18\n" );
			break;
		default:
			printf( "Unknown (0x%02X)!\n", cid );
			break;
	}

	/* Read Array */
	bus_write( chain, 0 << o, 0x00FF00FF );
}

static 
void intel_flash_print_info( chain_t *chain )
{
	int o = 1;
	/* Intel Primary Algorithm Extended Query Table - see Table 5. in [3] */
	/* TODO */

	/* Clear Status Register */
	bus_write( chain, 0 << o, 0x0050 );

	/* Read Identifier Command */
	bus_write( chain, 0 << 0, 0x0090 );

	_intel_flash_print_info( chain, o );
}

static 
void intel_flash_print_info32( chain_t *chain )
{
	int o = 2;
	/* Intel Primary Algorithm Extended Query Table - see Table 5. in [3] */
	/* TODO */

	/* Clear Status Register */
	bus_write( chain, 0 << o, 0x00500050 );

	/* Read Identifier Command */
	bus_write( chain, 0 << 0, 0x00900090 );

	_intel_flash_print_info( chain, o );
}

static int
intel_flash_erase_block( chain_t *chain, uint32_t adr )
{
	uint16_t sr;

	bus_write( chain, 0, CFI_INTEL_CMD_CLEAR_STATUS_REGISTER );
	bus_write( chain, adr, CFI_INTEL_CMD_BLOCK_ERASE );
	bus_write( chain, adr, CFI_INTEL_CMD_CONFIRM );

	while (!((sr = bus_read( chain, 0 ) & 0xFE) & CFI_INTEL_SR_READY)) ; 		/* TODO: add timeout */

	switch (sr & ~CFI_INTEL_SR_READY) {
		case 0:
			return 0;
		case CFI_INTEL_SR_ERASE_ERROR | CFI_INTEL_SR_PROGRAM_ERROR:
			printf("flash: invalid command seq\n");
			return CFI_INTEL_ERROR_INVALID_COMMAND_SEQUENCE;
		case CFI_INTEL_SR_ERASE_ERROR | CFI_INTEL_SR_VPEN_ERROR:
			printf("flash: low vpen\n");
			return CFI_INTEL_ERROR_LOW_VPEN;
		case CFI_INTEL_SR_ERASE_ERROR | CFI_INTEL_SR_BLOCK_LOCKED:
			printf("flash: block locked\n");
			return CFI_INTEL_ERROR_BLOCK_LOCKED;
		default:
			break;
	}

	return CFI_INTEL_ERROR_UNKNOWN;
}

static int
intel_flash_unlock_block( chain_t *chain, uint32_t adr )
{
	uint16_t sr;

	bus_write( chain, 0, CFI_INTEL_CMD_CLEAR_STATUS_REGISTER );
	bus_write( chain, adr, CFI_INTEL_CMD_LOCK_SETUP );
	bus_write( chain, adr, CFI_INTEL_CMD_UNLOCK_BLOCK );

	while (!((sr = bus_read( chain, 0 ) & 0xFE) & CFI_INTEL_SR_READY)) ; 		/* TODO: add timeout */

	if (sr != CFI_INTEL_SR_READY) {
		printf("flash: unknown error while unblocking\n");
		return CFI_INTEL_ERROR_UNKNOWN;
	} else
		return 0;
}

static int
intel_flash_program( chain_t *chain, uint32_t adr, uint32_t data )
{
	uint16_t sr;

	bus_write( chain, 0, CFI_INTEL_CMD_CLEAR_STATUS_REGISTER );
	bus_write( chain, adr, CFI_INTEL_CMD_PROGRAM1 );
	bus_write( chain, adr, data );

	while (!((sr = bus_read( chain, 0 ) & 0xFE) & CFI_INTEL_SR_READY)) ; 		/* TODO: add timeout */

	if (sr != CFI_INTEL_SR_READY) {
		printf("flash: unknown error while programming\n");
		return CFI_INTEL_ERROR_UNKNOWN;
	} else
		return 0;
}

static int
intel_flash_erase_block32( chain_t *chain, uint32_t adr )
{
	uint32_t sr;

	bus_write( chain, 0, (CFI_INTEL_CMD_CLEAR_STATUS_REGISTER << 16) | CFI_INTEL_CMD_CLEAR_STATUS_REGISTER );
	bus_write( chain, adr, (CFI_INTEL_CMD_BLOCK_ERASE << 16) | CFI_INTEL_CMD_BLOCK_ERASE );
	bus_write( chain, adr, (CFI_INTEL_CMD_CONFIRM << 16) | CFI_INTEL_CMD_CONFIRM );

	while (((sr = bus_read( chain, 0 ) & 0x00FE00FE) & ((CFI_INTEL_SR_READY << 16) | CFI_INTEL_SR_READY)) != ((CFI_INTEL_SR_READY << 16) | CFI_INTEL_SR_READY)) ; 		/* TODO: add timeout */

	if (sr != ((CFI_INTEL_SR_READY << 16) | CFI_INTEL_SR_READY)) {
		printf( "\nsr = 0x%08X\n", sr );
		return CFI_INTEL_ERROR_UNKNOWN;
	} else
		return 0;
}

static int
intel_flash_unlock_block32( chain_t *chain, uint32_t adr )
{
	uint32_t sr;

	bus_write( chain, 0, (CFI_INTEL_CMD_CLEAR_STATUS_REGISTER << 16) | CFI_INTEL_CMD_CLEAR_STATUS_REGISTER );
	bus_write( chain, adr, (CFI_INTEL_CMD_LOCK_SETUP << 16) | CFI_INTEL_CMD_LOCK_SETUP );
	bus_write( chain, adr, (CFI_INTEL_CMD_UNLOCK_BLOCK << 16) | CFI_INTEL_CMD_UNLOCK_BLOCK );

	while (((sr = bus_read( chain, 0 ) & 0x00FE00FE) & ((CFI_INTEL_SR_READY << 16) | CFI_INTEL_SR_READY)) != ((CFI_INTEL_SR_READY << 16) | CFI_INTEL_SR_READY)) ; 		/* TODO: add timeout */

	if (sr != ((CFI_INTEL_SR_READY << 16) | CFI_INTEL_SR_READY)) {
		printf( "\nsr = 0x%08X\n", sr );
		return CFI_INTEL_ERROR_UNKNOWN;
	} else
		return 0;
}

static int
intel_flash_program32( chain_t *chain, uint32_t adr, uint32_t data )
{
	uint32_t sr;

	bus_write( chain, 0, (CFI_INTEL_CMD_CLEAR_STATUS_REGISTER << 16) | CFI_INTEL_CMD_CLEAR_STATUS_REGISTER );
	bus_write( chain, adr, (CFI_INTEL_CMD_PROGRAM1 << 16) | CFI_INTEL_CMD_PROGRAM1 );
	bus_write( chain, adr, data );

	while (((sr = bus_read( chain, 0 ) & 0x00FE00FE) & ((CFI_INTEL_SR_READY << 16) | CFI_INTEL_SR_READY)) != ((CFI_INTEL_SR_READY << 16) | CFI_INTEL_SR_READY)) ; 		/* TODO: add timeout */

	if (sr != ((CFI_INTEL_SR_READY << 16) | CFI_INTEL_SR_READY)) {
		printf( "\nsr = 0x%08X\n", sr );
		return CFI_INTEL_ERROR_UNKNOWN;
	} else
		return 0;
}

static void
intel_flash_readarray32( chain_t *chain )
{
	/* Read Array */
	bus_write( chain, 0, 0x00FF00FF );
}

static void
intel_flash_readarray( chain_t *chain )
{
	/* Read Array */
	bus_write( chain, 0, 0x00FF00FF );
}

flash_driver_t intel_32_flash_driver = {
	4, /* buswidth */
	"Intel Standard Command Set",
	"supported: 28Fxxxx, 2 x 16 bit",
	intel_flash_autodetect32,
	intel_flash_print_info32,
	intel_flash_erase_block32,
	intel_flash_unlock_block32,
	intel_flash_program32,
	intel_flash_readarray32,
};

flash_driver_t intel_16_flash_driver = {
	2, /* buswidth */
	"Intel Standard Command Set",
	"supported: 28Fxxxx, 1 x 16 bit",
	intel_flash_autodetect,
	intel_flash_print_info,
	intel_flash_erase_block,
	intel_flash_unlock_block,
	intel_flash_program,
	intel_flash_readarray,
};