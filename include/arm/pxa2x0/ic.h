/*
 * $Id$
 *
 * XScale PXA250/PXA210 Interrupt Control Registers
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
 *
 * Documentation:
 * [1] Intel Corporation, "Intel PXA250 and PXA210 Application Processors
 *     Developer's Manual", February 2002, Order Number: 278522-001
 * [2] Intel Corporation, "Intel PXA250 and PXA210 Application Processors
 *     Specification Update", April 2002, Order Number: 278534-004
 *
 */

#ifndef	PXA2X0_IC_H
#define	PXA2X0_IC_H

#ifndef uint32_t
typedef	unsigned int	uint32_t;
#endif

/* Interrupt Control Registers */

#define	IC_BASE		0x40D00000

typedef volatile struct IC_registers {
	uint32_t icip;
	uint32_t icmr;
	uint32_t iclr;
	uint32_t icfp;
	uint32_t icpr;
	uint32_t iccr;
} IC_registers;

#ifndef IC_pointer
#define	IC_pointer	((IC_registers*) IC_BASE)
#endif

#define	ICIP		IC_pointer->icip
#define	ICMR		IC_pointer->icmr
#define	ICLR		IC_pointer->iclr
#define	ICFP		IC_pointer->icfp
#define	ICPR		IC_pointer->icpr
#define	ICCR		IC_pointer->iccr

#endif	/* PXA2X0_IC_H */
