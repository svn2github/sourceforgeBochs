/////////////////////////////////////////////////////////////////////////
// $Id: apic.cc,v 1.128 2009-02-22 10:44:50 vruppert Exp $
/////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2002 Zwane Mwaikambo, Stanislav Shwartsman
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
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//
/////////////////////////////////////////////////////////////////////////

#define NEED_CPU_REG_SHORTCUTS 1
#include "bochs.h"
#include "cpu.h"
#include "iodev/iodev.h"

#if BX_SUPPORT_APIC

#define LOG_THIS this->

#define BX_CPU_APIC(i) (&(BX_CPU(i)->lapic))

#define BX_LAPIC_FIRST_VECTOR	0x10
#define BX_LAPIC_LAST_VECTOR	0xfe

///////////// APIC BUS /////////////

int apic_bus_deliver_interrupt(Bit8u vector, Bit8u dest, Bit8u delivery_mode, bx_bool logical_dest, bx_bool level, bx_bool trig_mode)
{
  if(delivery_mode == APIC_DM_LOWPRI)
  {
     if(! logical_dest) {
       // I/O subsytem initiated interrupt with lowest priority delivery
       // which is not supported in physical destination mode
       return 0;
     }
     else {
       return apic_bus_deliver_lowest_priority(vector, dest, trig_mode, 0);
     }
  }

  // determine destination local apics and deliver
  if(! logical_dest) {
    // physical destination mode
    if((dest & APIC_ID_MASK) == APIC_MAX_ID) {
       return apic_bus_broadcast_interrupt(vector, delivery_mode, trig_mode, APIC_MAX_ID);
    }
    else {
       // the destination is single agent
       for (unsigned i=0;i<BX_NUM_LOCAL_APICS;i++)
       {
         if(BX_CPU_APIC(i)->get_id() == dest) {
           BX_CPU_APIC(i)->deliver(vector, delivery_mode, trig_mode);
           return 1;
         }
       }

       return 0;
    }
  }
  else {
    // logical destination mode
    if(dest == 0) return 0;

    bx_bool interrupt_delivered = 0;

    for (int i=0; i<BX_NUM_LOCAL_APICS; i++) {
      if(BX_CPU_APIC(i)->match_logical_addr(dest)) {
        BX_CPU_APIC(i)->deliver(vector, delivery_mode, trig_mode);
        interrupt_delivered = 1;
      }
    }

    return interrupt_delivered;
  }
}

int apic_bus_deliver_lowest_priority(Bit8u vector, Bit8u dest, bx_bool trig_mode, bx_bool broadcast)
{
  int i;

#ifndef BX_IMPLEMENT_XAPIC
  // search for if focus processor exists
  for (i=0; i<BX_NUM_LOCAL_APICS; i++) {
    if(BX_CPU_APIC(i)->is_focus(vector)) {
      BX_CPU_APIC(i)->deliver(vector, APIC_DM_LOWPRI, trig_mode);
      return 1;
    }
  }
#endif

  // focus processor not found, looking for lowest priority agent
  int lowest_priority_agent = -1, lowest_priority = 0x100;

  for (i=0; i<BX_NUM_LOCAL_APICS; i++) {
    if(broadcast || BX_CPU_APIC(i)->match_logical_addr(dest)) {
#ifndef BX_IMPLEMENT_XAPIC
      int priority = BX_CPU_APIC(i)->get_apr();
#else
      int priority = BX_CPU_APIC(i)->get_tpr();
#endif
      if(priority < lowest_priority) {
        lowest_priority = priority;
        lowest_priority_agent = i;
      }
    }
  }

  if(lowest_priority_agent >= 0)
  {
    BX_CPU_APIC(lowest_priority_agent)->deliver(vector, APIC_DM_LOWPRI, trig_mode);
    return 1;
  }

  return 0;
}

int apic_bus_broadcast_interrupt(Bit8u vector, Bit8u delivery_mode, bx_bool trig_mode, int exclude_cpu)
{
  if(delivery_mode == APIC_DM_LOWPRI)
  {
    return apic_bus_deliver_lowest_priority(vector, 0 /* doesn't matter */, trig_mode, 1);
  }

  // deliver to all bus agents except 'exclude_cpu'
  for (int i=0; i<BX_NUM_LOCAL_APICS; i++) {
    if(i == exclude_cpu) continue;
    BX_CPU_APIC(i)->deliver(vector, delivery_mode, trig_mode);
  }

  return 1;
}

static void apic_bus_broadcast_eoi(Bit8u vector)
{
  DEV_ioapic_receive_eoi(vector);
}

#endif

// available even if APIC is not compiled in
void apic_bus_deliver_smi(void)
{
  BX_CPU(0)->deliver_SMI();
}

void apic_bus_broadcast_smi(void)
{
  for (unsigned i=0; i<BX_SMP_PROCESSORS; i++)
    BX_CPU(i)->deliver_SMI();
}

#if BX_SUPPORT_APIC

////////////////////////////////////

bx_local_apic_c::bx_local_apic_c(BX_CPU_C *mycpu, unsigned id)
  : base_addr(BX_LAPIC_BASE_ADDR), cpu(mycpu)
{
  apic_id = id;
  BX_ASSERT(apic_id < APIC_MAX_ID);

  char buffer[16];
  sprintf(buffer, "APIC%x", apic_id);
  put(buffer);

  // Register a non-active timer for use when the timer is started.
  timer_handle = bx_pc_system.register_timer_ticks(this,
            BX_CPU(0)->lapic.periodic_smf, 0, 0, 0, "lapic");
  timer_active = 0;

  reset(BX_RESET_HARDWARE);
}

void bx_local_apic_c::reset(unsigned type)
{
  int i;

  BX_INFO(("local apic %d initializing", apic_id));

  // default address for a local APIC, can be moved
  base_addr = BX_LAPIC_BASE_ADDR;
  error_status = shadow_error_status = 0;
  ldr = 0;
  dest_format = 0xf;
  icr_hi = 0;
  icr_lo = 0;
  task_priority = 0;

  for(i=0; i<BX_LAPIC_MAX_INTS; i++) {
    irr[i] = isr[i] = tmr[i] = 0;
  }

  timer_divconf = 0;
  timer_divide_factor = 1;
  timer_initial = 0;
  timer_current = 0;

  if(timer_active) {
    bx_pc_system.deactivate_timer(timer_handle);
    timer_active = 0;
  }

  for(i=0; i<APIC_LVT_ENTRIES; i++) {
    lvt[i] = 0x10000;	// all LVT are masked
  }

  // split spurious vector register to 3 fields
  spurious_vector = 0xff;
  software_enabled = 0;
  focus_disable = 0;

  mode = BX_APIC_XAPIC_MODE;

  INTR = 0;
}

void bx_local_apic_c::set_base(bx_phy_address newbase)
{
  mode = (newbase >> 10) & 3;
  newbase &= ~((bx_phy_address) 0xfff);
  base_addr = newbase;
  BX_INFO(("allocate APIC id=%d (MMIO %s) to 0x" FMT_PHY_ADDRX,
    apic_id, (mode == BX_APIC_XAPIC_MODE) ? "enabled" : "disabled", newbase));
}

bx_bool bx_local_apic_c::is_selected(bx_phy_address addr)
{
  if (mode != BX_APIC_XAPIC_MODE) return 0;

  if((addr & ~0xfff) == base_addr) {
    if((addr & 0xf) != 0)
      BX_INFO(("warning: misaligned APIC access. addr=0x" FMT_PHY_ADDRX, addr));
    return 1;
  }
  return 0;
}

void bx_local_apic_c::read(bx_phy_address addr, void *data, unsigned len)
{
  if((addr & ~0x3) != ((addr+len-1) & ~0x3)) {
    BX_PANIC(("APIC read at address 0x" FMT_PHY_ADDRX " spans 32-bit boundary !", addr));
    return;
  }
  Bit32u value = read_aligned(addr & ~0x3);
  if(len == 4) { // must be 32-bit aligned
    *((Bit32u *)data) = value;
    return;
  }
  // handle partial read, independent of endian-ness
  value >>= (addr&3)*8;
  if (len == 1)
    *((Bit8u *) data) = value & 0xff;
  else if (len == 2)
    *((Bit16u *)data) = value & 0xffff;
  else
    BX_PANIC(("Unsupported APIC read at address 0x" FMT_PHY_ADDRX ", len=%d", addr, len));
}

void bx_local_apic_c::write(bx_phy_address addr, void *data, unsigned len)
{
  if (len != 4) {
    BX_PANIC(("APIC write with len=%d (should be 4)", len));
    return;
  }

  if(addr & 0xf) {
    BX_PANIC(("APIC write at unaligned address 0x" FMT_PHY_ADDRX, addr));
    return;
  }

  write_aligned(addr, *((Bit32u*) data));
}

#define BX_LAPIC_ID                   0x020
#define BX_LAPIC_VERSION              0x030
#define BX_LAPIC_TPR                  0x080
#define BX_LAPIC_ARBITRATION_PRIORITY 0x090
#define BX_LAPIC_PPR                  0x0A0
#define BX_LAPIC_EOI                  0x0B0
#define BX_LAPIC_LDR                  0x0D0
#define BX_LAPIC_DESTINATION_FORMAT   0x0E0
#define BX_LAPIC_SPURIOUS_VECTOR      0x0F0
#define BX_LAPIC_ISR1                 0x100
#define BX_LAPIC_ISR2                 0x110
#define BX_LAPIC_ISR3                 0x120
#define BX_LAPIC_ISR4                 0x130
#define BX_LAPIC_ISR5                 0x140
#define BX_LAPIC_ISR6                 0x150
#define BX_LAPIC_ISR7                 0x160
#define BX_LAPIC_ISR8                 0x170
#define BX_LAPIC_TMR1                 0x180
#define BX_LAPIC_TMR2                 0x190
#define BX_LAPIC_TMR3                 0x1A0
#define BX_LAPIC_TMR4                 0x1B0
#define BX_LAPIC_TMR5                 0x1C0
#define BX_LAPIC_TMR6                 0x1D0
#define BX_LAPIC_TMR7                 0x1E0
#define BX_LAPIC_TMR8                 0x1F0
#define BX_LAPIC_IRR1                 0x200
#define BX_LAPIC_IRR2                 0x210
#define BX_LAPIC_IRR3                 0x220
#define BX_LAPIC_IRR4                 0x230
#define BX_LAPIC_IRR5                 0x240
#define BX_LAPIC_IRR6                 0x250
#define BX_LAPIC_IRR7                 0x260
#define BX_LAPIC_IRR8                 0x270
#define BX_LAPIC_ESR                  0x280
#define BX_LAPIC_LVT_CMCI             0x2F0
#define BX_LAPIC_ICR_LO               0x300
#define BX_LAPIC_ICR_HI               0x310
#define BX_LAPIC_LVT_TIMER            0x320
#define BX_LAPIC_LVT_THERMAL          0x330
#define BX_LAPIC_LVT_PERFMON          0x340
#define BX_LAPIC_LVT_LINT0            0x350
#define BX_LAPIC_LVT_LINT1            0x360
#define BX_LAPIC_LVT_ERROR            0x370
#define BX_LAPIC_TIMER_INITIAL_COUNT  0x380
#define BX_LAPIC_TIMER_CURRENT_COUNT  0x390
#define BX_LAPIC_TIMER_DIVIDE_CFG     0x3E0
#define BX_LAPIC_SELF_IPI             0x3F0

// APIC write: 4 byte write to 16-byte aligned APIC address
void bx_local_apic_c::write_aligned(bx_phy_address addr, Bit32u value)
{
  BX_ASSERT((addr & 0xf) == 0);
  unsigned apic_reg = addr & 0xff0;
  BX_DEBUG(("LAPIC write 0x%08x to register 0x%04x", value, apic_reg));
  switch(apic_reg) {
    case BX_LAPIC_TPR: // task priority
      set_tpr(value & 0xff);
      break;
    case BX_LAPIC_EOI: // EOI
      receive_EOI(value);
      break;
    case BX_LAPIC_LDR: // logical destination
      ldr = (value >> 24) & APIC_ID_MASK;
      BX_DEBUG(("set logical destination to %08x", ldr));
      break;
    case BX_LAPIC_DESTINATION_FORMAT:
      dest_format = (value >> 28) & 0xf;
      BX_DEBUG(("set destination format to %02x", dest_format));
      break;
    case BX_LAPIC_SPURIOUS_VECTOR:
      write_spurious_interrupt_register(value);
      break;
    case BX_LAPIC_ESR: // error status reg
      // Here's what the IA-devguide-3 says on p.7-45:
      // The ESR is a read/write register and is reset after being written to
      // by the processor. A write to the ESR must be done just prior to
      // reading the ESR to allow the register to be updated.
      error_status = shadow_error_status;
      shadow_error_status = 0;
      break;
    case BX_LAPIC_ICR_LO: // interrupt command reg 0-31
      icr_lo = value & ~(1<<12);  // force delivery status bit = 0(idle)
      send_ipi();
      break;
    case BX_LAPIC_ICR_HI: // interrupt command reg 31-63
      icr_hi = value & 0xff000000;
      break;
    case BX_LAPIC_LVT_TIMER:   // LVT Timer Reg
    case BX_LAPIC_LVT_THERMAL: // LVT Thermal Monitor
    case BX_LAPIC_LVT_PERFMON: // LVT Performance Counter
    case BX_LAPIC_LVT_LINT0:   // LVT LINT0 Reg
    case BX_LAPIC_LVT_LINT1:   // LVT LINT1 Reg
    case BX_LAPIC_LVT_ERROR:   // LVT Error Reg
      {
         static Bit32u lvt_mask[] = {
           0x000310ff, /* TIMER */
           0x000117ff, /* THERMAL */
           0x000117ff, /* PERFMON */
           0x0001f7ff, /* LINT0 */
           0x0001f7ff, /* LINT1 */
           0x000110ff  /* ERROR */
         };
         unsigned lvt_entry = (apic_reg - BX_LAPIC_LVT_TIMER) >> 4;
         lvt[lvt_entry] = value & lvt_mask[lvt_entry];
         if(! software_enabled) lvt[lvt_entry] |= 0x10000;
         break;
      }
    case BX_LAPIC_TIMER_INITIAL_COUNT:
      set_initial_timer_count(value);
      break;
    case BX_LAPIC_TIMER_DIVIDE_CFG:
      // only bits 3, 1, and 0 are writable
      timer_divconf = value & 0xb;
      set_divide_configuration(timer_divconf);
      break;
    /* all read-only registers go here */
    case BX_LAPIC_ID:      // local APIC id
    case BX_LAPIC_VERSION: // local APIC version
    case BX_LAPIC_ARBITRATION_PRIORITY:
    case BX_LAPIC_PPR:     // processor priority
    // ISRs not writable
    case BX_LAPIC_ISR1: case BX_LAPIC_ISR2:
    case BX_LAPIC_ISR3: case BX_LAPIC_ISR4:
    case BX_LAPIC_ISR5: case BX_LAPIC_ISR6:
    case BX_LAPIC_ISR7: case BX_LAPIC_ISR8:
    // TMRs not writable
    case BX_LAPIC_TMR1: case BX_LAPIC_TMR2:
    case BX_LAPIC_TMR3: case BX_LAPIC_TMR4:
    case BX_LAPIC_TMR5: case BX_LAPIC_TMR6:
    case BX_LAPIC_TMR7: case BX_LAPIC_TMR8:
    // IRRs not writable
    case BX_LAPIC_IRR1: case BX_LAPIC_IRR2:
    case BX_LAPIC_IRR3: case BX_LAPIC_IRR4:
    case BX_LAPIC_IRR5: case BX_LAPIC_IRR6:
    case BX_LAPIC_IRR7: case BX_LAPIC_IRR8:
    // current count for timer
    case BX_LAPIC_TIMER_CURRENT_COUNT:
      // all read-only registers should fall into this line
      BX_INFO(("warning: write to read-only APIC register 0x%x", apic_reg));
      break;
    default:
      shadow_error_status |= APIC_ERR_ILLEGAL_ADDR;
      // but for now I want to know about it in case I missed some.
      BX_PANIC(("APIC register %x not implemented", apic_reg));
  }
}

void bx_local_apic_c::send_ipi(void)
{
  int dest = (icr_hi >> 24) & 0xff;
  int dest_shorthand = (icr_lo >> 18) & 3;
  int trig_mode = (icr_lo >> 15) & 1;
  int level = (icr_lo >> 14) & 1;
  int logical_dest = (icr_lo >> 11) & 1;
  int delivery_mode = (icr_lo >> 8) & 7;
  int vector = (icr_lo & 0xff);
  int accepted = 0;

  if(delivery_mode == APIC_DM_INIT)
  {
    if(level == 0 && trig_mode == 1) {
      // special mode in local apic.  See "INIT Level Deassert" in the
      // Intel Soft. Devel. Guide Vol 3, page 7-34.  This magic code
      // causes all APICs(regardless of dest address) to set their
      // arbitration ID to their APIC ID. Not supported by Pentium 4
      // and Intel Xeon processors.
      return; // we not model APIC bus arbitration ID anyway
    }
  }

  switch(dest_shorthand) {
  case 0:  // no shorthand, use real destination value
    accepted = apic_bus_deliver_interrupt(vector, dest, delivery_mode, logical_dest, level, trig_mode);
    break;
  case 1:  // self
    trigger_irq(vector, trig_mode);
    accepted = 1;
    break;
  case 2:  // all including self
    accepted = apic_bus_broadcast_interrupt(vector, delivery_mode, trig_mode, APIC_MAX_ID);
    break;
  case 3:  // all but self
    accepted = apic_bus_broadcast_interrupt(vector, delivery_mode, trig_mode, get_id());
    break;
  default:
    BX_PANIC(("Invalid desination shorthand %#x\n", dest_shorthand));
  }

  if(! accepted) {
    BX_DEBUG(("An IPI wasn't accepted, raise APIC_ERR_TX_ACCEPT_ERR"));
    shadow_error_status |= APIC_ERR_TX_ACCEPT_ERR;
  }
}

void bx_local_apic_c::write_spurious_interrupt_register(Bit32u value)
{
  BX_DEBUG(("write of %08x to spurious interrupt register", value));

#ifdef BX_IMPLEMENT_XAPIC
  spurious_vector = value & 0xff;
#else
  // bits 0-3 of the spurious vector hardwired to '1
  spurious_vector = (value & 0xf0) | 0xf;
#endif

  software_enabled = (value >> 8) & 1;
  focus_disable    = (value >> 9) & 1;

  if(! software_enabled) {
    for(unsigned i=0; i<APIC_LVT_ENTRIES; i++) {
      lvt[i] |= 0x10000;	// all LVT are masked
    }
  }
}

void bx_local_apic_c::receive_EOI(Bit32u value)
{
  BX_DEBUG(("Wrote 0x%x to EOI", value));
  int vec = highest_priority_int(isr);
  if(vec < 0) {
    BX_DEBUG(("EOI written without any bit in ISR"));
  } else {
    if ((Bit32u) vec != spurious_vector) {
       BX_DEBUG(("local apic received EOI, hopefully for vector 0x%02x", vec));
       isr[vec] = 0;
       if(tmr[vec]) {
           apic_bus_broadcast_eoi(vec);
           tmr[vec] = 0;
       }
       service_local_apic();
    }
  }

  if(bx_dbg.apic) print_status();
}

void bx_local_apic_c::startup_msg(Bit8u vector)
{
  cpu->deliver_SIPI(vector);
}

// APIC read: 4 byte read from 16-byte aligned APIC address
Bit32u bx_local_apic_c::read_aligned(bx_phy_address addr)
{
  BX_ASSERT((addr & 0xf) == 0);
  Bit32u data = 0;  // default value for unimplemented registers
  unsigned apic_reg = addr & 0xff0;
  BX_DEBUG(("LAPIC read from register 0x%04x", apic_reg));
  switch(apic_reg) {
  case BX_LAPIC_ID:      // local APIC id
    data = apic_id << 24; break;
  case BX_LAPIC_VERSION: // local APIC version
    data = BX_LAPIC_VERSION_ID; break;
  case BX_LAPIC_TPR:     // task priority
    data = task_priority & 0xff; break;
  case BX_LAPIC_ARBITRATION_PRIORITY:
    data = get_apr(); break;
  case BX_LAPIC_PPR:     // processor priority
    data = get_ppr(); break;
  case BX_LAPIC_EOI:     // EOI
    /*
     * Read-modify-write operations should operate without generating
     * exceptions, and are used by some operating systems to EOI.
     * The results of reads should be ignored by the OS.
     */
    break;
  case BX_LAPIC_LDR:     // logical destination
    data = (ldr & APIC_ID_MASK) << 24; break;
  case BX_LAPIC_DESTINATION_FORMAT:
    data = ((dest_format & 0xf) << 28) | 0x0fffffff; break;
  case BX_LAPIC_SPURIOUS_VECTOR:
    {
      Bit32u reg = spurious_vector;
      if(software_enabled) reg |= 0x100;
      if(focus_disable) reg |= 0x200;
      data = reg;
    }
    break;
  case BX_LAPIC_ISR1: case BX_LAPIC_ISR2:
  case BX_LAPIC_ISR3: case BX_LAPIC_ISR4:
  case BX_LAPIC_ISR5: case BX_LAPIC_ISR6:
  case BX_LAPIC_ISR7: case BX_LAPIC_ISR8:
    {
      unsigned index = (apic_reg - BX_LAPIC_ISR1) << 1;
      Bit32u value = 0, mask = 1;
      for(int i=0;i<32;i++) {
        if(isr[index+i]) value |= mask;
        mask <<= 1;
      }
      data = value;
    }
    break;
  case BX_LAPIC_TMR1: case BX_LAPIC_TMR2:
  case BX_LAPIC_TMR3: case BX_LAPIC_TMR4:
  case BX_LAPIC_TMR5: case BX_LAPIC_TMR6:
  case BX_LAPIC_TMR7: case BX_LAPIC_TMR8:
    {
      unsigned index = (apic_reg - BX_LAPIC_TMR1) << 1;
      Bit32u value = 0, mask = 1;
      for(int i=0;i<32;i++) {
        if(tmr[index+i]) value |= mask;
        mask <<= 1;
      }
      data = value;
    }
    break;
  case BX_LAPIC_IRR1: case BX_LAPIC_IRR2:
  case BX_LAPIC_IRR3: case BX_LAPIC_IRR4:
  case BX_LAPIC_IRR5: case BX_LAPIC_IRR6:
  case BX_LAPIC_IRR7: case BX_LAPIC_IRR8:
    {
      unsigned index = (apic_reg - BX_LAPIC_IRR1) << 1;
      Bit32u value = 0, mask = 1;
      for(int i=0;i<32;i++) {
        if(irr[index+i]) value |= mask;
        mask <<= 1;
      }
      data = value;
    }
    break;
  case BX_LAPIC_ESR:     // error status reg
    data = error_status; break;
  case BX_LAPIC_ICR_LO:  // interrupt command reg  0-31
    data = icr_lo; break;
  case BX_LAPIC_ICR_HI:  // interrupt command reg 31-63
    data = icr_hi; break;
  case BX_LAPIC_LVT_TIMER:   // LVT Timer Reg
  case BX_LAPIC_LVT_THERMAL: // LVT Thermal Monitor
  case BX_LAPIC_LVT_PERFMON: // LVT Performance Counter
  case BX_LAPIC_LVT_LINT0:   // LVT LINT0 Reg
  case BX_LAPIC_LVT_LINT1:   // LVT Lint1 Reg
  case BX_LAPIC_LVT_ERROR:   // LVT Error Reg
    {
      int index = (apic_reg - BX_LAPIC_LVT_TIMER) >> 4;
      data = lvt[index];
      break;
    }
  case BX_LAPIC_TIMER_INITIAL_COUNT: // initial count for timer
    data = timer_initial;
    break;
  case BX_LAPIC_TIMER_CURRENT_COUNT: // current count for timer
    if(timer_active==0) {
      data = timer_current;
    } else {
      Bit64u delta64 = (bx_pc_system.time_ticks() - ticksInitial) / timer_divide_factor;
      Bit32u delta32 = (Bit32u) delta64;
      if(delta32 > timer_initial)
        BX_PANIC(("APIC: R(curr timer count): delta < initial"));
      timer_current = timer_initial - delta32;
      data = timer_current;
    }
    break;
  case BX_LAPIC_TIMER_DIVIDE_CFG:   // timer divide configuration
    data = timer_divconf;
    break;
  default:
    BX_INFO(("APIC register %08x not implemented", apic_reg));
  }

  BX_DEBUG(("read from APIC address 0x" FMT_PHY_ADDRX " = %08x", addr, data));
  return data;
}

int bx_local_apic_c::highest_priority_int(Bit8u *array)
{
  for(int i=BX_LAPIC_LAST_VECTOR; i>=BX_LAPIC_FIRST_VECTOR; i--)
    if(array[i]) return i;

  return -1;
}

void bx_local_apic_c::service_local_apic(void)
{
  if(bx_dbg.apic) {
    BX_INFO(("service_local_apic()"));
    print_status();
  }
  if(INTR) return;  // INTR already up; do nothing
  // find first interrupt in irr.
  int first_irr = highest_priority_int(irr);
  if (first_irr < 0) return;   // no interrupts, leave INTR=0
  int first_isr = highest_priority_int(isr);
  if (first_isr >= 0 && first_irr <= first_isr) {
    BX_DEBUG(("lapic(%d): not delivering int 0x%02x because int 0x%02x is in service", apic_id, first_irr, first_isr));
    return;
  }
  if(((Bit32u)(first_irr) & 0xf0) <= (task_priority & 0xf0)) {
    BX_DEBUG(("lapic(%d): not delivering int 0x%02X because task_priority is 0x%02X", apic_id, first_irr, task_priority));
    return;
  }
  // interrupt has appeared in irr. Raise INTR. When the CPU
  // acknowledges, we will run highest_priority_int again and
  // return it.
  BX_DEBUG(("service_local_apic(): setting INTR=1 for vector 0x%02x", first_irr));
  INTR = 1;
  cpu->async_event = 1;
}

bx_bool bx_local_apic_c::deliver(Bit8u vector, Bit8u delivery_mode, Bit8u trig_mode)
{
  switch(delivery_mode) {
  case APIC_DM_FIXED:
  case APIC_DM_LOWPRI:
    BX_DEBUG(("Deliver lowest priority of fixed interrupt vector %02x", vector));
    trigger_irq(vector, trig_mode);
    break;
  case APIC_DM_SMI:
    BX_INFO(("Deliver SMI"));
    cpu->deliver_SMI();
    return 1;
  case APIC_DM_NMI:
    BX_INFO(("Deliver NMI"));
    cpu->deliver_NMI();
    return 1;
  case APIC_DM_INIT:
    BX_INFO(("Deliver INIT IPI"));
    cpu->deliver_INIT();
    break;
  case APIC_DM_SIPI:
    BX_INFO(("Deliver Start Up IPI"));
    startup_msg(vector);
    break;
  case APIC_DM_EXTINT:
    BX_DEBUG(("Deliver EXTINT vector %02x", vector));
    trigger_irq(vector, trig_mode, 1);
    break;
  default:
    return 0;
  }

  return 1;
}

void bx_local_apic_c::trigger_irq(Bit8u vector, unsigned trigger_mode, bx_bool bypass_irr_isr)
{
  BX_DEBUG(("trigger interrupt vector=0x%02x", vector));

  if(vector > BX_LAPIC_LAST_VECTOR || vector < BX_LAPIC_FIRST_VECTOR) {
    shadow_error_status |= APIC_ERR_RX_ILLEGAL_VEC;
    BX_INFO(("bogus vector %#x, ignoring ...", vector));
    return;
  }

  BX_DEBUG(("triggered vector %#02x", vector));

  if(bypass_irr_isr) {
    goto service_vector;
  }

  if(irr[vector] != 0) {
    BX_DEBUG(("triggered vector %#02x not accepted", vector));
    return;
  }

service_vector:
  irr[vector] = 1;
  tmr[vector] = trigger_mode;	// set for level triggered
  service_local_apic();
}

void bx_local_apic_c::untrigger_irq(Bit8u vector, unsigned trigger_mode)
{
  BX_DEBUG(("untrigger interrupt vector=0x%02x", vector));
  // hardware says "no more".  clear the bit.  If the CPU hasn't yet
  // acknowledged the interrupt, it will never be serviced.
  BX_ASSERT(irr[vector] == 1);
  irr[vector] = 0;
  if(bx_dbg.apic) print_status();
}

Bit8u bx_local_apic_c::acknowledge_int(void)
{
  // CPU calls this when it is ready to service one interrupt
  if(!INTR)
    BX_PANIC(("APIC %d acknowledged an interrupt, but INTR=0", apic_id));

  BX_ASSERT(INTR);
  int vector = highest_priority_int(irr);
  if (vector < 0) goto spurious;
  if((vector & 0xf0) <= get_ppr()) goto spurious;
  BX_ASSERT(irr[vector] == 1);
  BX_DEBUG(("acknowledge_int() returning vector 0x%02x", vector));
  irr[vector] = 0;
  isr[vector] = 1;
  if(bx_dbg.apic) {
    BX_INFO(("Status after setting isr:"));
    print_status();
  }
  INTR = 0;
  cpu->async_event = 1;
  service_local_apic();  // will set INTR again if another is ready
  return vector;

spurious:
  INTR = 0;
  cpu->async_event = 1;
  return spurious_vector;
}

void bx_local_apic_c::print_status(void)
{
  BX_INFO(("lapic %d: status is {:", apic_id));
  for(int vec=0; vec<BX_LAPIC_MAX_INTS; vec++) {
    if(irr[vec] || isr[vec]) {
      BX_INFO(("vec 0x%x: irr=%d, isr=%d", vec,(int)irr[vec],(int)isr[vec]));
    }
  }
  BX_INFO(("}"));
}

bx_bool bx_local_apic_c::match_logical_addr(Bit8u address)
{
  bx_bool match = 0;

  if (dest_format == 0xf) {
    // flat model
    match = ((address & ldr) != 0);
    BX_DEBUG(("comparing MDA %02x to my LDR %02x -> %s", address,
      ldr, match ? "Match" : "Not a match"));
  }
  else if (dest_format == 0) {
    // cluster model
    if (address == 0xff) // broadcast all
      return 1;

    if ((unsigned)(address & 0xf0) == (unsigned)(ldr & 0xf0))
      match = ((address & ldr & 0x0f) != 0);
  }
  else {
    BX_PANIC(("bx_local_apic_c::match_logical_addr: unsupported dest format 0x%x", dest_format));
  }

  return match;
}

Bit8u bx_local_apic_c::get_ppr(void)
{
  int ppr = highest_priority_int(isr);

  if((ppr < 0) || ((task_priority & 0xF0) >= ((Bit32u) ppr & 0xF0)))
    ppr = task_priority;
  else
    ppr &= 0xF0;

  return ppr;
}

void bx_local_apic_c::set_tpr(Bit8u priority)
{
  if(priority < task_priority) {
    task_priority = priority;
    service_local_apic();
  } else {
    task_priority = priority;
  }
}

Bit8u bx_local_apic_c::get_apr(void)
{
  Bit32u tpr  = (task_priority >> 4) & 0xf;
  int first_isr = highest_priority_int(isr);
  if (first_isr < 0) first_isr = 0;
  int first_irr = highest_priority_int(irr);
  if (first_irr < 0) first_irr = 0;
  Bit32u isrv = (first_isr >> 4) & 0xf;
  Bit32u irrv = (first_irr >> 4) & 0xf;
  Bit8u  apr;

  if((tpr >= irrv) && (tpr > isrv)) {
    apr = task_priority & 0xff;
  }
  else {
    apr = ((tpr & isrv) > irrv) ?(tpr & isrv) : irrv;
    apr <<= 4;
  }

  BX_DEBUG(("apr = %d\n", apr));

  return(Bit8u) apr;
}

bx_bool bx_local_apic_c::is_focus(Bit8u vector)
{
  if(focus_disable) return 0;
  return(irr[vector] || isr[vector]) ? 1 : 0;
}

void bx_local_apic_c::periodic_smf(void *this_ptr)
{
  bx_local_apic_c *class_ptr = (bx_local_apic_c *) this_ptr;
  class_ptr->periodic();
}

void bx_local_apic_c::periodic(void)
{
  if(!timer_active) {
    BX_ERROR(("bx_local_apic_c::periodic called, timer_active==0"));
    return;
  }

  // timer reached zero since the last call to periodic.
  Bit32u timervec = lvt[APIC_LVT_TIMER];
  if(timervec & 0x20000) {
    // Periodic mode.
    // If timer is not masked, trigger interrupt.
    if((timervec & 0x10000)==0) {
      trigger_irq(timervec & 0xff, APIC_EDGE_TRIGGERED);
    }
    else {
      BX_DEBUG(("local apic timer LVT masked"));
    }
    // Reload timer values.
    timer_current = timer_initial;
    ticksInitial = bx_pc_system.time_ticks(); // Take a reading.
    BX_DEBUG(("local apic timer(periodic) triggered int, reset counter to 0x%08x", timer_current));
  }
  else {
    // one-shot mode
    timer_current = 0;
    // If timer is not masked, trigger interrupt.
    if((timervec & 0x10000)==0) {
      trigger_irq(timervec & 0xff, APIC_EDGE_TRIGGERED);
    }
    else {
      BX_DEBUG(("local apic timer LVT masked"));
    }
    timer_active = 0;
    BX_DEBUG(("local apic timer(one-shot) triggered int"));
    bx_pc_system.deactivate_timer(timer_handle);
  }
}

void bx_local_apic_c::set_divide_configuration(Bit32u value)
{
  BX_ASSERT(value == (value & 0x0b));
  // move bit 3 down to bit 0.
  value = ((value & 8) >> 1) | (value & 3);
  BX_ASSERT(value >= 0 && value <= 7);
  timer_divide_factor = (value==7) ? 1 : (2 << value);
  BX_INFO(("set timer divide factor to %d", timer_divide_factor));
}

void bx_local_apic_c::set_initial_timer_count(Bit32u value)
{
  // If active before, deactive the current timer before changing it.
  if(timer_active) {
    bx_pc_system.deactivate_timer(timer_handle);
    timer_active = 0;
  }

  timer_initial = value;
  timer_current = 0;

  if(timer_initial != 0)  // terminate the counting if timer_initial = 0
  {
    // This should trigger the counter to start.  If already started,
    // restart from the new start value.
    BX_DEBUG(("APIC: Initial Timer Count Register = %u", value));
    timer_current = timer_initial;
    timer_active = 1;
    Bit32u timervec = lvt[APIC_LVT_TIMER];
    bx_bool continuous = (timervec & 0x20000) > 0;
    ticksInitial = bx_pc_system.time_ticks(); // Take a reading.
    bx_pc_system.activate_timer_ticks(timer_handle,
            Bit64u(timer_initial) * Bit64u(timer_divide_factor), continuous);
  }
}

void bx_local_apic_c::register_state(bx_param_c *parent)
{
  unsigned i;
  char name[6];

  bx_list_c *lapic = new bx_list_c(parent, "local_apic", 25);

  BXRS_HEX_PARAM_SIMPLE(lapic, base_addr);
  BXRS_HEX_PARAM_SIMPLE(lapic, apic_id);
  BXRS_HEX_PARAM_SIMPLE(lapic, mode);
  BXRS_HEX_PARAM_SIMPLE(lapic, spurious_vector);
  BXRS_PARAM_BOOL(lapic, software_enabled, software_enabled);
  BXRS_PARAM_BOOL(lapic, focus_disable, focus_disable);
  BXRS_HEX_PARAM_SIMPLE(lapic, task_priority);
  BXRS_HEX_PARAM_SIMPLE(lapic, ldr);
  BXRS_HEX_PARAM_SIMPLE(lapic, dest_format);

  bx_list_c *ISR = new bx_list_c(lapic, "isr", BX_LAPIC_MAX_INTS);
  bx_list_c *TMR = new bx_list_c(lapic, "tmr", BX_LAPIC_MAX_INTS);
  bx_list_c *IRR = new bx_list_c(lapic, "irr", BX_LAPIC_MAX_INTS);
  for (i=0; i<BX_LAPIC_MAX_INTS; i++) {
    sprintf(name, "0x%02x", i);
    new bx_shadow_num_c(ISR, name, &isr[i]);
    new bx_shadow_num_c(TMR, name, &tmr[i]);
    new bx_shadow_num_c(IRR, name, &irr[i]);
  }

  BXRS_HEX_PARAM_SIMPLE(lapic, error_status);
  BXRS_HEX_PARAM_SIMPLE(lapic, shadow_error_status);
  BXRS_HEX_PARAM_SIMPLE(lapic, icr_hi);
  BXRS_HEX_PARAM_SIMPLE(lapic, icr_lo);

  bx_list_c *LVT = new bx_list_c(lapic, "lvt", APIC_LVT_ENTRIES);
  for (i=0; i<APIC_LVT_ENTRIES; i++) {
    sprintf(name, "%d", i);
    new bx_shadow_num_c(LVT, name, &lvt[i], BASE_HEX);
  }

  BXRS_HEX_PARAM_SIMPLE(lapic, timer_initial);
  BXRS_HEX_PARAM_SIMPLE(lapic, timer_current);
  BXRS_HEX_PARAM_SIMPLE(lapic, timer_divconf);
  BXRS_DEC_PARAM_SIMPLE(lapic, timer_divide_factor);
  BXRS_PARAM_BOOL(lapic, timer_active, timer_active);
  BXRS_HEX_PARAM_SIMPLE(lapic, ticksInitial);
  BXRS_PARAM_BOOL(lapic, INTR, INTR);
}

#endif /* if BX_SUPPORT_APIC */
