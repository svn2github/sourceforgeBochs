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
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

//
// bochs.h is the master header file for all C++ code.  It includes all 
// the system header files needed by bochs, and also includes all the bochs
// C++ header files.  Because bochs.h and the files that it includes has 
// structure and class definitions, it cannot be called from C code.
// 

#ifndef BX_BOCHS_H
#  define BX_BOCHS_H 1

extern "C" {
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#ifndef WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif
#include <time.h>
#ifdef macintosh
#  include <types.h>
#  include <stat.h>
#  include <utime.h>
#else
#  ifndef WIN32
#    include <sys/time.h>
#  endif
#  include <sys/types.h>
#  include <sys/stat.h>
#endif
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#ifdef macintosh
#  include "macutils.h"
#  define SuperDrive "[fd:]"
#endif
}

#include "config.h"      /* generated by configure script from config.h.in */
#include "osdep.h"       /* platform dependent includes and defines */ 
#include "debug/debug.h"
#include "bxversion.h"

#include "gui/siminterface.h"

//
// some macros to interface the CPU and memory to external environment
// so that these functions can be redirected to the debugger when
// needed.
//

#if ((BX_DEBUGGER == 1) && (BX_NUM_SIMULATORS >= 2))
// =-=-=-=-=-=-=- Redirected to cosimulation debugger -=-=-=-=-=-=-=
#define BX_VGA_MEM_READ(addr)       bx_dbg_ucmem_read(addr)
#define BX_VGA_MEM_WRITE(addr, val) bx_dbg_ucmem_write(addr, val)
#if BX_SUPPORT_A20
#  define A20ADDR(x)               ( (x) & bx_pc_system.a20_mask )
#else
#  define A20ADDR(x)                (x)
#endif
#define BX_INP(addr, len)           bx_dbg_inp(addr, len)
#define BX_OUTP(addr, val, len)     bx_dbg_outp(addr, val, len)
#define BX_HRQ                      (bx_pc_system.HRQ)
#define BX_RAISE_HLDA()             bx_dbg_raise_HLDA()
#define BX_TICK1()
#define BX_INTR                     bx_pc_system.INTR
#define BX_SET_INTR(b)              bx_dbg_set_INTR(b)
#if BX_SIM_ID == 0
#  define BX_CPU_C                  bx_cpu0_c
#  define BX_CPU                    bx_cpu0
#  define BX_MEM_C                  bx_mem0_c
#  define BX_MEM                    bx_mem0
#else
#  define BX_CPU_C                  bx_cpu1_c
#  define BX_CPU                    bx_cpu1
#  define BX_MEM_C                  bx_mem1_c
#  define BX_MEM                    bx_mem1
#endif
#define BX_SET_ENABLE_A20(enabled)  bx_dbg_async_pin_request(BX_DBG_ASYNC_PENDING_A20, \
                                      enabled)
#define BX_GET_ENABLE_A20()         bx_pc_system.get_enable_a20()
#error FIXME: cosim mode not fixed yet

#else

// =-=-=-=-=-=-=- Normal optimized use -=-=-=-=-=-=-=-=-=-=-=-=-=-=
#define BX_VGA_MEM_READ(addr) (bx_devices.vga->mem_read(addr))
#define BX_VGA_MEM_WRITE(addr, val) bx_devices.vga->mem_write(addr, val)
#if BX_SUPPORT_A20
#  define A20ADDR(x)               ( (x) & bx_pc_system.a20_mask )
#else
#  define A20ADDR(x)               (x)
#endif
//
// some pc_systems functions just redirect to the IO devices so optimize
// by eliminating call here
//
// #define BX_INP(addr, len)        bx_pc_system.inp(addr, len)
// #define BX_OUTP(addr, val, len)  bx_pc_system.outp(addr, val, len)
#define BX_INP(addr, len)           bx_devices.inp(addr, len)
#define BX_OUTP(addr, val, len)     bx_devices.outp(addr, val, len)
#define BX_HRQ                      (bx_pc_system.HRQ)
#define BX_RAISE_HLDA()             bx_pc_system.raise_HLDA()
#define BX_TICK1()                  bx_pc_system.tick1()
#define BX_TICKN(n)                 bx_pc_system.tickn(n)
#define BX_INTR                     bx_pc_system.INTR
#define BX_SET_INTR(b)              bx_pc_system.set_INTR(b)
#define BX_CPU_C                    bx_cpu_c
#define BX_MEM_C                    bx_mem_c
#if BX_SMP_PROCESSORS==1
#define BX_CPU(x)                   (&bx_cpu)
#define BX_MEM(x)                   (&bx_mem)
#else
#define BX_CPU(x)                   (bx_cpu_array[x])
#define BX_MEM(x)                   (bx_mem_array[x])
#endif

#define BX_SET_ENABLE_A20(enabled)  bx_pc_system.set_enable_a20(enabled)
#define BX_GET_ENABLE_A20()         bx_pc_system.get_enable_a20()

#endif

// you can't use static member functions on the CPU, if there are going
// to be 2 cpus.  Check this early on.
#if (BX_SMP_PROCESSORS>1)
#  if (BX_USE_CPU_SMF!=0)
#    error For SMP simulation, BX_USE_CPU_SMF must be 0.
#  endif
#endif


// #define BX_IAC()                 bx_pc_system.IAC()
#define BX_IAC()                    bx_devices.pic->IAC()
//#define BX_IAC()                    bx_dbg_IAC()

//
// Ways for the the external environment to report back information
// to the debugger.
//

#if BX_DEBUGGER
#  define BX_DBG_ASYNC_INTR bx_guard.async.irq
#  define BX_DBG_ASYNC_DMA  bx_guard.async.dma
#if (BX_NUM_SIMULATORS > 1)
// for multiple simulators, we always need this info, since we're
// going to replay it.
#  define BX_DBG_DMA_REPORT(addr, len, what, val) \
        bx_dbg_dma_report(addr, len, what, val)
#  define BX_DBG_IAC_REPORT(vector, irq) \
        bx_dbg_iac_report(vector, irq)
#  define BX_DBG_A20_REPORT(val) \
        bx_dbg_a20_report(val)
#  define BX_DBG_IO_REPORT(addr, size, op, val) \
        bx_dbg_io_report(addr, size, op, val)
#  define BX_DBG_UCMEM_REPORT(addr, size, op, val)
#else
// for a single simulator debug environment, we can optimize a little
// by conditionally calling, as per requested.

#  define BX_DBG_DMA_REPORT(addr, len, what, val) \
        if (bx_guard.report.dma) bx_dbg_dma_report(addr, len, what, val)
#  define BX_DBG_IAC_REPORT(vector, irq) \
        if (bx_guard.report.irq) bx_dbg_iac_report(vector, irq)
#  define BX_DBG_A20_REPORT(val) \
        if (bx_guard.report.a20) bx_dbg_a20_report(val)
#  define BX_DBG_IO_REPORT(addr, size, op, val) \
        if (bx_guard.report.io) bx_dbg_io_report(addr, size, op, val)
#  define BX_DBG_UCMEM_REPORT(addr, size, op, val) \
        if (bx_guard.report.ucmem) bx_dbg_ucmem_report(addr, size, op, val)
#endif  // #if (BX_NUM_SIMULATORS > 1)

#else  // #if BX_DEBUGGER
// debugger not compiled in, use empty stubs
#  define BX_DBG_ASYNC_INTR 1
#  define BX_DBG_ASYNC_DMA  1
#  define BX_DBG_DMA_REPORT(addr, len, what, val)
#  define BX_DBG_IAC_REPORT(vector, irq)
#  define BX_DBG_A20_REPORT(val)
#  define BX_DBG_IO_REPORT(addr, size, op, val)
#  define BX_DBG_UCMEM_REPORT(addr, size, op, val)
#endif  // #if BX_DEBUGGER

extern Bit8u DTPageDirty[];
#if BX_DYNAMIC_TRANSLATION
#  define BX_DYN_DIRTY_PAGE(page) DTPageDirty[page] = 1;
#else
#  define BX_DYN_DIRTY_PAGE(page)
#endif

#define MAGIC_LOGNUM 0x12345678

// Log Level defines
#define LOGLEV_DEBUG 0
#define LOGLEV_INFO  1
#define LOGLEV_ERROR 2
#define LOGLEV_PANIC 3
#define N_LOGLEV   4

typedef class logfunctions {
	char *prefix;
	int type;
// values of onoff: 0=ignore, 1=report, 2=fatal
#define ACT_IGNORE 0
#define ACT_REPORT 1
#if BX_USE_CONTROL_PANEL
#  define ACT_ASK    2
#else
   // if control panel disabled, then map all ACT_ASK into ACT_FATAL.
#  define ACT_ASK    ACT_FATAL
#endif
#define ACT_FATAL  3
#define N_ACT      4
	int onoff[N_LOGLEV];
	class iofunctions *logio;
public:
	logfunctions(void);
	logfunctions(class iofunctions *);
	~logfunctions(void);

	void info(char *fmt, ...);
	void error(char *fmt, ...);
	void panic(char *fmt, ...);
	void ldebug(char *fmt, ...);
	void fatal (char *prefix, char *fmt, va_list ap);
	void ask (int level, char *prefix, char *fmt, va_list ap);
	void put(char *);
	void settype(int);
	void setio(class iofunctions *);
	void setonoff(int loglev, int value) {
	  assert (loglev >= 0 && loglev < N_LOGLEV);
	  onoff[loglev] = value; 
	}
	char *getprefix () { return prefix; }
	int getonoff(int level) {
	  assert (level>=0 && level<N_LOGLEV);
	  return onoff[level]; 
        }
} logfunc_t;

class iofunctions {
	int showtick,magic;
	FILE *logfd;
	class logfunctions *log;
	void init(void);
	void flush(void);
// Log Class defines
#define    IOLOG           0
#define    FDLOG           1
#define    GENLOG          2
#define    CMOSLOG         3
#define    CDLOG           4
#define    DMALOG          5
#define    ETHLOG          6
#define    G2HLOG          7
#define    HDLOG           8
#define    KBDLOG          9
#define    NE2KLOG         10
#define    PARLOG          11
#define    PCILOG          12
#define    PICLOG          13
#define    PITLOG          14
#define    SB16LOG         15
#define    SERLOG          16
#define    VGALOG          17
#define    STLOG           18   // state_file.cc
#define    DEVLOG          19
#define    MEMLOG          20
#define    DISLOG          21
#define    GUILOG          22
#define    IOAPICLOG       23
#define    APICLOG         24
#define    CPU0LOG         25
#define    CPU1LOG         26
#define    CPU2LOG         27
#define    CPU3LOG         28
#define    CPU4LOG         29
#define    CPU5LOG         30
#define    CPU6LOG         31
#define    CPU7LOG         32
#define    CPU8LOG         33
#define    CPU9LOG         34
#define    CPU10LOG         35
#define    CPU11LOG         36
#define    CPU12LOG         37
#define    CPU13LOG         38
#define    CPU14LOG         39
#define    CPU15LOG         40
#define    CTRLLOG         41
#define    UNMAPLOG        42
#define    SERRLOG         43


public:
	iofunctions(void);
	iofunctions(FILE *);
	iofunctions(int);
	iofunctions(char *);
	~iofunctions(void);

	void out(int facility, int level, char *pre, char *fmt, va_list ap);

	void init_log(char *fn);
	void init_log(int fd);
	void init_log(FILE *fs);
	int get_n_logfns () { return n_logfn; }
	logfunc_t *get_logfn (int index) { return logfn_list[index]; }
	void add_logfn (logfunc_t *fn);
	void set_log_action (int loglevel, int action);
	char *getlevel(int i) {
		static char *loglevel[] = {
			"DEBUG",
			"INFO",
			"ERROR",
			"PANIC",
		};
	        assert (i>=0 && i<4);
		return loglevel[i];
	}
	char *getaction(int i) {
	   static char *name[] = { "ignore", "report", "ask", "fatal" };
	   assert (i>=ACT_IGNORE && i<N_ACT);
	   return name[i];
	}

protected:
	int n_logfn;
#define MAX_LOGFNS 64
	logfunc_t *logfn_list[MAX_LOGFNS];
	char *logfn;


};

typedef class iofunctions iofunc_t;


#define SAFE_GET_IOFUNC() \
  ((io==NULL)? (io=new iofunc_t("/dev/stderr")) : io)
#define SAFE_GET_GENLOG() \
  ((genlog==NULL)? (genlog=new logfunc_t(SAFE_GET_IOFUNC())) : genlog)
/* #define NO_LOGGING */
#ifndef NO_LOGGING

#define BX_INFO(x)  (LOG_THIS info) x
#define BX_DEBUG(x) (LOG_THIS ldebug) x
#define BX_ERROR(x) (LOG_THIS error) x
#define BX_PANIC(x) (LOG_THIS panic) x

#else

#define EMPTY do { } while(0)

#define BX_INFO(x)  EMPTY
#define BX_DEBUG(x) EMPTY
#define BX_ERROR(x) EMPTY
#define BX_PANIC(x) (LOG_THIS panic) x

#endif

extern iofunc_t *io;
extern logfunc_t *genlog;

#include "state_file.h"

#ifndef UNUSED
#  define UNUSED(x) ((void)x)
#endif

#define uint8   Bit8u
#define uint16  Bit16u
#define uint32  Bit32u


#if BX_PROVIDE_CPU_MEMORY==1
#  include "cpu/cpu.h"
#endif

#if BX_DISASM
#  include "disasm/disasm.h"
#endif

#if BX_DYNAMIC_TRANSLATION
#  include "dynamic/dynamic.h"
#endif


typedef struct {
  Boolean floppy;
  Boolean keyboard;
  Boolean video;
  Boolean disk;
  Boolean pit;
  Boolean pic;
  Boolean bios;
  Boolean cmos;
  Boolean a20;
  Boolean interrupts;
  Boolean exceptions;
  Boolean unsupported;
  Boolean temp;
  Boolean reset;
  Boolean debugger;
  Boolean mouse;
  Boolean io;
  Boolean xms;
  Boolean v8086;
  Boolean paging;
  Boolean creg;
  Boolean dreg;
  Boolean dma;
  Boolean unsupported_io;
  Boolean serial;
  Boolean cdrom;
#ifdef MAGIC_BREAKPOINT
  Boolean magic_break_enabled;
#endif /* MAGIC_BREAKPOINT */
#if BX_SUPPORT_APIC
  Boolean apic;
  Boolean ioapic;
#endif
#if BX_DEBUG_LINUX
  Boolean linux_syscall;
#endif
  void* record_io;
  } bx_debug_t;

#define BX_ASSERT(x) do {if (!(x)) BX_PANIC(("failed assertion \"%s\" at %s:%d\n", #x, __FILE__, __LINE__));} while (0)
void bx_signal_handler (int signum);
void bx_atexit(void);
extern bx_debug_t bx_dbg;



#define BX_FLOPPY_NONE   10 // floppy not present
#define BX_FLOPPY_1_2    11 // 1.2M  5.25"
#define BX_FLOPPY_1_44   12 // 1.44M 3.5"
#define BX_FLOPPY_2_88   13 // 2.88M 3.5"
#define BX_FLOPPY_720K   14 // 720K  3.5"
#define BX_FLOPPY_LAST   14 // last one


#define BX_READ    10
#define BX_WRITE   11
#define BX_RW      12





#include "memory/memory.h"


enum PCS_OP { PCS_CLEAR, PCS_SET, PCS_TOGGLE };

#include "pc_system.h"

#include "gui/gui.h"
#include "gui/control.h"
extern bx_gui_c   bx_gui;
#include "iodev/iodev.h"







/* --- EXTERNS --- */

#if ( BX_PROVIDE_DEVICE_MODELS==1 )
extern bx_devices_c   bx_devices;
#endif

#define BX_EJECTED   10
#define BX_INSERTED  11


#define BX_RESET_SOFTWARE 10
#define BX_RESET_HARDWARE 11


char *bx_find_bochsrc (void);
int bx_parse_cmdline (int argc, char *argv[]);
int bx_read_configuration (char *rcfile);
int bx_write_configuration (char *rcfile, int overwrite);

#define BX_PATHNAME_LEN 512

typedef struct {
  bx_param_string_c *Opath;
  bx_param_num_c *Oaddress;
  } bx_rom_options;

typedef struct {
  bx_param_string_c *Opath;
  } bx_vgarom_options;

typedef struct {
  bx_param_num_c *Osize;
  } bx_mem_options;

typedef struct {
  bx_param_string_c *Opath;
  bx_param_bool_c *OcmosImage;
  bx_param_num_c *Otime0;
  } bx_cmos_options;

typedef struct {
  bx_param_bool_c *Ovalid;
  bx_param_num_c *Oioaddr;
  bx_param_num_c *Oirq;
  bx_param_string_c *Omacaddr;
  bx_param_string_c *Oethmod;
  bx_param_string_c *Oethdev;
  } bx_ne2k_options;

typedef struct {
// These options are used for a special hack to load a
// 32bit OS directly into memory, so it can be run without
// any of the 16bit real mode or BIOS assistance.  This
// is for the development of freemware, so we don't have
// to implement real mode up front.
#define Load32bitOSNone        0
#define Load32bitOSLinux       1
#define Load32bitOSNullKernel  2 // being developed for freemware
#define Load32bitOSLast        2
  bx_param_num_c *OwhichOS;
  bx_param_string_c *Opath;
  bx_param_string_c *Oiolog;
  bx_param_string_c *Oinitrd;
  } bx_load32bitOSImage_t;

typedef struct {
  bx_param_string_c *Ofilename;
  // one array item for each log level, indexed by LOGLEV_*.
  // values: ACT_IGNORE, ACT_REPORT, ACT_ASK, ACT_FATAL
  unsigned char actions[N_LOGLEV];  
} bx_log_options;

typedef struct {
  bx_param_bool_c *Opresent;
  bx_param_string_c *Omidifile;
  bx_param_string_c *Owavefile;
  bx_param_string_c *Ologfile;
  bx_param_num_c *Omidimode;
  bx_param_num_c *Owavemode;
  bx_param_num_c *Ologlevel;
  bx_param_num_c *Odmatimer;
  } bx_sb16_options;

#define BX_BOOT_FLOPPYA 0
#define BX_BOOT_DISKC   1

typedef struct {
  bx_floppy_options floppya;
  bx_floppy_options floppyb;
  bx_disk_options   diskc;
  bx_disk_options   diskd;
  bx_cdrom_options  cdromd; 
  bx_rom_options    rom;
  bx_vgarom_options vgarom;
  bx_mem_options    memory;
  bx_sb16_options   sb16;
  bx_param_num_c    *Obootdrive;  //0=floppya, 0x80=diskc
  bx_param_num_c    *Ovga_update_interval;
  bx_param_num_c    *Okeyboard_serial_delay;
  bx_param_num_c    *Ofloppy_command_delay;
  bx_param_num_c    *Oips;
  bx_param_num_c    *Omax_ips;
  bx_param_bool_c   *Osystem_clock_sync;
  bx_param_bool_c   *Omouse_enabled;
  bx_param_bool_c   *Oprivate_colormap;
#if BX_USE_AMIGAOS
  bx_param_bool_c   *Ofullscreen;
  bx_param_string_c *Oscreenmode;
#endif
  bx_param_bool_c   *Oi440FXSupport;
  bx_cmos_options   cmos;
  bx_ne2k_options   ne2k;
  bx_param_bool_c   *OnewHardDriveSupport;
  bx_load32bitOSImage_t load32bitOSImage;
  bx_log_options    log;
  } bx_options_t;

extern bx_options_t bx_options;

void bx_center_print (FILE *file, char *line, int maxwidth);

#if BX_PROVIDE_CPU_MEMORY==1
#else
// #  include "external_interface.h"
#endif

#define BX_USE_PS2_MOUSE 1

int bx_init_hardware ();

#include "instrument.h"

#endif  /* BX_BOCHS_H */
