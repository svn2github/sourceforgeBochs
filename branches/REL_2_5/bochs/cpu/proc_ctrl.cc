/////////////////////////////////////////////////////////////////////////
// $Id$
/////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2001-2011  The Bochs Project
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
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA B 02110-1301 USA
//
/////////////////////////////////////////////////////////////////////////

#define NEED_CPU_REG_SHORTCUTS 1
#include "bochs.h"
#include "param_names.h"
#include "cpu.h"
#define LOG_THIS BX_CPU_THIS_PTR

BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::UndefinedOpcode(bxInstruction_c *i)
{
  BX_DEBUG(("UndefinedOpcode: generate #UD exception"));
  exception(BX_UD_EXCEPTION, 0);

  BX_NEXT_TRACE(i); // keep compiler happy
}

BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::NOP(bxInstruction_c *i)
{
  // No operation.

  BX_NEXT_INSTR(i);
}

BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::PAUSE(bxInstruction_c *i)
{
#if BX_SUPPORT_VMX
  if (BX_CPU_THIS_PTR in_vmx_guest)
    VMexit_PAUSE(i);
#endif

  BX_NEXT_INSTR(i);
}

BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::PREFETCH(bxInstruction_c *i)
{
#if BX_INSTRUMENTATION
  BX_INSTR_PREFETCH_HINT(BX_CPU_ID, i->nnn(), i->seg(),
     BX_CPU_CALL_METHODR(i->ResolveModrm, (i)));
#endif

  BX_NEXT_INSTR(i);
}

BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::CPUID(bxInstruction_c *i)
{
#if BX_CPU_LEVEL >= 4

#if BX_SUPPORT_VMX
  if (BX_CPU_THIS_PTR in_vmx_guest) {
    BX_ERROR(("VMEXIT: CPUID in VMX non-root operation"));
    VMexit(i, VMX_VMEXIT_CPUID, 0);
  }
#endif

  struct cpuid_function_t leaf;
  BX_CPU_THIS_PTR cpuid->get_cpuid_leaf(EAX, ECX, &leaf);

  RAX = leaf.eax;
  RBX = leaf.ebx;
  RCX = leaf.ecx;
  RDX = leaf.edx;
#endif

  BX_NEXT_INSTR(i);
}

//
// The shutdown state is very similar to the state following the exection
// if HLT instruction. In this mode the processor stops executing
// instructions until #NMI, #SMI, #RESET or #INIT is received. If
// shutdown occurs why in NMI interrupt handler or in SMM, a hardware
// reset must be used to restart the processor execution.
//
void BX_CPU_C::shutdown(void)
{
  BX_PANIC(("Entering to shutdown state still not implemented"));

  BX_CPU_THIS_PTR clear_IF();

  // artificial trap bit, why use another variable.
  BX_CPU_THIS_PTR activity_state = BX_ACTIVITY_STATE_HLT;
  BX_CPU_THIS_PTR async_event = 1; // so processor knows to check
  // Execution of this instruction completes.  The processor
  // will remain in a halt state until one of the above conditions
  // is met.

  BX_INSTR_HLT(BX_CPU_ID);

#if BX_DEBUGGER
  bx_dbg_halt(BX_CPU_ID);
#endif

#if BX_USE_IDLE_HACK
  bx_gui->sim_is_idle();
#endif

  longjmp(BX_CPU_THIS_PTR jmp_buf_env, 1); // go back to main decode loop
}

BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::HLT(bxInstruction_c *i)
{
  // CPL is always 0 in real mode
  if (/* !real_mode() && */ CPL!=0) {
    BX_DEBUG(("HLT: %s priveledge check failed, CPL=%d, generate #GP(0)",
        cpu_mode_string(BX_CPU_THIS_PTR cpu_mode), CPL));
    exception(BX_GP_EXCEPTION, 0);
  }

  if (! BX_CPU_THIS_PTR get_IF()) {
    BX_INFO(("WARNING: HLT instruction with IF=0!"));
  }

#if BX_SUPPORT_VMX
  if (BX_CPU_THIS_PTR in_vmx_guest)
    VMexit_HLT(i);
#endif

  // stops instruction execution and places the processor in a
  // HALT state.  An enabled interrupt, NMI, or reset will resume
  // execution.  If interrupt (including NMI) is used to resume
  // execution after HLT, the saved CS:eIP points to instruction
  // following HLT.

  // artificial trap bit, why use another variable.
  BX_CPU_THIS_PTR activity_state = BX_ACTIVITY_STATE_HLT;
  BX_CPU_THIS_PTR async_event = 1; // so processor knows to check
  // Execution of this instruction completes.  The processor
  // will remain in a halt state until one of the above conditions
  // is met.

  BX_INSTR_HLT(BX_CPU_ID);

#if BX_DEBUGGER
  bx_dbg_halt(BX_CPU_ID);
#endif

#if BX_USE_IDLE_HACK
  bx_gui->sim_is_idle();
#endif

  BX_NEXT_TRACE(i);
}

/* 0F 08 */
BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::INVD(bxInstruction_c *i)
{
  // CPL is always 0 in real mode
  if (/* !real_mode() && */ CPL!=0) {
    BX_ERROR(("INVD: priveledge check failed, generate #GP(0)"));
    exception(BX_GP_EXCEPTION, 0);
  }

#if BX_SUPPORT_VMX
  if (BX_CPU_THIS_PTR in_vmx_guest) {
    BX_ERROR(("VMEXIT: INVD in VMX non-root operation"));
    VMexit(i, VMX_VMEXIT_INVD, 0);
  }
#endif

  invalidate_prefetch_q();

  BX_DEBUG(("INVD: Flush internal caches !"));
  BX_INSTR_CACHE_CNTRL(BX_CPU_ID, BX_INSTR_INVD);

  flushICaches();

  BX_NEXT_TRACE(i);
}

/* 0F 09 */
BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::WBINVD(bxInstruction_c *i)
{
  // CPL is always 0 in real mode
  if (/* !real_mode() && */ CPL!=0) {
    BX_ERROR(("INVD/WBINVD: priveledge check failed, generate #GP(0)"));
    exception(BX_GP_EXCEPTION, 0);
  }

#if BX_SUPPORT_VMX
  if (BX_CPU_THIS_PTR in_vmx_guest)
    VMexit_WBINVD(i);
#endif

  invalidate_prefetch_q();

  BX_DEBUG(("WBINVD: Flush internal caches !"));
  BX_INSTR_CACHE_CNTRL(BX_CPU_ID, BX_INSTR_WBINVD);

  flushICaches();

  BX_NEXT_TRACE(i);
}

BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::CLFLUSH(bxInstruction_c *i)
{
  bx_segment_reg_t *seg = &BX_CPU_THIS_PTR sregs[i->seg()];

  bx_address eaddr = BX_CPU_CALL_METHODR(i->ResolveModrm, (i));
  bx_address laddr = BX_CPU_THIS_PTR get_laddr(i->seg(), eaddr);

#if BX_SUPPORT_X86_64
  if (BX_CPU_THIS_PTR cpu_mode == BX_MODE_LONG_64) {
    if (! IsCanonical(laddr)) {
      BX_ERROR(("CLFLUSH: non-canonical access !"));
      exception(int_number(i->seg()), 0);
    }
  }
  else
#endif
  {
    // check if we could access the memory segment
    if (!(seg->cache.valid & SegAccessROK)) {
      if (! execute_virtual_checks(seg, (Bit32u) eaddr, 1))
        exception(int_number(i->seg()), 0);
    }
    else {
      if (eaddr > seg->cache.u.segment.limit_scaled) {
        BX_ERROR(("CLFLUSH: segment limit violation"));
        exception(int_number(i->seg()), 0);
      }
    }
  }

#if BX_INSTRUMENTATION
  bx_phy_address paddr =
#endif
    translate_linear(laddr, USER_PL, BX_READ);

  BX_INSTR_CLFLUSH(BX_CPU_ID, laddr, paddr);

#if BX_X86_DEBUGGER
  hwbreakpoint_match(laddr, 1, BX_READ);
#endif

  BX_NEXT_INSTR(i);
}

void BX_CPU_C::handleCpuModeChange(void)
{
  unsigned mode = BX_CPU_THIS_PTR cpu_mode;

#if BX_SUPPORT_X86_64
  if (BX_CPU_THIS_PTR efer.get_LMA()) {
    if (! BX_CPU_THIS_PTR cr0.get_PE()) {
      BX_PANIC(("change_cpu_mode: EFER.LMA is set when CR0.PE=0 !"));
    }
    if (BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.l) {
      BX_CPU_THIS_PTR cpu_mode = BX_MODE_LONG_64;
    }
    else {
      BX_CPU_THIS_PTR cpu_mode = BX_MODE_LONG_COMPAT;
      // clear upper part of RIP/RSP when leaving 64-bit long mode
      BX_CLEAR_64BIT_HIGH(BX_64BIT_REG_RIP);
      BX_CLEAR_64BIT_HIGH(BX_64BIT_REG_RSP);
    }
  }
  else
#endif
  {
    if (BX_CPU_THIS_PTR cr0.get_PE()) {
      if (BX_CPU_THIS_PTR get_VM()) {
        BX_CPU_THIS_PTR cpu_mode = BX_MODE_IA32_V8086;
      }
      else
        BX_CPU_THIS_PTR cpu_mode = BX_MODE_IA32_PROTECTED;
    }
    else {
      BX_CPU_THIS_PTR cpu_mode = BX_MODE_IA32_REAL;

      // CS segment in real mode always allows full access
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.p        = 1;
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.segment  = 1;  /* data/code segment */
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.type = BX_DATA_READ_WRITE_ACCESSED;

      BX_ASSERT(CPL == 0);
    }
  }

  updateFetchModeMask();

#if BX_CPU_LEVEL >= 6
#if BX_SUPPORT_AVX
  handleAvxModeChange();
#endif
#endif

  if (mode != BX_CPU_THIS_PTR cpu_mode) {
    BX_DEBUG(("%s activated", cpu_mode_string(BX_CPU_THIS_PTR cpu_mode)));
#if BX_DEBUGGER
    if (BX_CPU_THIS_PTR mode_break) {
      BX_CPU_THIS_PTR stop_reason = STOP_MODE_BREAK_POINT;
      bx_debug_break(); // trap into debugger
    }
#endif
  }
}

#if BX_CPU_LEVEL >= 4 && BX_SUPPORT_ALIGNMENT_CHECK
void BX_CPU_C::handleAlignmentCheck(void)
{
  if (CPL == 3 && BX_CPU_THIS_PTR cr0.get_AM() && BX_CPU_THIS_PTR get_AC()) {
    if (BX_CPU_THIS_PTR alignment_check_mask == 0) {
      BX_CPU_THIS_PTR alignment_check_mask = 0xF;
      BX_INFO(("Enable alignment check (#AC exception)"));
    }
  }
  else {
    if (BX_CPU_THIS_PTR alignment_check_mask != 0) {
      BX_CPU_THIS_PTR alignment_check_mask = 0;
      BX_INFO(("Disable alignment check (#AC exception)"));
    }
  }
}
#endif

#if BX_CPU_LEVEL >= 6
void BX_CPU_C::handleSseModeChange(void)
{
  if(BX_CPU_THIS_PTR cr0.get_TS()) {
    BX_CPU_THIS_PTR sse_ok = 0;
  }
  else {
    if(BX_CPU_THIS_PTR cr0.get_EM() || !BX_CPU_THIS_PTR cr4.get_OSFXSR())
      BX_CPU_THIS_PTR sse_ok = 0;
    else
      BX_CPU_THIS_PTR sse_ok = 1;
  }

  updateFetchModeMask(); /* SSE_OK changed */
}  

BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::BxNoSSE(bxInstruction_c *i)
{
  if(BX_CPU_THIS_PTR cr0.get_EM() || !BX_CPU_THIS_PTR cr4.get_OSFXSR())
    exception(BX_UD_EXCEPTION, 0);

  if(BX_CPU_THIS_PTR cr0.get_TS())
    exception(BX_NM_EXCEPTION, 0);

  BX_ASSERT(0);

  BX_NEXT_TRACE(i); // keep compiler happy
}

#if BX_SUPPORT_AVX
void BX_CPU_C::handleAvxModeChange(void)
{
  if(BX_CPU_THIS_PTR cr0.get_TS()) {
    BX_CPU_THIS_PTR avx_ok = 0;
  }
  else {
    if (! protected_mode() || ! BX_CPU_THIS_PTR cr4.get_OSXSAVE() ||
        (~BX_CPU_THIS_PTR xcr0.val32 & 0x6) != 0) BX_CPU_THIS_PTR avx_ok = 0;
    else
      BX_CPU_THIS_PTR avx_ok = 1;
  }

  updateFetchModeMask(); /* AVX_OK changed */
}  

BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::BxNoAVX(bxInstruction_c *i)
{
  if (! protected_mode() || ! BX_CPU_THIS_PTR cr4.get_OSXSAVE())
    exception(BX_UD_EXCEPTION, 0);

  if (~BX_CPU_THIS_PTR xcr0.val32 & 0x6)
    exception(BX_UD_EXCEPTION, 0);

  if(BX_CPU_THIS_PTR cr0.get_TS())
    exception(BX_NM_EXCEPTION, 0);

  BX_ASSERT(0);

  BX_NEXT_TRACE(i); // keep compiler happy
}
#endif

#endif

BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::RDPMC(bxInstruction_c *i)
{
#if BX_CPU_LEVEL >= 5
  if (BX_CPU_THIS_PTR cr4.get_PCE() || CPL==0 || real_mode()) {

#if BX_SUPPORT_VMX
    if (BX_CPU_THIS_PTR in_vmx_guest)
      VMexit_RDPMC(i);
#endif

    /* According to manual, Pentium 4 has 18 counters,
     * previous versions have two.  And the P4 also can do
     * short read-out (EDX always 0).  Otherwise it is
     * limited to 40 bits.
     */

    if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_SSE2)) { // Pentium 4 processor (see cpuid.cc)
      if ((ECX & 0x7fffffff) >= 18)
        exception(BX_GP_EXCEPTION, 0);
    }
    else {
      if ((ECX & 0xffffffff) >= 2)
        exception(BX_GP_EXCEPTION, 0);
    }

    // Most counters are for hardware specific details, which
    // we anyhow do not emulate (like pipeline stalls etc)

    // Could be interesting to count number of memory reads,
    // writes.  Misaligned etc...  But to monitor bochs, this
    // is easier done from the host.

    RAX = 0;
    RDX = 0; // if P4 and ECX & 0x10000000, then always 0 (short read 32 bits)

    BX_ERROR(("RDPMC: Performance Counters Support not reasonably implemented yet"));
  } else {
    // not allowed to use RDPMC!
    exception(BX_GP_EXCEPTION, 0);
  }
#endif

  BX_NEXT_INSTR(i);
}

#if BX_CPU_LEVEL >= 5
Bit64u BX_CPU_C::get_TSC(void)
{
  Bit64u tsc = bx_pc_system.time_ticks() - BX_CPU_THIS_PTR msr.tsc_last_reset;
#if BX_SUPPORT_VMX
  if (BX_CPU_THIS_PTR in_vmx_guest)
    tsc += VMX_TSC_Offset();
#endif
  return tsc;
}

void BX_CPU_C::set_TSC(Bit64u newval)
{
  // compute the correct setting of tsc_last_reset so that a get_TSC()
  // will return newval
  BX_CPU_THIS_PTR msr.tsc_last_reset = bx_pc_system.time_ticks() - newval;

  // verify
  BX_ASSERT(get_TSC() == newval);
}
#endif

BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::RDTSC(bxInstruction_c *i)
{
#if BX_CPU_LEVEL >= 5
  if (! BX_CPU_THIS_PTR cr4.get_TSD() || CPL==0) {

#if BX_SUPPORT_VMX
    if (BX_CPU_THIS_PTR in_vmx_guest)
      VMexit_RDTSC(i);
#endif

    // return ticks
    Bit64u ticks = BX_CPU_THIS_PTR get_TSC();

    RAX = GET32L(ticks);
    RDX = GET32H(ticks);

    BX_DEBUG(("RDTSC: ticks 0x%08x:%08x", EDX, EAX));

  } else {
    BX_ERROR(("RDTSC: not allowed to use instruction !"));
    exception(BX_GP_EXCEPTION, 0);
  }
#endif

  BX_NEXT_INSTR(i);
}

BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::RDTSCP(bxInstruction_c *i)
{
#if BX_SUPPORT_X86_64

#if BX_SUPPORT_VMX
  // RDTSCP will always #UD in legacy VMX mode
  if (BX_CPU_THIS_PTR in_vmx_guest) {
    if (! SECONDARY_VMEXEC_CONTROL(VMX_VM_EXEC_CTRL3_RDTSCP)) {
       BX_ERROR(("RDTSCP in VMX guest: not allowed to use instruction !"));
       exception(BX_UD_EXCEPTION, 0);
    }
  }
#endif

  if (! BX_CPU_THIS_PTR cr4.get_TSD() || CPL==0) {

#if BX_SUPPORT_VMX
    if (BX_CPU_THIS_PTR in_vmx_guest)
      VMexit_RDTSC(i);
#endif

    // return ticks
    Bit64u ticks = BX_CPU_THIS_PTR get_TSC();

    RAX = GET32L(ticks);
    RDX = GET32H(ticks);
    RCX = MSR_TSC_AUX;

  } else {
    BX_ERROR(("RDTSCP: not allowed to use instruction !"));
    exception(BX_GP_EXCEPTION, 0);
  }
#endif

  BX_NEXT_INSTR(i);
}

#if BX_SUPPORT_MONITOR_MWAIT
bx_bool BX_CPU_C::is_monitor(bx_phy_address begin_addr, unsigned len)
{
  if (! BX_CPU_THIS_PTR monitor.armed) return 0;

  bx_phy_address monitor_begin = BX_CPU_THIS_PTR monitor.monitor_addr;
  bx_phy_address monitor_end = monitor_begin + CACHE_LINE_SIZE - 1;

  bx_phy_address end_addr = begin_addr + len;
  if (begin_addr >= monitor_end || end_addr <= monitor_begin)
    return 0;
  else
    return 1;
}

void BX_CPU_C::check_monitor(bx_phy_address begin_addr, unsigned len)
{
  if (is_monitor(begin_addr, len)) {
    // wakeup from MWAIT state
    if(BX_CPU_THIS_PTR activity_state >= BX_ACTIVITY_STATE_MWAIT)
       BX_CPU_THIS_PTR activity_state = BX_ACTIVITY_STATE_ACTIVE;
    // clear monitor
    BX_CPU_THIS_PTR monitor.reset_monitor();
  }
}
#endif

BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::MONITOR(bxInstruction_c *i)
{
#if BX_SUPPORT_MONITOR_MWAIT
  // CPL is always 0 in real mode
  if (/* !real_mode() && */ CPL != 0) {
    BX_DEBUG(("MWAIT instruction not recognized when CPL != 0"));
    exception(BX_UD_EXCEPTION, 0);
  }

  BX_DEBUG(("MONITOR instruction executed EAX = 0x%08x", EAX));

#if BX_SUPPORT_VMX
  if (BX_CPU_THIS_PTR in_vmx_guest)
    VMexit_MONITOR(i);
#endif

  if (RCX != 0) {
    BX_ERROR(("MONITOR: no optional extensions supported"));
    exception(BX_GP_EXCEPTION, 0);
  }

  bx_segment_reg_t *seg = &BX_CPU_THIS_PTR sregs[i->seg()];

  bx_address offset = RAX & i->asize_mask();

  // set MONITOR
  bx_address laddr = BX_CPU_THIS_PTR get_laddr(i->seg(), offset);

#if BX_SUPPORT_X86_64
  if (BX_CPU_THIS_PTR cpu_mode == BX_MODE_LONG_64) {
    if (! IsCanonical(laddr)) {
      BX_ERROR(("MONITOR: non-canonical access !"));
      exception(int_number(i->seg()), 0);
    }
  }
  else
#endif
  {
    // check if we could access the memory segment
    if (!(seg->cache.valid & SegAccessROK)) {
      if (! execute_virtual_checks(seg, (Bit32u) offset, 1))
        exception(int_number(i->seg()), 0);
    }
    else {
      if (offset > seg->cache.u.segment.limit_scaled) {
        BX_ERROR(("MONITOR: segment limit violation"));
        exception(int_number(i->seg()), 0);
      }
    }
  }

  bx_phy_address paddr = translate_linear(laddr, USER_PL, BX_READ);

  // Set the monitor immediately. If monitor is still armed when we MWAIT,
  // the processor will stall.

  bx_pc_system.invlpg(paddr);

  BX_CPU_THIS_PTR monitor.arm(paddr);

  BX_DEBUG(("MONITOR for phys_addr=0x" FMT_PHY_ADDRX, BX_CPU_THIS_PTR monitor.monitor_addr));
#endif

  BX_NEXT_INSTR(i);
}

BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::MWAIT(bxInstruction_c *i)
{
#if BX_SUPPORT_MONITOR_MWAIT
  // CPL is always 0 in real mode
  if (/* !real_mode() && */ CPL != 0) {
    BX_DEBUG(("MWAIT instruction not recognized when CPL != 0"));
    exception(BX_UD_EXCEPTION, 0);
  }

  BX_DEBUG(("MWAIT instruction executed ECX = 0x%08x", ECX));

#if BX_SUPPORT_VMX
  if (BX_CPU_THIS_PTR in_vmx_guest)
    VMexit_MWAIT(i);
#endif

  // only one extension is supported
  //   ECX[0] - interrupt MWAIT even if EFLAGS.IF = 0
  if (RCX & ~(BX_CONST64(1))) {
    BX_ERROR(("MWAIT: incorrect optional extensions in RCX"));
    exception(BX_GP_EXCEPTION, 0);
  }

  // If monitor has already triggered, we just return.
  if (! BX_CPU_THIS_PTR monitor.armed) {
    BX_DEBUG(("MWAIT: the MONITOR was not armed or already triggered"));
    BX_NEXT_TRACE(i);
  }

  static bx_bool mwait_is_nop = SIM->get_param_bool(BXPN_CPUID_MWAIT_IS_NOP)->get();
  if (mwait_is_nop) {
    BX_NEXT_TRACE(i);
  }

  // stops instruction execution and places the processor in a optimized
  // state.  Events that cause exit from MWAIT state are:
  // A store from another processor to monitored range, any unmasked
  // interrupt, including INTR, NMI, SMI, INIT or reset will resume
  // the execution. Any far control transfer between MONITOR and MWAIT
  // resets the monitoring logic.

  if (ECX & 1) {
#if BX_SUPPORT_VMX
    // When "interrupt window exiting" VMX control is set MWAIT instruction
    // won't cause the processor to enter BX_ACTIVITY_STATE_MWAIT_IF sleep
    // state with EFLAGS.IF = 0
    if (BX_CPU_THIS_PTR vmx_interrupt_window && ! BX_CPU_THIS_PTR get_IF())
    {
      BX_NEXT_TRACE(i);
    }
#endif
    BX_CPU_THIS_PTR activity_state = BX_ACTIVITY_STATE_MWAIT_IF;
  }
  else {
    BX_CPU_THIS_PTR activity_state = BX_ACTIVITY_STATE_MWAIT;
  }

  BX_CPU_THIS_PTR async_event = 1; // so processor knows to check
  // Execution of this instruction completes.  The processor
  // will remain in a optimized state until one of the above
  // conditions is met.

  BX_INSTR_MWAIT(BX_CPU_ID, BX_CPU_THIS_PTR monitor.monitor_addr, CACHE_LINE_SIZE, ECX);

#if BX_USE_IDLE_HACK
  bx_gui->sim_is_idle();
#endif

#if BX_DEBUGGER
  bx_dbg_halt(BX_CPU_ID);
#endif
#endif

  BX_NEXT_TRACE(i);
}

BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::SYSENTER(bxInstruction_c *i)
{
#if BX_CPU_LEVEL >= 6
  if (real_mode()) {
    BX_ERROR(("SYSENTER not recognized in real mode !"));
    exception(BX_GP_EXCEPTION, 0);
  }
  if ((BX_CPU_THIS_PTR msr.sysenter_cs_msr & BX_SELECTOR_RPL_MASK) == 0) {
    BX_ERROR(("SYSENTER with zero sysenter_cs_msr !"));
    exception(BX_GP_EXCEPTION, 0);
  }

  invalidate_prefetch_q();

  BX_CPU_THIS_PTR clear_VM();       // do this just like the book says to do
  BX_CPU_THIS_PTR clear_IF();
  BX_CPU_THIS_PTR clear_RF();

#if BX_SUPPORT_X86_64
  if (long_mode()) {
    if (!IsCanonical(BX_CPU_THIS_PTR msr.sysenter_eip_msr)) {
      BX_ERROR(("SYSENTER with non-canonical SYSENTER_EIP_MSR !"));
      exception(BX_GP_EXCEPTION, 0);
    }
    if (!IsCanonical(BX_CPU_THIS_PTR msr.sysenter_esp_msr)) {
      BX_ERROR(("SYSENTER with non-canonical SYSENTER_ESP_MSR !"));
      exception(BX_GP_EXCEPTION, 0);
    }
  }
#endif

  parse_selector(BX_CPU_THIS_PTR msr.sysenter_cs_msr & BX_SELECTOR_RPL_MASK,
                       &BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].selector);

  BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.valid   = SegValidCache | SegAccessROK | SegAccessWOK;
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.p       = 1;
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.dpl     = 0;
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.segment = 1;  /* data/code segment */
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.type    = BX_CODE_EXEC_READ_ACCESSED;
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.base         = 0;          // base address
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.limit_scaled = 0xFFFFFFFF; // scaled segment limit
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.g            = 1;          // 4k granularity
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.avl          = 0;          // available for use by system
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.d_b          = !long_mode();
#if BX_SUPPORT_X86_64
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.l            =  long_mode();
#endif

#if BX_SUPPORT_X86_64
  handleCpuModeChange(); // mode change could happen only when in long_mode()
#else
  updateFetchModeMask(/* CS reloaded */);
#endif

#if BX_SUPPORT_ALIGNMENT_CHECK
  BX_CPU_THIS_PTR alignment_check_mask = 0; // CPL=0
#endif

  parse_selector((BX_CPU_THIS_PTR msr.sysenter_cs_msr + 8) & BX_SELECTOR_RPL_MASK,
                       &BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].selector);

  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.valid    = SegValidCache | SegAccessROK | SegAccessWOK;
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.p        = 1;
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.dpl      = 0;
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.segment  = 1; /* data/code segment */
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.type     = BX_DATA_READ_WRITE_ACCESSED;
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.base         = 0;          // base address
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.limit_scaled = 0xFFFFFFFF; // scaled segment limit
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.g            = 1;          // 4k granularity
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.d_b          = 1;          // 32-bit mode
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.avl          = 0;          // available for use by system
#if BX_SUPPORT_X86_64
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.l            = 0;
#endif

#if BX_SUPPORT_X86_64
  if (long_mode()) {
    RSP = BX_CPU_THIS_PTR msr.sysenter_esp_msr;
    RIP = BX_CPU_THIS_PTR msr.sysenter_eip_msr;
  }
  else
#endif
  {
    ESP = (Bit32u) BX_CPU_THIS_PTR msr.sysenter_esp_msr;
    EIP = (Bit32u) BX_CPU_THIS_PTR msr.sysenter_eip_msr;
  }

  BX_INSTR_FAR_BRANCH(BX_CPU_ID, BX_INSTR_IS_SYSENTER,
                      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].selector.value, RIP);
#endif

  BX_NEXT_TRACE(i);
}

BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::SYSEXIT(bxInstruction_c *i)
{
#if BX_CPU_LEVEL >= 6
  if (real_mode() || CPL != 0) {
    BX_ERROR(("SYSEXIT from real mode or with CPL<>0 !"));
    exception(BX_GP_EXCEPTION, 0);
  }
  if ((BX_CPU_THIS_PTR msr.sysenter_cs_msr & BX_SELECTOR_RPL_MASK) == 0) {
    BX_ERROR(("SYSEXIT with zero sysenter_cs_msr !"));
    exception(BX_GP_EXCEPTION, 0);
  }

  invalidate_prefetch_q();

#if BX_SUPPORT_X86_64
  if (i->os64L()) {
    if (!IsCanonical(RDX)) {
       BX_ERROR(("SYSEXIT with non-canonical RDX (RIP) pointer !"));
       exception(BX_GP_EXCEPTION, 0);
    }
    if (!IsCanonical(RCX)) {
       BX_ERROR(("SYSEXIT with non-canonical RCX (RSP) pointer !"));
       exception(BX_GP_EXCEPTION, 0);
    }

    parse_selector(((BX_CPU_THIS_PTR msr.sysenter_cs_msr + 32) & BX_SELECTOR_RPL_MASK) | 3,
            &BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].selector);

    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.valid   = SegValidCache | SegAccessROK | SegAccessWOK;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.p       = 1;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.dpl     = 3;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.segment = 1;  /* data/code segment */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.type    = BX_CODE_EXEC_READ_ACCESSED;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.base         = 0;           // base address
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.limit_scaled = 0xFFFFFFFF;  // scaled segment limit
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.g            = 1;           // 4k granularity
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.avl          = 0;           // available for use by system
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.d_b          = 0;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.l            = 1;

    RSP = RCX;
    RIP = RDX;
  }
  else
#endif
  {
    parse_selector(((BX_CPU_THIS_PTR msr.sysenter_cs_msr + 16) & BX_SELECTOR_RPL_MASK) | 3,
            &BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].selector);

    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.valid   = SegValidCache | SegAccessROK | SegAccessWOK;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.p       = 1;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.dpl     = 3;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.segment = 1;  /* data/code segment */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.type    = BX_CODE_EXEC_READ_ACCESSED;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.base         = 0;           // base address
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.limit_scaled = 0xFFFFFFFF;  // scaled segment limit
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.g            = 1;           // 4k granularity
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.avl          = 0;           // available for use by system
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.d_b          = 1;
#if BX_SUPPORT_X86_64
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.l            = 0;
#endif

    ESP = ECX;
    EIP = EDX;
  }

#if BX_SUPPORT_X86_64
  handleCpuModeChange(); // mode change could happen only when in long_mode()
#else
  updateFetchModeMask(/* CS reloaded */);
#endif

#if BX_SUPPORT_ALIGNMENT_CHECK
  handleAlignmentCheck(/* CPL change */);
#endif

  parse_selector(((BX_CPU_THIS_PTR msr.sysenter_cs_msr + (i->os64L() ? 40:24)) & BX_SELECTOR_RPL_MASK) | 3,
            &BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].selector);

  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.valid    = SegValidCache | SegAccessROK | SegAccessWOK;
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.p        = 1;
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.dpl      = 3;
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.segment  = 1; /* data/code segment */
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.type     = BX_DATA_READ_WRITE_ACCESSED;
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.base         = 0;           // base address
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.limit_scaled = 0xFFFFFFFF;  // scaled segment limit
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.g            = 1;           // 4k granularity
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.d_b          = 1;           // 32-bit mode
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.avl          = 0;           // available for use by system
#if BX_SUPPORT_X86_64
  BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.l            = 0;
#endif

  BX_INSTR_FAR_BRANCH(BX_CPU_ID, BX_INSTR_IS_SYSEXIT,
                      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].selector.value, RIP);
#endif

  BX_NEXT_TRACE(i);
}

BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::SYSCALL(bxInstruction_c *i)
{
#if BX_CPU_LEVEL >= 5
  bx_address temp_RIP;

  BX_DEBUG(("Execute SYSCALL instruction"));

  if (!BX_CPU_THIS_PTR efer.get_SCE()) {
    exception(BX_UD_EXCEPTION, 0);
  }

  invalidate_prefetch_q();

#if BX_SUPPORT_X86_64
  if (long_mode())
  {
    RCX = RIP;
    R11 = read_eflags() & ~(EFlagsRFMask);

    if (BX_CPU_THIS_PTR cpu_mode == BX_MODE_LONG_64) {
      temp_RIP = MSR_LSTAR;
    }
    else {
      temp_RIP = MSR_CSTAR;
    }

    // set up CS segment, flat, 64-bit DPL=0
    parse_selector((MSR_STAR >> 32) & BX_SELECTOR_RPL_MASK,
                       &BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].selector);

    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.valid   = SegValidCache | SegAccessROK | SegAccessWOK;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.p       = 1;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.dpl     = 0;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.segment = 1;  /* data/code segment */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.type    = BX_CODE_EXEC_READ_ACCESSED;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.base         = 0; /* base address */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.limit_scaled = 0xFFFFFFFF;  /* scaled segment limit */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.g            = 1; /* 4k granularity */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.d_b          = 0;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.l            = 1; /* 64-bit code */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.avl          = 0; /* available for use by system */

    handleCpuModeChange(); // mode change could only happen when in long_mode()

#if BX_SUPPORT_ALIGNMENT_CHECK
    BX_CPU_THIS_PTR alignment_check_mask = 0; // CPL=0
#endif

    // set up SS segment, flat, 64-bit DPL=0
    parse_selector(((MSR_STAR >> 32) + 8) & BX_SELECTOR_RPL_MASK,
                       &BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].selector);

    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.valid   = SegValidCache | SegAccessROK | SegAccessWOK;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.p       = 1;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.dpl     = 0;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.segment = 1; /* data/code segment */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.type    = BX_DATA_READ_WRITE_ACCESSED;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.base         = 0; /* base address */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.limit_scaled = 0xFFFFFFFF;  /* scaled segment limit */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.g            = 1; /* 4k granularity */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.d_b          = 1; /* 32 bit stack */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.l            = 0;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.avl          = 0; /* available for use by system */

    writeEFlags(read_eflags() & (~MSR_FMASK), EFlagsValidMask);
    BX_CPU_THIS_PTR clear_RF();
    RIP = temp_RIP;
  }
  else
#endif
  {
    // legacy mode

    ECX = EIP;
    temp_RIP = MSR_STAR & 0xFFFFFFFF;

    // set up CS segment, flat, 32-bit DPL=0
    parse_selector((MSR_STAR >> 32) & BX_SELECTOR_RPL_MASK,
                       &BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].selector);

    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.valid   = SegValidCache | SegAccessROK | SegAccessWOK;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.p       = 1;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.dpl     = 0;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.segment = 1;  /* data/code segment */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.type    = BX_CODE_EXEC_READ_ACCESSED;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.base         = 0; /* base address */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.limit_scaled = 0xFFFFFFFF;  /* scaled segment limit */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.g            = 1; /* 4k granularity */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.d_b          = 1;
#if BX_SUPPORT_X86_64
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.l            = 0; /* 32-bit code */
#endif
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.avl          = 0; /* available for use by system */

    updateFetchModeMask(/* CS reloaded */);

#if BX_SUPPORT_ALIGNMENT_CHECK
    BX_CPU_THIS_PTR alignment_check_mask = 0; // CPL=0
#endif

    // set up SS segment, flat, 32-bit DPL=0
    parse_selector(((MSR_STAR >> 32) + 8) & BX_SELECTOR_RPL_MASK,
                       &BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].selector);

    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.valid   = SegValidCache | SegAccessROK | SegAccessWOK;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.p       = 1;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.dpl     = 0;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.segment = 1; /* data/code segment */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.type    = BX_DATA_READ_WRITE_ACCESSED;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.base         = 0; /* base address */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.limit_scaled = 0xFFFFFFFF;  /* scaled segment limit */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.g            = 1; /* 4k granularity */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.d_b          = 1; /* 32 bit stack */
#if BX_SUPPORT_X86_64
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.l            = 0;
#endif
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.u.segment.avl          = 0; /* available for use by system */

    BX_CPU_THIS_PTR clear_VM();
    BX_CPU_THIS_PTR clear_IF();
    BX_CPU_THIS_PTR clear_RF();
    RIP = temp_RIP;
  }

  BX_INSTR_FAR_BRANCH(BX_CPU_ID, BX_INSTR_IS_SYSCALL,
                      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].selector.value, RIP);
#endif

  BX_NEXT_TRACE(i);
}

BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::SYSRET(bxInstruction_c *i)
{
#if BX_CPU_LEVEL >= 5
  bx_address temp_RIP;

  BX_DEBUG(("Execute SYSRET instruction"));

  if (!BX_CPU_THIS_PTR efer.get_SCE()) {
    exception(BX_UD_EXCEPTION, 0);
  }

  if(!protected_mode() || CPL != 0) {
    BX_ERROR(("SYSRET: priveledge check failed, generate #GP(0)"));
    exception(BX_GP_EXCEPTION, 0);
  }

  invalidate_prefetch_q();

#if BX_SUPPORT_X86_64
  if (BX_CPU_THIS_PTR cpu_mode == BX_MODE_LONG_64)
  {
    if (i->os64L()) {
      if (!IsCanonical(RCX)) {
        BX_ERROR(("SYSRET: canonical failure for RCX (RIP)"));
        exception(BX_GP_EXCEPTION, 0);
      }

      // Return to 64-bit mode, set up CS segment, flat, 64-bit DPL=3
      parse_selector((((MSR_STAR >> 48) + 16) & BX_SELECTOR_RPL_MASK) | 3,
                       &BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].selector);

      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.valid   = SegValidCache | SegAccessROK | SegAccessWOK;
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.p       = 1;
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.dpl     = 3;
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.segment = 1;  /* data/code segment */
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.type    = BX_CODE_EXEC_READ_ACCESSED;
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.base         = 0; /* base address */
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.limit_scaled = 0xFFFFFFFF;  /* scaled segment limit */
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.g            = 1; /* 4k granularity */
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.d_b          = 0;
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.l            = 1; /* 64-bit code */
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.avl          = 0; /* available for use by system */

      temp_RIP = RCX;
    }
    else {
      // Return to 32-bit compatibility mode, set up CS segment, flat, 32-bit DPL=3
      parse_selector((MSR_STAR >> 48) | 3,
                       &BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].selector);

      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.valid   = SegValidCache | SegAccessROK | SegAccessWOK;
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.p       = 1;
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.dpl     = 3;
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.segment = 1;  /* data/code segment */
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.type    = BX_CODE_EXEC_READ_ACCESSED;
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.base         = 0; /* base address */
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.limit_scaled = 0xFFFFFFFF;  /* scaled segment limit */
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.g            = 1; /* 4k granularity */
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.d_b          = 1;
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.l            = 0; /* 32-bit code */
      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.avl          = 0; /* available for use by system */

      temp_RIP = ECX;
    }

    handleCpuModeChange(); // mode change could only happen when in long64 mode

#if BX_SUPPORT_ALIGNMENT_CHECK
    handleAlignmentCheck(/* CPL change */);
#endif

    // SS base, limit, attributes unchanged
    parse_selector((Bit16u)(((MSR_STAR >> 48) + 8) | 3),
                       &BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].selector);

    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.valid   = SegValidCache | SegAccessROK | SegAccessWOK;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.p       = 1;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.dpl     = 3;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.segment = 1;  /* data/code segment */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.type    = BX_DATA_READ_WRITE_ACCESSED;

    writeEFlags((Bit32u) R11, EFlagsValidMask);
  }
  else // (!64BIT_MODE)
#endif
  {
    // Return to 32-bit legacy mode, set up CS segment, flat, 32-bit DPL=3
    parse_selector((MSR_STAR >> 48) | 3,
                     &BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].selector);

    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.valid   = SegValidCache | SegAccessROK | SegAccessWOK;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.p       = 1;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.dpl     = 3;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.segment = 1;  /* data/code segment */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.type    = BX_CODE_EXEC_READ_ACCESSED;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.base         = 0; /* base address */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.limit_scaled = 0xFFFFFFFF;  /* scaled segment limit */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.g            = 1; /* 4k granularity */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.d_b          = 1;
#if BX_SUPPORT_X86_64
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.l            = 0; /* 32-bit code */
#endif
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].cache.u.segment.avl          = 0; /* available for use by system */

    updateFetchModeMask(/* CS reloaded */);

#if BX_SUPPORT_ALIGNMENT_CHECK
    handleAlignmentCheck(/* CPL change */);
#endif

    // SS base, limit, attributes unchanged
    parse_selector((Bit16u)(((MSR_STAR >> 48) + 8) | 3),
                     &BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].selector);

    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.valid   = SegValidCache | SegAccessROK | SegAccessWOK;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.p       = 1;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.dpl     = 3;
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.segment = 1;  /* data/code segment */
    BX_CPU_THIS_PTR sregs[BX_SEG_REG_SS].cache.type    = BX_DATA_READ_WRITE_ACCESSED;

    BX_CPU_THIS_PTR assert_IF();
    temp_RIP = ECX;
  }

  handleCpuModeChange();

  RIP = temp_RIP;

  BX_INSTR_FAR_BRANCH(BX_CPU_ID, BX_INSTR_IS_SYSRET,
                      BX_CPU_THIS_PTR sregs[BX_SEG_REG_CS].selector.value, RIP);
#endif

  BX_NEXT_TRACE(i);
}

#if BX_SUPPORT_X86_64
BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::SWAPGS(bxInstruction_c *i)
{
  if(CPL != 0)
    exception(BX_GP_EXCEPTION, 0);

  Bit64u temp_GS_base = MSR_GSBASE;
  MSR_GSBASE = MSR_KERNELGSBASE;
  MSR_KERNELGSBASE = temp_GS_base;

  BX_NEXT_INSTR(i);
}

/* F3 0F AE /0 */
BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::RDFSBASE(bxInstruction_c *i)
{
  if (! BX_CPU_THIS_PTR cr4.get_FSGSBASE())
    exception(BX_UD_EXCEPTION, 0);

  if (i->os64L()) {
    BX_WRITE_64BIT_REG(i->rm(), MSR_FSBASE);
  }
  else {
    BX_WRITE_32BIT_REGZ(i->rm(), (Bit32u) MSR_FSBASE);
  }

  BX_NEXT_INSTR(i);
}

/* F3 0F AE /1 */
BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::RDGSBASE(bxInstruction_c *i)
{
  if (! BX_CPU_THIS_PTR cr4.get_FSGSBASE())
    exception(BX_UD_EXCEPTION, 0);

  if (i->os64L()) {
    BX_WRITE_64BIT_REG(i->rm(), MSR_GSBASE);
  }
  else {
    BX_WRITE_32BIT_REGZ(i->rm(), (Bit32u) MSR_GSBASE);
  }

  BX_NEXT_INSTR(i);
}

/* F3 0F AE /2 */
BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::WRFSBASE(bxInstruction_c *i)
{
  if (! BX_CPU_THIS_PTR cr4.get_FSGSBASE())
    exception(BX_UD_EXCEPTION, 0);

  if (i->os64L()) {
    Bit64u fsbase = BX_READ_64BIT_REG(i->rm());
    if (!IsCanonical(fsbase)) {
      BX_ERROR(("WRFSBASE: canonical failure !"));
      exception(BX_GP_EXCEPTION, 0);
    }
    MSR_FSBASE = fsbase;
  }
  else {
    // 32-bit value is always canonical
    MSR_FSBASE = BX_READ_32BIT_REG(i->rm());
  }

  BX_NEXT_INSTR(i);
}

/* F3 0F AE /3 */
BX_INSF_TYPE BX_CPP_AttrRegparmN(1) BX_CPU_C::WRGSBASE(bxInstruction_c *i)
{
  if (! BX_CPU_THIS_PTR cr4.get_FSGSBASE())
    exception(BX_UD_EXCEPTION, 0);

  if (i->os64L()) {
    Bit64u gsbase = BX_READ_64BIT_REG(i->rm());
    if (!IsCanonical(gsbase)) {
      BX_ERROR(("WRGSBASE: canonical failure !"));
      exception(BX_GP_EXCEPTION, 0);
    }
    MSR_GSBASE = gsbase;
  }
  else {
    // 32-bit value is always canonical
    MSR_GSBASE = BX_READ_32BIT_REG(i->rm());
  }

  BX_NEXT_INSTR(i);
}
#endif
