/////////////////////////////////////////////////////////////////////////
// $Id$
/////////////////////////////////////////////////////////////////////////
//
//   Copyright (c) 2011 Stanislav Shwartsman
//          Written by Stanislav Shwartsman [sshwarts at sourceforge net]
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

#include "bochs.h"
#include "cpu.h"
#include "param_names.h"
#include "generic_cpuid.h"

#define LOG_THIS cpu->

#if BX_CPU_LEVEL >= 4

bx_generic_cpuid_t::bx_generic_cpuid_t(BX_CPU_C *cpu): bx_cpuid_t(cpu)
{
#if BX_SUPPORT_SMP
  nthreads = SIM->get_param_num(BXPN_CPU_NTHREADS)->get();
  ncores = SIM->get_param_num(BXPN_CPU_NCORES)->get();
  nprocessors = SIM->get_param_num(BXPN_CPU_NPROCESSORS)->get();
#endif

  init_isa_extensions_bitmask();
  init_cpu_extensions_bitmask();
}

void bx_generic_cpuid_t::get_cpuid_leaf(Bit32u function, Bit32u subfunction, cpuid_function_t *leaf) const
{
  static bx_bool cpuid_limit_winnt = SIM->get_param_bool(BXPN_CPUID_LIMIT_WINNT)->get();
  if (cpuid_limit_winnt)
    if (function > 2 && function < 0x80000000) function = 2;

  switch(function) {
#if BX_CPU_LEVEL >= 6
  case 0x80000000:
    get_ext_cpuid_leaf_0(leaf);
    return;
  case 0x80000001:
    get_ext_cpuid_leaf_1(leaf);
    return;
  case 0x80000002:
  case 0x80000003:
  case 0x80000004:
    get_ext_cpuid_brand_string_leaf(function, leaf);
    return;
#if BX_SUPPORT_X86_64
  case 0x80000005:
    get_ext_cpuid_leaf_5(leaf);
    return;
  case 0x80000006:
    get_ext_cpuid_leaf_6(leaf);
    return;
  case 0x80000007:
    get_ext_cpuid_leaf_7(leaf);
    return;
  case 0x80000008:
    get_ext_cpuid_leaf_8(leaf);
    return;
#endif
#endif
  case 0x00000000:
    get_std_cpuid_leaf_0(leaf);
    return;
  case 0x00000001:
    get_std_cpuid_leaf_1(leaf);
    return;
#if BX_CPU_LEVEL >= 6
  case 0x00000002:
    get_std_cpuid_leaf_2(leaf);
    return;
  case 0x00000003:
    get_std_cpuid_leaf_3(leaf);
    return;
  case 0x00000004:
    get_std_cpuid_leaf_4(subfunction, leaf);
    return;
  case 0x00000005:
    get_std_cpuid_leaf_5(leaf);
    return;
  case 0x00000006:
    get_std_cpuid_leaf_6(leaf);
    return;
  case 0x00000007:
    get_std_cpuid_leaf_7(subfunction, leaf);
    return;
  case 0x00000008:
    get_reserved_leaf(leaf);
    return;
  case 0x00000009:
    get_std_cpuid_leaf_9(leaf);
    return;
  case 0x0000000A:
    get_std_cpuid_leaf_A(leaf);
    return;
  case 0x0000000B:
    get_std_cpuid_extended_topology_leaf(subfunction, leaf);
    return;
  case 0x0000000C:
    get_reserved_leaf(leaf);
    return;
  case 0x0000000D:
  default:
    get_std_cpuid_xsave_leaf(subfunction, leaf);
    return;
#endif
  }
}

// leaf 0x00000000 //
void bx_generic_cpuid_t::get_std_cpuid_leaf_0(cpuid_function_t *leaf) const
{
  static Bit8u *vendor_string = (Bit8u *)SIM->get_param_string(BXPN_VENDOR_STRING)->getptr();

  // EAX: highest std function understood by CPUID
  // EBX: vendor ID string
  // EDX: vendor ID string
  // ECX: vendor ID string

#if BX_CPU_LEVEL <= 5
  // 486 and Pentium processors
  leaf->eax = 1;
#else
  // for Pentium Pro, Pentium II, Pentium 4 processors
  leaf->eax = 2;

  // do not report CPUID functions above 0x3 if cpuid_limit_winnt is set
  // to workaround WinNT issue.
  static bx_bool cpuid_limit_winnt = SIM->get_param_bool(BXPN_CPUID_LIMIT_WINNT)->get();
  if (! cpuid_limit_winnt) {
    if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_MONITOR_MWAIT))
      leaf->eax = 0x5;
    if (BX_CPUID_SUPPORT_CPU_EXTENSION(BX_CPU_X2APIC))
      leaf->eax = 0xb;
    if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_XSAVE))
      leaf->eax = 0xd;
  }
#endif

  // CPUID vendor string (e.g. GenuineIntel, AuthenticAMD, CentaurHauls, ...)
  memcpy(&(leaf->ebx), vendor_string,     4);
  memcpy(&(leaf->edx), vendor_string + 4, 4);
  memcpy(&(leaf->ecx), vendor_string + 8, 4);
#ifdef BX_BIG_ENDIAN
  leaf->ebx = bx_bswap32(leaf->ebx);
  leaf->ecx = bx_bswap32(leaf->ecx);
  leaf->edx = bx_bswap32(leaf->edx);
#endif
}

// leaf 0x00000001 //
void bx_generic_cpuid_t::get_std_cpuid_leaf_1(cpuid_function_t *leaf) const
{
  // EAX:       CPU Version Information
  //   [3:0]   Stepping ID
  //   [7:4]   Model: starts at 1
  //   [11:8]  Family: 4=486, 5=Pentium, 6=PPro, ...
  //   [13:12] Type: 0=OEM, 1=overdrive, 2=dual cpu, 3=reserved
  //   [19:16] Extended Model
  //   [27:20] Extended Family
  leaf->eax = get_cpu_version_information();

  // EBX:
  //   [7:0]   Brand ID
  //   [15:8]  CLFLUSH cache line size (value*8 = cache line size in bytes)
  //   [23:16] Number of logical processors in one physical processor
  //   [31:24] Local Apic ID
  leaf->ebx = 0;
  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_CLFLUSH)) {
    leaf->ebx |= (CACHE_LINE_SIZE / 8) << 8;
  }
#if BX_SUPPORT_SMP
  unsigned n_logical_processors = ncores*nthreads;
#else
  unsigned n_logical_processors = 1;
#endif
  leaf->ebx |= (n_logical_processors << 16);
#if BX_SUPPORT_APIC
  leaf->ebx |= ((cpu->get_apic_id() & 0xff) << 24);
#endif

  // ECX: Extended Feature Flags
#if BX_CPU_LEVEL >= 6
  leaf->ecx = get_extended_cpuid_features();
#else
  leaf->ecx = 0;
#endif

  // EDX: Standard Feature Flags
  leaf->edx = get_std_cpuid_features();
}

#if BX_CPU_LEVEL >= 6

// leaf 0x00000002 //
void bx_generic_cpuid_t::get_std_cpuid_leaf_2(cpuid_function_t *leaf) const
{
  // CPUID function 0x00000002 - Cache and TLB Descriptors
#if BX_CPU_VENDOR_INTEL
  leaf->eax = 0x00410601;  // for Pentium Pro compatibility
  leaf->ebx = 0;
  leaf->ecx = 0;
  leaf->edx = 0;
#else
  leaf->eax = 0;           // ignore for AMD
  leaf->ebx = 0;
  leaf->ecx = 0;
  leaf->edx = 0;
#endif
}

// leaf 0x00000003 //
void bx_generic_cpuid_t::get_std_cpuid_leaf_3(cpuid_function_t *leaf) const
{
  // CPUID function 0x00000003 - Processor Serial Number
  leaf->eax = 0;
  leaf->ebx = 0;
  leaf->ecx = 0;
  leaf->edx = 0;
}

// leaf 0x00000004 //
void bx_generic_cpuid_t::get_std_cpuid_leaf_4(Bit32u subfunction, cpuid_function_t *leaf) const
{
  // CPUID function 0x00000004 - Deterministic Cache Parameters
  leaf->eax = 0;
  leaf->ebx = 0;
  leaf->ecx = 0;
  leaf->edx = 0;
}

// leaf 0x00000005 //
void bx_generic_cpuid_t::get_std_cpuid_leaf_5(cpuid_function_t *leaf) const
{
  // CPUID function 0x00000005 - MONITOR/MWAIT Leaf
#if BX_SUPPORT_MONITOR_MWAIT
  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_MONITOR_MWAIT))
  {
    // EAX - Smallest monitor-line size in bytes
    // EBX - Largest  monitor-line size in bytes
    // ECX -
    //   [31:2] - reserved
    //    [1:1] - exit MWAIT even with EFLAGS.IF = 0
    //    [0:0] - MONITOR/MWAIT extensions are supported
    // EDX -
    //  [03-00] - number of C0 sub C-states supported using MWAIT
    //  [07-04] - number of C1 sub C-states supported using MWAIT
    //  [11-08] - number of C2 sub C-states supported using MWAIT
    //  [15-12] - number of C3 sub C-states supported using MWAIT
    //  [19-16] - number of C4 sub C-states supported using MWAIT
    //  [31-20] - reserved (MBZ)
    leaf->eax = CACHE_LINE_SIZE;
    leaf->ebx = CACHE_LINE_SIZE;
    leaf->ecx = 3;
    leaf->edx = 0x00000020;
  }
  else
#endif
  {
    leaf->eax = 0;
    leaf->ebx = 0;
    leaf->ecx = 0;
    leaf->edx = 0;
  }
}

// leaf 0x00000006 //
void bx_generic_cpuid_t::get_std_cpuid_leaf_6(cpuid_function_t *leaf) const
{
  // CPUID function 0x00000006 - Thermal and Power Management Leaf
  leaf->eax = 0;
  leaf->ebx = 0;
  leaf->ecx = 0;
  leaf->edx = 0;
}

// leaf 0x00000007 //
void bx_generic_cpuid_t::get_std_cpuid_leaf_7(Bit32u subfunction, cpuid_function_t *leaf) const
{
  leaf->eax = 0; /* report max sub-leaf that supported in leaf 7 */
  leaf->ebx = get_ext3_cpuid_features();
  leaf->ecx = 0;
  leaf->edx = 0;
}

// leaf 0x00000008 - reserved //

// leaf 0x00000009 //
void bx_generic_cpuid_t::get_std_cpuid_leaf_9(cpuid_function_t *leaf) const
{
  // CPUID function 0x00000009 - Direct Cache Access Information Leaf
  leaf->eax = 0;
  leaf->ebx = 0;
  leaf->ecx = 0;
  leaf->edx = 0;
}

// leaf 0x0000000A //
void bx_generic_cpuid_t::get_std_cpuid_leaf_A(cpuid_function_t *leaf) const
{
  // CPUID function 0x0000000A - Architectural Performance Monitoring Leaf
  leaf->eax = 0;
  leaf->ebx = 0;
  leaf->ecx = 0;
  leaf->edx = 0;
}

BX_CPP_INLINE static Bit32u ilog2(Bit32u x)
{
  Bit32u count = 0;
  while(x>>=1) count++;
  return count;
}

// leaf 0x0000000B //
void bx_generic_cpuid_t::get_std_cpuid_extended_topology_leaf(Bit32u subfunction, cpuid_function_t *leaf) const
{
  // CPUID function 0x0000000B - Extended Topology Leaf
  leaf->eax = 0;
  leaf->ebx = 0;
  leaf->ecx = subfunction;
  leaf->edx = cpu->get_apic_id();

#if BX_SUPPORT_SMP
  switch(subfunction) {
  case 0:
     if (nthreads > 1) {
        leaf->eax = ilog2(nthreads-1)+1;
        leaf->ebx = nthreads;
        leaf->ecx |= (1<<8);
     }
     else if (ncores > 1) {
        leaf->eax = ilog2(ncores-1)+1;
        leaf->ebx = ncores;
        leaf->ecx |= (2<<8);
     }
     else if (nprocessors > 1) {
        leaf->eax = ilog2(nprocessors-1)+1;
        leaf->ebx = nprocessors;
     }
     else {
        leaf->eax = 1;
        leaf->ebx = 1; // number of logical CPUs at this level
     }
     break;

  case 1:
     if (nthreads > 1) {
        if (ncores > 1) {
           leaf->eax = ilog2(ncores-1)+1;
           leaf->ebx = ncores;
           leaf->ecx |= (2<<8);
        }
        else if (nprocessors > 1) {
           leaf->eax = ilog2(nprocessors-1)+1;
           leaf->ebx = nprocessors;
        }
     }
     else if (ncores > 1) {
        if (nprocessors > 1) {
           leaf->eax = ilog2(nprocessors-1)+1;
           leaf->ebx = nprocessors;
        }
     }
     break;

  case 2:
     if (nthreads > 1) {
        if (nprocessors > 1) {
           leaf->eax = ilog2(nprocessors-1)+1;
           leaf->ebx = nprocessors;
        }
     }
     break;

  default:
     break;
  }
#endif
}

// leaf 0x0000000C - reserved //

// leaf 0x0000000D //
void bx_generic_cpuid_t::get_std_cpuid_xsave_leaf(Bit32u subfunction, cpuid_function_t *leaf) const
{
  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_XSAVE))
  {
    switch(subfunction) {
    case 0:
      // EAX - valid bits of XCR0 (lower part)
      // EBX - Maximum size (in bytes) required by enabled features
      // ECX - Maximum size (in bytes) required by CPU supported features
      // EDX - valid bits of XCR0 (upper part)
      leaf->eax = cpu->xcr0_suppmask;
      leaf->ebx = 512+64;
#if BX_SUPPORT_AVX
      if (cpu->xcr0.get_AVX())
        leaf->ebx += 256;
#endif
      leaf->ecx = 512+64;
#if BX_SUPPORT_AVX
      if (cpu->xcr0_suppmask & BX_XCR0_AVX_MASK)
        leaf->ecx += 256;
#endif
      leaf->edx = 0;
      return;

    case 1:
      leaf->eax = BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_XSAVEOPT);
      leaf->ebx = 0;
      leaf->ecx = 0;
      leaf->edx = 0;
      return;

#if BX_SUPPORT_AVX
    case 2: // AVX leaf
      if (cpu->xcr0_suppmask & BX_XCR0_AVX_MASK) {
        leaf->eax = 256;
        leaf->ebx = 576;
        leaf->ecx = 0;
        leaf->edx = 0;
        break;
      }
      // else fall through
#endif

    default:
      leaf->eax = 0; // reserved
      leaf->ebx = 0; // reserved
      leaf->ecx = 0; // reserved
      leaf->edx = 0; // reserved
      break;
    }
  }
}

// leaf 0x80000000 //
void bx_generic_cpuid_t::get_ext_cpuid_leaf_0(cpuid_function_t *leaf) const
{
  // EAX: highest extended function understood by CPUID
  // EBX: vendor ID string
  // EDX: vendor ID string
  // ECX: vendor ID string
  leaf->eax = BX_SUPPORT_X86_64 ? 0x80000008 : 0x80000004;
#if BX_CPU_VENDOR_INTEL
  leaf->ebx = 0;
  leaf->edx = 0;          // Reserved for Intel
  leaf->ecx = 0;
#else
  static Bit8u *vendor_string = (Bit8u *)SIM->get_param_string(BXPN_VENDOR_STRING)->getptr();

  memcpy(&(leaf->ebx), vendor_string,     4);
  memcpy(&(leaf->edx), vendor_string + 4, 4);
  memcpy(&(leaf->ecx), vendor_string + 8, 4);
#endif

#ifdef BX_BIG_ENDIAN
  leaf->ebx = bx_bswap32(leaf->ebx);
  leaf->ecx = bx_bswap32(leaf->ecx);
  leaf->edx = bx_bswap32(leaf->edx);
#endif
}

// leaf 0x80000001 //
void bx_generic_cpuid_t::get_ext_cpuid_leaf_1(cpuid_function_t *leaf) const
{
  // EAX:       CPU Version Information
  leaf->eax = BX_CPU_VENDOR_INTEL ? 0 : get_cpu_version_information();

  // EBX:       Brand ID
  leaf->ebx = 0;

  // ECX:
  leaf->ecx = get_ext2_cpuid_features();

  // EDX:
  // Many of the bits in EDX are the same as FN 0x00000001 [*] for AMD
  // [*] [0:0]   FPU on chip
  // [*] [1:1]   VME: Virtual-8086 Mode enhancements
  // [*] [2:2]   DE: Debug Extensions (I/O breakpoints)
  // [*] [3:3]   PSE: Page Size Extensions
  // [*] [4:4]   TSC: Time Stamp Counter
  // [*] [5:5]   MSR: RDMSR and WRMSR support
  // [*] [6:6]   PAE: Physical Address Extensions
  // [*] [7:7]   MCE: Machine Check Exception
  // [*] [8:8]   CXS: CMPXCHG8B instruction
  // [*] [9:9]   APIC: APIC on Chip
  //     [10:10] Reserved
  //     [11:11] SYSCALL/SYSRET support
  // [*] [12:12] MTRR: Memory Type Range Reg
  // [*] [13:13] PGE/PTE Global Bit
  // [*] [14:14] MCA: Machine Check Architecture
  // [*] [15:15] CMOV: Cond Mov/Cmp Instructions
  // [*] [16:16] PAT: Page Attribute Table
  // [*] [17:17] PSE-36: Physical Address Extensions
  //     [18:19] Reserved
  //     [20:20] No-Execute page protection
  //     [21:21] Reserved
  //     [22:22] AMD MMX Extensions
  // [*] [23:23] MMX Technology
  // [*] [24:24] FXSR: FXSAVE/FXRSTOR (also indicates CR4.OSFXSR is available)
  //     [25:25] Fast FXSAVE/FXRSTOR mode support
  //     [26:26] 1G paging support
  //     [27:27] Support RDTSCP Instruction
  //     [28:28] Reserved
  //     [29:29] Long Mode
  //     [30:30] AMD 3DNow! Extensions
  //     [31:31] AMD 3DNow! Instructions
  leaf->edx = get_std2_cpuid_features();
}

// leaf 0x80000002 //
// leaf 0x80000003 //
// leaf 0x80000004 //
void bx_generic_cpuid_t::get_ext_cpuid_brand_string_leaf(Bit32u function, cpuid_function_t *leaf) const
{
  // CPUID function 0x800000002-0x800000004 - Processor Name String Identifier
  static Bit8u *brand_string = (Bit8u *)SIM->get_param_string(BXPN_BRAND_STRING)->getptr();

  switch(function) {
  case 0x80000002:
    memcpy(&(leaf->eax), brand_string     , 4);
    memcpy(&(leaf->ebx), brand_string +  4, 4);
    memcpy(&(leaf->ecx), brand_string +  8, 4);
    memcpy(&(leaf->edx), brand_string + 12, 4);
    break;
  case 0x80000003:
    memcpy(&(leaf->eax), brand_string + 16, 4);
    memcpy(&(leaf->ebx), brand_string + 20, 4);
    memcpy(&(leaf->ecx), brand_string + 24, 4);
    memcpy(&(leaf->edx), brand_string + 28, 4);
    break;
  case 0x80000004:
    memcpy(&(leaf->eax), brand_string + 32, 4);
    memcpy(&(leaf->ebx), brand_string + 36, 4);
    memcpy(&(leaf->ecx), brand_string + 40, 4);
    memcpy(&(leaf->edx), brand_string + 44, 4);
    break;
  default:
    break;
  }

#ifdef BX_BIG_ENDIAN
  leaf->eax = bx_bswap32(leaf->eax);
  leaf->ebx = bx_bswap32(leaf->ebx);
  leaf->ecx = bx_bswap32(leaf->ecx);
  leaf->edx = bx_bswap32(leaf->edx);
#endif
}

#if BX_SUPPORT_X86_64

// leaf 0x80000005 //
void bx_generic_cpuid_t::get_ext_cpuid_leaf_5(cpuid_function_t *leaf) const
{
  // CPUID function 0x800000005 - L1 Cache and TLB Identifiers
  leaf->eax = 0x01ff01ff;
  leaf->ebx = 0x01ff01ff;
  leaf->ecx = 0x40020140;
  leaf->edx = 0x40020140;
}

// leaf 0x80000006 //
void bx_generic_cpuid_t::get_ext_cpuid_leaf_6(cpuid_function_t *leaf) const
{
  // CPUID function 0x800000006 - L2 Cache and TLB Identifiers
  leaf->eax = 0;
  leaf->ebx = 0x42004200;
  leaf->ecx = 0x02008140;
  leaf->edx = 0;
}

// leaf 0x80000007 //
void bx_generic_cpuid_t::get_ext_cpuid_leaf_7(cpuid_function_t *leaf) const
{
  // CPUID function 0x800000007 - Advanced Power Management
  leaf->eax = 0;
  leaf->ebx = 0;
  leaf->ecx = 0;
  leaf->edx = 0;
}

// leaf 0x80000008 //
void bx_generic_cpuid_t::get_ext_cpuid_leaf_8(cpuid_function_t *leaf) const
{
  // virtual & phys address size in low 2 bytes.
  leaf->eax = BX_PHY_ADDRESS_WIDTH | (BX_LIN_ADDRESS_WIDTH << 8);
  leaf->ebx = 0;
  leaf->ecx = 0; // Reserved, undefined
  leaf->edx = 0;
}

#endif

#endif

void bx_generic_cpuid_t::init_isa_extensions_bitmask(void)
{
  Bit64u features_bitmask = 0;

#if BX_SUPPORT_FPU
  features_bitmask |= BX_ISA_X87;
#endif

#if BX_CPU_LEVEL >= 4
  features_bitmask |= BX_ISA_486;

#if BX_CPU_LEVEL >= 5
  features_bitmask |= BX_ISA_PENTIUM;

  static bx_bool mmx_enabled = SIM->get_param_bool(BXPN_CPUID_MMX)->get();
  if (mmx_enabled)
    features_bitmask |= BX_ISA_MMX;

#if BX_SUPPORT_3DNOW
  features_bitmask |= BX_ISA_3DNOW;

  if (! mmx_enabled) {
    BX_PANIC(("PANIC: 3DNOW emulation requires MMX support !"));
    return;
  }
#endif

#if BX_CPU_LEVEL >= 6
  features_bitmask |= BX_ISA_P6;

#if BX_SUPPORT_MONITOR_MWAIT
  static bx_bool mwait_enabled = SIM->get_param_bool(BXPN_CPUID_MWAIT)->get();
  if (mwait_enabled)
    features_bitmask |= BX_ISA_MONITOR_MWAIT;
#endif

  static unsigned sse_enabled = SIM->get_param_enum(BXPN_CPUID_SSE)->get();
  // determine SSE in runtime
  switch (sse_enabled) {
    case BX_CPUID_SUPPORT_SSE4_2:
      features_bitmask |= BX_ISA_SSE4_2;
    case BX_CPUID_SUPPORT_SSE4_1:
      features_bitmask |= BX_ISA_SSE4_1;
    case BX_CPUID_SUPPORT_SSSE3:
      features_bitmask |= BX_ISA_SSSE3;
    case BX_CPUID_SUPPORT_SSE3:
      features_bitmask |= BX_ISA_SSE3;
    case BX_CPUID_SUPPORT_SSE2:
      features_bitmask |= BX_ISA_SSE2;
    case BX_CPUID_SUPPORT_SSE:
      features_bitmask |= BX_ISA_SSE;
    case BX_CPUID_SUPPORT_NOSSE:
    default:
      break;
  };

  if (sse_enabled) {
    if (mmx_enabled == 0 || BX_CPU_LEVEL < 6) {
      BX_PANIC(("PANIC: SSE support requires P6 emulation with MMX enabled !"));
      return;
    }
  }

  // enabled CLFLUSH only when SSE2 or higher is enabled
  if (sse_enabled >= BX_CPUID_SUPPORT_SSE2)
    features_bitmask |= BX_ISA_CLFLUSH;

  static bx_bool sse4a_enabled = SIM->get_param_bool(BXPN_CPUID_SSE4A)->get();
  if (sse4a_enabled) {
    features_bitmask |= BX_ISA_SSE4A;

    if (! sse_enabled) {
      BX_PANIC(("PANIC: SSE4A require SSE to be enabled !"));
      return;
    }
  }

  static bx_bool sep_enabled = SIM->get_param_bool(BXPN_CPUID_SEP)->get();
  if (sep_enabled)
    features_bitmask |= BX_ISA_SYSENTER_SYSEXIT;

  static bx_bool xsave_enabled = SIM->get_param_bool(BXPN_CPUID_XSAVE)->get();
  if (xsave_enabled) {
    features_bitmask |= BX_ISA_XSAVE;

    if (! sse_enabled) {
       BX_PANIC(("PANIC: XSAVE emulation requires SSE support !"));
       return;
    }
  }

  static bx_bool xsaveopt_enabled = SIM->get_param_bool(BXPN_CPUID_XSAVEOPT)->get();
  if (xsaveopt_enabled) {
    features_bitmask |= BX_ISA_XSAVEOPT;

    if (! xsave_enabled) {
      BX_PANIC(("PANIC: XSAVEOPT emulation requires XSAVE !"));
      return;
    }
  }

  static bx_bool aes_enabled = SIM->get_param_bool(BXPN_CPUID_AES)->get();
  if (aes_enabled) {
    features_bitmask |= BX_ISA_AES_PCLMULQDQ;

    // AES required 3-byte opcode (SSS3E support or more)
    if (sse_enabled < BX_CPUID_SUPPORT_SSSE3) {
      BX_PANIC(("PANIC: AES support requires SSSE3 or higher !"));
      return;
    }
  }

  static bx_bool movbe_enabled = SIM->get_param_bool(BXPN_CPUID_MOVBE)->get();
  if (movbe_enabled) {
    features_bitmask |= BX_ISA_MOVBE;

    // MOVBE required 3-byte opcode (SSS3E support or more)
    if (sse_enabled < BX_CPUID_SUPPORT_SSSE3) {
      BX_PANIC(("PANIC: MOVBE support requires SSSE3 or higher !"));
      return;
    }
  }

#if BX_SUPPORT_X86_64
  static bx_bool x86_64_enabled = SIM->get_param_bool(BXPN_CPUID_X86_64)->get();
  if (x86_64_enabled) {
    features_bitmask |= BX_ISA_CMPXCHG16B | BX_ISA_RDTSCP | BX_ISA_LM_LAHF_SAHF;

    if (sse_enabled < BX_CPUID_SUPPORT_SSE2) {
      BX_PANIC(("PANIC: x86-64 emulation requires SSE2 support !"));
      return;
    }

    if (! sep_enabled) {
      BX_PANIC(("PANIC: x86-64 emulation requires SYSENTER/SYSEXIT support !"));
      return;
    }

    static bx_bool fsgsbase_enabled = SIM->get_param_bool(BXPN_CPUID_FSGSBASE)->get();
    if (fsgsbase_enabled)
      features_bitmask |= BX_ISA_FSGSBASE;

    static unsigned apic_enabled = SIM->get_param_enum(BXPN_CPUID_APIC)->get();
    if (apic_enabled < BX_CPUID_SUPPORT_XAPIC) {
      BX_PANIC(("PANIC: x86-64 emulation requires XAPIC support !"));
      return;
    }
  }
  else {
    if (BX_SUPPORT_VMX >= 2) {
      BX_PANIC(("PANIC: VMXx2 emulation requires x86-64 support !"));
      return;
    }
  }

#if BX_SUPPORT_AVX
  static unsigned avx_enabled = SIM->get_param_num(BXPN_CPUID_AVX)->get();
  if (avx_enabled) {
    features_bitmask |= BX_ISA_AVX;

    if (! xsave_enabled) {
      BX_PANIC(("PANIC: AVX emulation requires XSAVE support !"));
      return;
    }

    if (! x86_64_enabled) {
      BX_PANIC(("PANIC: AVX emulation requires x86-64 support !"));
      return;
    }

    if (avx_enabled >= 2)
      features_bitmask |= BX_ISA_AVX2;
  }

  static bx_bool avx_f16c_enabled = SIM->get_param_bool(BXPN_CPUID_AVX_F16CVT)->get();
  if (avx_f16c_enabled) {
    if (! avx_enabled) {
      BX_PANIC(("PANIC: Float16 convert emulation requires AVX support !"));
      return;
    }

    features_bitmask |= BX_ISA_AVX_F16C;
  }

  static bx_bool avx_fma_enabled = SIM->get_param_bool(BXPN_CPUID_AVX_FMA)->get();
  if (avx_fma_enabled) {
    if (avx_enabled < 2) {
      BX_PANIC(("PANIC: FMA emulation requires AVX2 support !"));
      return;
    }

    features_bitmask |= BX_ISA_AVX_FMA;
  }

  static unsigned bmi_enabled = SIM->get_param_num(BXPN_CPUID_BMI)->get();
  if (bmi_enabled) {
    features_bitmask |= BX_ISA_BMI1 | BX_ISA_LZCNT;

    if (! avx_enabled) {
      BX_PANIC(("PANIC: Bit Manipulation Instructions (BMI) emulation requires AVX support !"));
      return;
    }

    if (bmi_enabled >= 2)
      features_bitmask |= BX_ISA_BMI2;
  }

  static bx_bool fma4_enabled = SIM->get_param_bool(BXPN_CPUID_FMA4)->get();
  if (fma4_enabled) {
    if (! avx_enabled) {
      BX_PANIC(("PANIC: FMA4 emulation requires AVX support !"));
      return;
    }

    features_bitmask |= BX_ISA_FMA4;
  }

  static bx_bool xop_enabled = SIM->get_param_bool(BXPN_CPUID_XOP)->get();
  if (xop_enabled) {
    if (! avx_enabled) {
      BX_PANIC(("PANIC: XOP emulation requires AVX support !"));
      return;
    }

    features_bitmask |= BX_ISA_XOP;
  }

  static bx_bool tbm_enabled = SIM->get_param_bool(BXPN_CPUID_TBM)->get();
  if (tbm_enabled) {
    if (! avx_enabled || ! xop_enabled) {
      BX_PANIC(("PANIC: TBM emulation requires AVX and XOP support !"));
      return;
    }

    features_bitmask |= BX_ISA_TBM;
  }
#endif // BX_SUPPORT_AVX

#endif // BX_SUPPORT_X86_64

#if BX_SUPPORT_VMX
  static unsigned vmx_enabled = SIM->get_param_num(BXPN_CPUID_VMX)->get();
  if (vmx_enabled) {
    features_bitmask |= BX_ISA_VMX;

    if (! sep_enabled) {
      BX_PANIC(("PANIC: VMX emulation requires SYSENTER/SYSEXIT support !"));
      return;
    }
  }
#endif

#endif // CPU_LEVEL >= 6

#endif // CPU_LEVEL >= 5

#endif // CPU_LEVEL >= 4

  this->isa_extensions_bitmask = features_bitmask;
}

void bx_generic_cpuid_t::init_cpu_extensions_bitmask(void)
{
  Bit32u features_bitmask = 0;

#if BX_SUPPORT_APIC
  static unsigned apic_enabled = SIM->get_param_enum(BXPN_CPUID_APIC)->get();
  // determine SSE in runtime
  switch (apic_enabled) {
    case BX_CPUID_SUPPORT_X2APIC:
      features_bitmask |= BX_CPU_X2APIC;
    case BX_CPUID_SUPPORT_XAPIC:
      features_bitmask |= BX_CPU_XAPIC;
    case BX_CPUID_SUPPORT_LEGACY_APIC:
    default:
      break;
  };

  // I would like to allow XAPIC configuration with i586 together
  if (apic_enabled >= BX_CPUID_SUPPORT_X2APIC && BX_CPU_LEVEL < 6) {
    BX_PANIC(("PANIC: X2APIC require CPU_LEVEL >= 6 !"));
    return;
  }
#endif

#if BX_CPU_LEVEL >= 5
  features_bitmask |= BX_CPU_VME;
  features_bitmask |= BX_CPU_DEBUG_EXTENSIONS;
  features_bitmask |= BX_CPU_PSE;

#if BX_CPU_LEVEL >= 6
  features_bitmask |= BX_CPU_PAE;
  features_bitmask |= BX_CPU_PGE;
#if BX_PHY_ADDRESS_LONG
  features_bitmask |= BX_CPU_PSE36;
#endif
  features_bitmask |= BX_CPU_MTRR;
  features_bitmask |= BX_CPU_PAT;

  static bx_bool smep_enabled = SIM->get_param_bool(BXPN_CPUID_SMEP)->get();
  if (smep_enabled)
    features_bitmask |= BX_CPU_SMEP;

#if BX_SUPPORT_X86_64
  static bx_bool x86_64_enabled = SIM->get_param_bool(BXPN_CPUID_X86_64)->get();
  if (x86_64_enabled) {
    features_bitmask |= BX_CPU_LONG_MODE | BX_CPU_FFXSR | BX_CPU_NX;

    static bx_bool pcid_enabled = SIM->get_param_bool(BXPN_CPUID_PCID)->get();
    if (pcid_enabled)
      features_bitmask |= BX_CPU_PCID;

    static bx_bool xlarge_pages = SIM->get_param_bool(BXPN_CPUID_1G_PAGES)->get();
    if (xlarge_pages)
      features_bitmask |= BX_CPU_1G_PAGES;
  }
#endif

#endif // CPU_LEVEL >= 6

#endif // CPU_LEVEL >= 5

  this->cpu_extensions_bitmask = features_bitmask;
}

#if BX_SUPPORT_VMX
void bx_generic_cpuid_t::init_vmx_extensions_bitmask(void)
{
  Bit32u features_bitmask = 0;

  static unsigned vmx_enabled = SIM->get_param_num(BXPN_CPUID_VMX)->get();
  if (vmx_enabled) {
    features_bitmask |= BX_VMX_VIRTUAL_NMI;

    static bx_bool x86_64_enabled = SIM->get_param_bool(BXPN_CPUID_X86_64)->get();
    if (x86_64_enabled) {
      features_bitmask |= BX_VMX_TPR_SHADOW |
                          BX_VMX_APIC_VIRTUALIZATION |
                          BX_VMX_WBINVD_VMEXIT;

#if BX_SUPPORT_VMX >= 2
      if (vmx_enabled >= 2) {
        features_bitmask |= BX_VMX_PREEMPTION_TIMER |
                            BX_VMX_PAT |
                            BX_VMX_EFER |
                            BX_VMX_EPT |
                            BX_VMX_VPID |
                            BX_VMX_UNRESTRICTED_GUEST |
                            BX_VMX_DESCRIPTOR_TABLE_EXIT |
                            BX_VMX_X2APIC_VIRTUALIZATION |
                            BX_VMX_PAUSE_LOOP_EXITING;

        features_bitmask |= BX_VMX_SAVE_DEBUGCTL_DISABLE |
                         /* BX_VMX_MONITOR_TRAP_FLAG | */ // not implemented yet
                            BX_VMX_PERF_GLOBAL_CTRL;
      }
#endif
    }
  }
  
  this->vmx_extensions_bitmask = features_bitmask;
}
#endif

/*
 * Get CPU version information:
 *
 * [3:0]   Stepping ID
 * [7:4]   Model: starts at 1
 * [11:8]  Family: 4=486, 5=Pentium, 6=PPro, ...
 * [13:12] Type: 0=OEM, 1=overdrive, 2=dual cpu, 3=reserved
 * [19:16] Extended Model
 * [27:29] Extended Family
 */

Bit32u bx_generic_cpuid_t::get_cpu_version_information(void) const
{
  static Bit32u stepping = SIM->get_param_num(BXPN_CPUID_STEPPING)->get();
  static Bit32u model = SIM->get_param_num(BXPN_CPUID_MODEL)->get();
  static Bit32u family = SIM->get_param_num(BXPN_CPUID_FAMILY)->get();

  if (family < 6 && family != BX_CPU_LEVEL)
    BX_PANIC(("PANIC: CPUID family %x not matching configured cpu level %d", family, BX_CPU_LEVEL));

  return ((family & 0xfff0) << 16) |
         ((model & 0xf0) << 12) |
         ((family & 0x0f) << 8) |
         ((model & 0x0f) << 4) | stepping;
}

#if BX_CPU_LEVEL >= 6

/* Get CPU extended feature flags. */
Bit32u bx_generic_cpuid_t::get_extended_cpuid_features(void) const
{
  // [0:0]   SSE3: SSE3 Instructions
  // [1:1]   PCLMULQDQ Instruction support
  // [2:2]   DTES64: 64-bit DS area
  // [3:3]   MONITOR/MWAIT support
  // [4:4]   DS-CPL: CPL qualified debug store
  // [5:5]   VMX: Virtual Machine Technology
  // [6:6]   SMX: Secure Virtual Machine Technology
  // [7:7]   EST: Enhanced Intel SpeedStep Technology
  // [8:8]   TM2: Thermal Monitor 2
  // [9:9]   SSSE3: SSSE3 Instructions
  // [10:10] CNXT-ID: L1 context ID
  // [11:11] reserved
  // [12:12] FMA Instructions support
  // [13:13] CMPXCHG16B: CMPXCHG16B instruction support
  // [14:14] xTPR update control
  // [15:15] PDCM - Perfon and Debug Capability MSR
  // [16:16] reserved
  // [17:17] PCID: Process Context Identifiers
  // [18:18] DCA - Direct Cache Access
  // [19:19] SSE4.1 Instructions
  // [20:20] SSE4.2 Instructions
  // [21:21] X2APIC
  // [22:22] MOVBE instruction
  // [23:23] POPCNT instruction
  // [24:24] TSC Deadline
  // [25:25] AES Instructions
  // [26:26] XSAVE extensions support
  // [27:27] OSXSAVE support
  // [28:28] AVX extensions support
  // [29:29] AVX F16C - Float16 conversion support
  // [30:30] RDRAND instruction
  // [31:31] reserved

  Bit32u features = 0;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_SSE3))
    features |= BX_CPUID_EXT_SSE3;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_AES_PCLMULQDQ))
    features |= BX_CPUID_EXT_PCLMULQDQ;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_MONITOR_MWAIT))
    features |= BX_CPUID_EXT_MONITOR_MWAIT;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_VMX))
    features |= BX_CPUID_EXT_VMX;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_SSSE3))
    features |= BX_CPUID_EXT_SSSE3;

#if BX_SUPPORT_X86_64
  if (BX_CPUID_SUPPORT_CPU_EXTENSION(BX_CPU_LONG_MODE))
    features |= BX_CPUID_EXT_CMPXCHG16B;

  if (BX_CPUID_SUPPORT_CPU_EXTENSION(BX_CPU_PCID))
    features |= BX_CPUID_EXT_PCID;
#endif

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_SSE4_1))
    features |= BX_CPUID_EXT_SSE4_1;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_SSE4_2))
    features |= BX_CPUID_EXT_SSE4_2;

  if (BX_CPUID_SUPPORT_CPU_EXTENSION(BX_CPU_X2APIC))
    features |= BX_CPUID_EXT_X2APIC;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_MOVBE))
    features |= BX_CPUID_EXT_MOVBE;

  // enable POPCNT if SSE4_2 is enabled
  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_SSE4_2))
    features |= BX_CPUID_EXT_POPCNT;

  // support for AES
  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_AES_PCLMULQDQ))
    features |= BX_CPUID_EXT_AES;

  // support XSAVE extensions
  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_XSAVE)) {
    features |= BX_CPUID_EXT_XSAVE;
    if (cpu->cr4.get_OSXSAVE())
      features |= BX_CPUID_EXT_OSXSAVE;
  }

#if BX_SUPPORT_AVX
  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_AVX))
    features |= BX_CPUID_EXT_AVX;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_AVX_F16C))
    features |= BX_CPUID_EXT_AVX_F16C;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_AVX_FMA))
    features |= BX_CPUID_EXT_FMA;
#endif

  return features;
}

#endif

/* Get CPU feature flags. Returned by CPUID functions 1 and 80000001.  */
Bit32u bx_generic_cpuid_t::get_std_cpuid_features(void) const
{
  //   [0:0]   FPU on chip
  //   [1:1]   VME: Virtual-8086 Mode enhancements
  //   [2:2]   DE: Debug Extensions (I/O breakpoints)
  //   [3:3]   PSE: Page Size Extensions
  //   [4:4]   TSC: Time Stamp Counter
  //   [5:5]   MSR: RDMSR and WRMSR support
  //   [6:6]   PAE: Physical Address Extensions
  //   [7:7]   MCE: Machine Check Exception
  //   [8:8]   CXS: CMPXCHG8B instruction
  //   [9:9]   APIC: APIC on Chip
  //   [10:10] Reserved
  //   [11:11] SYSENTER/SYSEXIT support
  //   [12:12] MTRR: Memory Type Range Reg
  //   [13:13] PGE/PTE Global Bit
  //   [14:14] MCA: Machine Check Architecture
  //   [15:15] CMOV: Cond Mov/Cmp Instructions
  //   [16:16] PAT: Page Attribute Table
  //   [17:17] PSE-36: Physical Address Extensions
  //   [18:18] PSN: Processor Serial Number
  //   [19:19] CLFLUSH: CLFLUSH Instruction support
  //   [20:20] Reserved
  //   [21:21] DS: Debug Store
  //   [22:22] ACPI: Thermal Monitor and Software Controlled Clock Facilities
  //   [23:23] MMX Technology
  //   [24:24] FXSR: FXSAVE/FXRSTOR (also indicates CR4.OSFXSR is available)
  //   [25:25] SSE: SSE Extensions
  //   [26:26] SSE2: SSE2 Extensions
  //   [27:27] Self Snoop
  //   [28:28] Hyper Threading Technology
  //   [29:29] TM: Thermal Monitor
  //   [30:30] Reserved
  //   [31:31] PBE: Pending Break Enable

  Bit32u features = 0;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_X87))
    features |= BX_CPUID_STD_X87;

#if BX_CPU_LEVEL >= 5
  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_PENTIUM)) {
    // Pentium only features
    features |= BX_CPUID_STD_TSC;
    features |= BX_CPUID_STD_MSR;
    // support Machine Check
    features |= BX_CPUID_STD_MCE | BX_CPUID_STD_MCA;
    features |= BX_CPUID_STD_CMPXCHG8B;
  }

  if (BX_CPUID_SUPPORT_CPU_EXTENSION(BX_CPU_VME))
    features |= BX_CPUID_STD_VME;

  if (BX_CPUID_SUPPORT_CPU_EXTENSION(BX_CPU_DEBUG_EXTENSIONS))
    features |= BX_CPUID_STD_DEBUG_EXTENSIONS;

  if (BX_CPUID_SUPPORT_CPU_EXTENSION(BX_CPU_PSE))
    features |= BX_CPUID_STD_PSE;
#endif

#if BX_SUPPORT_APIC
  // if MSR_APICBASE APIC Global Enable bit has been cleared,
  // the CPUID feature flag for the APIC is set to 0.
  if (cpu->msr.apicbase & 0x800)
    features |= BX_CPUID_STD_APIC; // APIC on chip
#endif

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_SYSENTER_SYSEXIT))
    features |= BX_CPUID_STD_SYSENTER_SYSEXIT;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_CLFLUSH))
    features |= BX_CPUID_STD_CLFLUSH;

#if BX_CPU_LEVEL >= 5
  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_MMX))
    features |= BX_CPUID_STD_MMX;
#endif

#if BX_CPU_LEVEL >= 6
  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_P6)) {
    features |= BX_CPUID_STD_CMOV;
    features |= BX_CPUID_STD_ACPI;
  }

  if (BX_CPUID_SUPPORT_CPU_EXTENSION(BX_CPU_MTRR))
    features |= BX_CPUID_STD_MTRR;

  if (BX_CPUID_SUPPORT_CPU_EXTENSION(BX_CPU_PAT))
    features |= BX_CPUID_STD_PAT;

  if (BX_CPUID_SUPPORT_CPU_EXTENSION(BX_CPU_PAE))
    features |= BX_CPUID_STD_PAE;

  if (BX_CPUID_SUPPORT_CPU_EXTENSION(BX_CPU_PGE))
    features |= BX_CPUID_STD_GLOBAL_PAGES;

  if (BX_CPUID_SUPPORT_CPU_EXTENSION(BX_CPU_PSE36))
    features |= BX_CPUID_STD_PSE36;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_SSE))
    features |= BX_CPUID_STD_FXSAVE_FXRSTOR | BX_CPUID_STD_SSE;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_SSE2))
    features |= BX_CPUID_STD_SSE2;

  if (BX_CPU_VENDOR_INTEL)
    features |= BX_CPUID_STD_SELF_SNOOP;
#endif

#if BX_SUPPORT_SMP
  // Intel(R) HyperThreading Technology
  if (SIM->get_param_num(BXPN_CPU_NTHREADS)->get() > 1)
    features |= BX_CPUID_STD_HT;
#endif

  return features;
}

#if BX_CPU_LEVEL >= 6

/* Get CPU feature flags. Returned by CPUID function 80000001 in EDX register */
Bit32u bx_generic_cpuid_t::get_std2_cpuid_features(void) const
{
  // Many of the bits in EDX are the same as EAX [*] for AMD
  // [*] [0:0]   FPU on chip
  // [*] [1:1]   VME: Virtual-8086 Mode enhancements
  // [*] [2:2]   DE: Debug Extensions (I/O breakpoints)
  // [*] [3:3]   PSE: Page Size Extensions
  // [*] [4:4]   TSC: Time Stamp Counter
  // [*] [5:5]   MSR: RDMSR and WRMSR support
  // [*] [6:6]   PAE: Physical Address Extensions
  // [*] [7:7]   MCE: Machine Check Exception
  // [*] [8:8]   CXS: CMPXCHG8B instruction
  // [*] [9:9]   APIC: APIC on Chip
  //     [10:10] Reserved
  //     [11:11] SYSCALL/SYSRET support
  // [*] [12:12] MTRR: Memory Type Range Reg
  // [*] [13:13] PGE/PTE Global Bit
  // [*] [14:14] MCA: Machine Check Architecture
  // [*] [15:15] CMOV: Cond Mov/Cmp Instructions
  // [*] [16:16] PAT: Page Attribute Table
  // [*] [17:17] PSE-36: Physical Address Extensions
  //     [18:19] Reserved
  //     [20:20] No-Execute page protection
  //     [21:21] Reserved
  //     [22:22] AMD MMX Extensions
  // [*] [23:23] MMX Technology
  // [*] [24:24] FXSR: FXSAVE/FXRSTOR (also indicates CR4.OSFXSR is available)
  //     [25:25] Fast FXSAVE/FXRSTOR mode support
  //     [26:26] 1G paging support
  //     [27:27] Support RDTSCP Instruction
  //     [28:28] Reserved
  //     [29:29] Long Mode
  //     [30:30] AMD 3DNow! Extensions
  //     [31:31] AMD 3DNow! Instructions
  Bit32u features = BX_CPU_VENDOR_INTEL ? 0 : get_std_cpuid_features();
  features &= 0x0183F3FF;
#if BX_SUPPORT_3DNOW
  // only AMD is interesting in AMD MMX extensions
  features |= BX_CPUID_STD2_AMD_MMX_EXT | BX_CPUID_STD2_3DNOW_EXT | BX_CPUID_STD2_3DNOW;
#endif
#if BX_SUPPORT_X86_64
  if (BX_CPUID_SUPPORT_CPU_EXTENSION(BX_CPU_LONG_MODE)) {
    features |= BX_CPUID_STD2_LONG_MODE;

    if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_RDTSCP))
      features |= BX_CPUID_STD2_RDTSCP;
    if (BX_CPUID_SUPPORT_CPU_EXTENSION(BX_CPU_NX))
      features |= BX_CPUID_STD2_NX;
    if (BX_CPUID_SUPPORT_CPU_EXTENSION(BX_CPU_FFXSR))
      features |= BX_CPUID_STD2_FFXSR;
    if (BX_CPUID_SUPPORT_CPU_EXTENSION(BX_CPU_1G_PAGES))
      features |= BX_CPUID_STD2_1G_PAGES;

    if (cpu->long64_mode())
      features |= BX_CPUID_STD2_SYSCALL_SYSRET;
  }
#endif

  return features;
}

/* Get CPU feature flags. Returned by CPUID function 80000001 in ECX register */
Bit32u bx_generic_cpuid_t::get_ext2_cpuid_features(void) const
{
  // ECX:
  //   [0:0]   LAHF/SAHF instructions support in 64-bit mode
  //   [1:1]   CMP_Legacy: Core multi-processing legacy mode (AMD)
  //   [2:2]   SVM: Secure Virtual Machine (AMD)
  //   [3:3]   Extended APIC Space
  //   [4:4]   AltMovCR8: LOCK MOV CR0 means MOV CR8
  //   [5:5]   LZCNT: LZCNT instruction support
  //   [6:6]   SSE4A: SSE4A Instructions support (deprecated?)
  //   [7:7]   Misaligned SSE support
  //   [8:8]   PREFETCHW: PREFETCHW instruction support
  //   [9:9]   OSVW: OS visible workarounds (AMD)
  //   [10:10] IBS: Instruction based sampling
  //   [11:11] XOP: Extended Operations Support and XOP Prefix
  //   [12:12] SKINIT support
  //   [13:13] WDT: Watchdog timer support
  //   [14:14] reserved
  //   [15:15] LWP: Light weight profiling
  //   [16:16] FMA4: Four-operand FMA instructions support
  //   [18:17] reserved
  //   [19:19] NodeId: Indicates support for NodeId MSR (0xc001100c)
  //   [20:20] reserved
  //   [21:21] TBM: trailing bit manipulation instructions support
  //   [22:22] Topology extensions support
  //   [31:23] reserved

  Bit32u features = 0;

#if BX_SUPPORT_X86_64
  if (BX_CPUID_SUPPORT_CPU_EXTENSION(BX_CPU_LONG_MODE))
    features |= BX_CPUID_EXT2_LAHF_SAHF | BX_CPUID_EXT2_PREFETCHW;
#endif

#if BX_SUPPORT_MISALIGNED_SSE
  features |= BX_CPUID_EXT2_MISALIGNED_SSE;
#endif

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_LZCNT))
    features |= BX_CPUID_EXT2_LZCNT;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_SSE4A))
    features |= BX_CPUID_EXT2_SSE4A;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_XOP))
    features |= BX_CPUID_EXT2_XOP;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_FMA4))
    features |= BX_CPUID_EXT2_FMA4;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_TBM))
    features |= BX_CPUID_EXT2_TBM;

  return features;
}

Bit32u bx_generic_cpuid_t::get_ext3_cpuid_features(void) const
{
  Bit32u features = 0;

  //   [0:0]    FS/GS BASE access instructions
  //   [2:1]    reserved
  //   [3:3]    BMI1: Advanced Bit Manipulation Extensions
  //   [4:4]    reserved
  //   [5:5]    AVX2
  //   [6:6]    reserved
  //   [7:7]    SMEP: Supervisor Mode Execution Protection
  //   [8:8]    BMI2: Advanced Bit Manipulation Extensions
  //   [9:9]    Support for Enhanced REP MOVSB/STOSB
  //   [10:10]  Support for INVPCID instruction
  //   [31:10]  reserved
  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_FSGSBASE))
    features |= BX_CPUID_EXT3_FSGSBASE;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_BMI1))
    features |= BX_CPUID_EXT3_BMI1;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_AVX2))
    features |= BX_CPUID_EXT3_AVX2;

  if (BX_CPUID_SUPPORT_CPU_EXTENSION(BX_CPU_SMEP))
    features |= BX_CPUID_EXT3_SMEP;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_BMI2))
    features |= BX_CPUID_EXT3_BMI2;

  if (BX_CPUID_SUPPORT_ISA_EXTENSION(BX_ISA_INVPCID))
    features |= BX_CPUID_EXT3_INVPCID;

  return features;
}

#endif

void bx_generic_cpuid_t::dump_cpuid(void) const
{
  struct cpuid_function_t leaf;

  get_cpuid_leaf(0x00000000, 0x00000000, &leaf);
  BX_INFO(("CPUID[0x00000000]: %08x %08x %08x %08x", leaf.eax, leaf.ebx, leaf.ecx, leaf.edx));
  Bit32u max_std_function = leaf.eax, n;

  if (max_std_function > 0) {
    for (n=1; n<=max_std_function;n++) {
      get_cpuid_leaf(n, 0x00000000, &leaf);
      BX_INFO(("CPUID[0x%08x]: %08x %08x %08x %08x", n, leaf.eax, leaf.ebx, leaf.ecx, leaf.edx));
    }
  }

#if BX_CPU_LEVEL >= 6
  get_cpuid_leaf(0x80000000, 0x00000000, &leaf);
  BX_INFO(("CPUID[0x80000000]: %08x %08x %08x %08x", leaf.eax, leaf.ebx, leaf.ecx, leaf.edx));
  Bit32u max_ext_function = leaf.eax;

  if (max_ext_function > 0) {
    for (n=0x80000001; n<=max_ext_function;n++) {
      get_cpuid_leaf(n, 0x00000000, &leaf);
      BX_INFO(("CPUID[0x%08x]: %08x %08x %08x %08x", n, leaf.eax, leaf.ebx, leaf.ecx, leaf.edx));
    }
  }
#endif
}

bx_cpuid_t *create_bx_generic_cpuid(BX_CPU_C *cpu) { return new bx_generic_cpuid_t(cpu); }

#endif
