/////////////////////////////////////////////////////////////////////////
// $Id: instrument.cc,v 1.32 2009-02-09 10:35:55 vruppert Exp $
/////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2001  MandrakeSoft S.A.
//
//    MandrakeSoft S.A.
//    43, rue d'Aboukir
//    75002 Paris - France
//    http://www.linux-mandrake.com/
//    http://www.mandrakesoft.com/
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA


#include "bochs.h"

#if BX_INSTRUMENTATION

void bx_instr_init_env(void) {}
void bx_instr_exit_env(void) {}

void bx_instr_initialize(unsigned cpu) {}
void bx_instr_exit(unsigned cpu) {}
void bx_instr_reset(unsigned cpu, unsigned type) {}
void bx_instr_hlt(unsigned cpu) {}
void bx_instr_mwait(unsigned cpu, bx_phy_address addr, unsigned len, Bit32u flags) {}
void bx_instr_new_instruction(unsigned cpu) {}

void bx_instr_debug_promt() {}
void bx_instr_start() {}
void bx_instr_stop() {}
void bx_instr_print() {}

void bx_instr_cnear_branch_taken(unsigned cpu, bx_address new_eip) {}
void bx_instr_cnear_branch_not_taken(unsigned cpu) {}
void bx_instr_ucnear_branch(unsigned cpu, unsigned what, bx_address new_eip) {}
void bx_instr_far_branch(unsigned cpu, unsigned what, Bit16u new_cs, bx_address new_eip) {}

void bx_instr_opcode(unsigned cpu, const Bit8u *opcode, unsigned len, bx_bool is32, bx_bool is64) {}

void bx_instr_interrupt(unsigned cpu, unsigned vector) {}
void bx_instr_exception(unsigned cpu, unsigned vector, unsigned error_code) {}
void bx_instr_hwinterrupt(unsigned cpu, unsigned vector, Bit16u cs, bx_address eip) {}

void bx_instr_tlb_cntrl(unsigned cpu, unsigned what, bx_phy_address new_cr3) {}
void bx_instr_clflush(unsigned cpu, bx_address laddr, bx_phy_address paddr) {}
void bx_instr_cache_cntrl(unsigned cpu, unsigned what) {}
void bx_instr_prefetch_hint(unsigned cpu, unsigned what, unsigned seg, bx_address offset) {}

void bx_instr_before_execution(unsigned cpu, bxInstruction_c *i) {}
void bx_instr_after_execution(unsigned cpu, bxInstruction_c *i) {}
void bx_instr_repeat_iteration(unsigned cpu, bxInstruction_c *i) {}

void bx_instr_inp(Bit16u addr, unsigned len) {}
void bx_instr_inp2(Bit16u addr, unsigned len, unsigned val) {}
void bx_instr_outp(Bit16u addr, unsigned len, unsigned val) {}

void bx_instr_mem_data_access(unsigned cpu, unsigned seg, bx_address offset, unsigned len, unsigned rw) {}
void bx_instr_lin_access(unsigned cpu, bx_address lin, bx_address phy, unsigned len, unsigned rw) {}

void bx_instr_phy_write(unsigned cpu, bx_address addr, unsigned len) {}
void bx_instr_phy_read(unsigned cpu, bx_address addr, unsigned len) {}

void bx_instr_wrmsr(unsigned cpu, unsigned addr, Bit64u value) {}

#endif
