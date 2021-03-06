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


#include "bochs.h"




/* main memory size (in Kbytes)
 * subtract 1k for extended BIOS area
 * report only base memory, not extended mem
 */
#define BASE_MEMORY_IN_K  640

// call periodic timers every N useconds
#define TIMER_DELTA 100
//#define TIMER_DELTA 10

bx_devices_c bx_devices;




  // constructor for bx_devices_c
bx_devices_c::bx_devices_c(void)
{
  unsigned i;

#if BX_PCI_SUPPORT
  pci = NULL;
#endif
  pit = NULL;
  keyboard = NULL;
  dma = NULL;
  floppy = NULL;
  cmos = NULL;
  serial = NULL;
  parallel = NULL;
  unmapped = NULL;
  vga = NULL;
  pic = NULL;
  hard_drive = NULL;
  sb16 = NULL;
  ne2k = NULL;
  g2h = NULL;

  num_read_handles = 0;
  num_write_handles = 0;

  /* set unused elements to appropriate values */
  for (i=0; i < BX_MAX_IO_DEVICES; i++) {
    io_read_handler[i].funct  = NULL;
    io_write_handler[i].funct = NULL;
    }

  for (i=0; i < 0x10000; i++) {
    read_handler_id[i] = BX_MAX_IO_DEVICES;  // not assigned
    write_handler_id[i] = BX_MAX_IO_DEVICES;  // not assigned
    }

  for (i=0; i < BX_MAX_IRQS; i++) {
    irq_handler_name[i] = NULL;
    }

  timer_handle = BX_NULL_TIMER_HANDLE;
}


bx_devices_c::~bx_devices_c(void)
{
  // nothing needed for now
}


  void
bx_devices_c::init(void)
{
  // Start with all IO port address registered to unmapped handler
  // MUST be called first
#if BX_USE_UM_SMF
  unmapped = &bx_unmapped;
#else
  unmapped = new bx_unmapped_c();
#endif
  unmapped->init(this);

#if BX_PCI_SUPPORT
  // PCI logic (i440FX)
#if BX_USE_PCI_SMF
  pci = & bx_pci;
#else
  pci = new bx_pci_c();
#endif
  pci->init(this);
  pci->reset();
#endif


  // CMOS RAM & RTC
#if BX_USE_CMOS_SMF
  cmos = &bx_cmos;
#else
  cmos = new bx_cmos_c();
#endif
  cmos->init(this);
  cmos->reset();

  /*--- HARD DRIVE ---*/
#if BX_USE_HD_SMF
  hard_drive = &bx_hard_drive;
#else
  hard_drive = new bx_hard_drive_c();
#endif
  hard_drive->init(this, cmos);

  //--- FLOPPY ---
#if BX_USE_FD_SMF
  floppy = &bx_floppy;
#else
  floppy = new bx_floppy_ctrl_c();
#endif
  floppy->init(this, cmos);
  floppy->reset(BX_RESET_HARDWARE);

#if BX_SUPPORT_SB16
  //--- SOUND ---
#if BX_USE_SB16_SMF
  sb16 = &bx_sb16;
#else
  sb16 = new bx_sb16_c();
#endif
  sb16->init(this);
#endif

#if BX_SUPPORT_VGA
  /*--- VGA adapter ---*/
#if BX_USE_VGA_SMF
  vga = & bx_vga;
#else
  vga = new bx_vga_c();
#endif
  vga->init(this, cmos);
#else
  /*--- HGA adapter ---*/
  bx_init_hga_hardware(cmos);
#endif

  /*--- 8259A PIC ---*/
#if BX_USE_PIC_SMF
  pic = & bx_pic;
#else
  pic = new bx_pic_c();
#endif
  pic->init(this);

  /*--- 8254 PIT ---*/
#if BX_USE_PIT_SMF
  pit = & bx_pit;
#else
  pit = new bx_pit_c();
#endif
  pit->init(this);


#if BX_USE_DMA_SMF
  dma = &bx_dma;
#else
  dma = new bx_dma_c();
#endif
  dma->init(this);

#if BX_USE_KEY_SMF
  keyboard = &bx_keyboard;
#else
  keyboard = new bx_keyb_c();
#endif
  keyboard->init(this, cmos);

  /*--- PARALLEL PORT ---*/
#if BX_USE_PAR_SMF
  parallel = &bx_parallel;
#else
  parallel = new bx_parallel_c();
#endif
  parallel->init(this);

  /*--- SERIAL PORT ---*/
#if BX_USE_SER_SMF
  serial = &bx_serial;
#else
  serial = new bx_serial_c();
#endif
  serial->init(this);

#if BX_NE2K_SUPPORT
  // NE2000 NIC
#if BX_USE_NE2K_SMF
  ne2k = &bx_ne2k;
#else
  ne2k = new bx_ne2k_c();
#endif
  ne2k->init(this);
#endif  // #if BX_NE2K_SUPPORT

#if 0
  // Guest to Host interface.  Used with special guest drivers
  // which move data to/from the host environment.
  g2h = &bx_g2h;
  g2h->init(this);
#endif

  // system hardware
  register_io_read_handler( this,
                            read_handler,
                            0x0092,
                            "Port 92h System Control" );
  register_io_write_handler(this,
                            write_handler,
                            0x0092,
                            "Port 92h System Control" );

  // misc. CMOS
  Bit16u extended_memory_in_k = BX_MEM.get_memory_in_k() - 1024;
  cmos->s.reg[0x15] = (Bit8u) BASE_MEMORY_IN_K;
  cmos->s.reg[0x16] = (Bit8u) (BASE_MEMORY_IN_K >> 8);
  cmos->s.reg[0x17] = (Bit8u) extended_memory_in_k;
  cmos->s.reg[0x18] = (Bit8u) (extended_memory_in_k >> 8);
  cmos->s.reg[0x30] = (Bit8u) extended_memory_in_k;
  cmos->s.reg[0x31] = (Bit8u) (extended_memory_in_k >> 8);

  /* now perform checksum of CMOS memory */
  cmos->checksum_cmos();

  timer_handle = bx_pc_system.register_timer( this, timer_handler,
    (unsigned) TIMER_DELTA, 1, 1);
}


  Bit32u
bx_devices_c::read_handler(void *this_ptr, Bit32u address, unsigned io_len)
{
#if !BX_USE_DEV_SMF
  bx_devices_c *class_ptr = (bx_devices_c *) this_ptr;

  return( class_ptr->port92_read(address, io_len) );
}


  Bit32u
bx_devices_c::port92_read(Bit32u address, unsigned io_len)
{
#else
  UNUSED(this_ptr);
#endif  // !BX_USE_DEV_SMF
  if (io_len > 1)
    bx_panic("devices.c: port 92h: io read from address %08x, len=%u\n",
             (unsigned) address, (unsigned) io_len);

  bx_printf("devices: port92h read partially supported!!!\n");
  bx_printf("devices:   returning %02x\n", (unsigned) (BX_GET_ENABLE_A20() << 1));
  return(BX_GET_ENABLE_A20() << 1);
}


  void
bx_devices_c::write_handler(void *this_ptr, Bit32u address, Bit32u value, unsigned io_len)
{
#if !BX_USE_DEV_SMF
  bx_devices_c *class_ptr = (bx_devices_c *) this_ptr;

  class_ptr->port92_write(address, value, io_len);
}

  void
bx_devices_c::port92_write(Bit32u address, Bit32u value, unsigned io_len)
{
#else
  UNUSED(this_ptr);
#endif  // !BX_USE_DEV_SMF
  Boolean bx_cpu_reset;

  if (io_len > 1)
    bx_panic("devices.c: port 92h: io read from address %08x, len=%u\n",
             (unsigned) address, (unsigned) io_len);

  bx_printf("devices: port92h write of %02x partially supported!!!\n",
    (unsigned) value);
bx_printf("devices: A20: set_enable_a20() called\n");
  BX_SET_ENABLE_A20( (value & 0x02) >> 1 );
  bx_printf("A20: now %u\n", (unsigned) BX_GET_ENABLE_A20());
  bx_cpu_reset  = (value & 0x01); /* high speed reset */
  if (bx_cpu_reset) {
    bx_panic("PORT 92h write: CPU reset requested!\n");
    }
}

  void
bx_devices_c::timer_handler(void *this_ptr)
{
  bx_devices_c *class_ptr = (bx_devices_c *) this_ptr;

  class_ptr->timer();
}

  void
bx_devices_c::timer()
{
  unsigned retval;

  if ( pit->periodic( TIMER_DELTA ) ) {
    pic->trigger_irq(0);
    }

  retval = keyboard->periodic( TIMER_DELTA );
  if (retval & 0x01) {
    if (bx_dbg.keyboard)
      bx_printf("keyboard: interrupt(1)\n");
    pic->trigger_irq(1);
    }
  if (retval & 0x02)
    pic->trigger_irq(12);
}


  void
bx_devices_c::drq(unsigned channel, Boolean val)
{
  dma->DRQ(channel, val);
}

  void
bx_devices_c::raise_hlda(void)
{
  dma->raise_HLDA( &bx_pc_system );
}

  void
bx_devices_c::dma_read8(unsigned channel, Bit8u *data_byte)
{
  //  UNUSED( channel );
  if (channel == 2) 
    floppy->dma_read(data_byte);
#if BX_SUPPORT_SB16
  else if (channel == (unsigned) sb16->currentdma8)
    sb16->dma_read8(data_byte);
#endif
}

  void
bx_devices_c::dma_write8(unsigned channel, Bit8u *data_byte)
{
  //  UNUSED( channel );
  if (channel == 2)
    floppy->dma_write(data_byte);
#if BX_SUPPORT_SB16
  else if (channel == (unsigned) sb16->currentdma8)
    sb16->dma_write8(data_byte);
#endif
}

  void
bx_devices_c::register_irq(unsigned irq, const char *name)
{
  if (irq >= BX_MAX_IRQS) {
    bx_panic("IO device %s registered with IRQ=%d above %u\n",
             name, irq, (unsigned) BX_MAX_IRQS-1);
    }
  if (irq_handler_name[irq]) {
    bx_panic("IRQ %u conflict, %s with %s\n", irq,
      irq_handler_name[irq], name);
    }
  irq_handler_name[irq] = name;
}

  void
bx_devices_c::unregister_irq(unsigned irq, const char *name)
{
  if (irq >= BX_MAX_IRQS) {
    bx_panic("IO device %s tried to unregister IRQ %d above %u\n",
             name, irq, (unsigned) BX_MAX_IRQS-1);
    }

  if (!irq_handler_name[irq]) {
    bx_printf("IO device %s tried to unregister IRQ %d, not registered\n",
	      name, irq);
    return;
  }

  if (strcmp(irq_handler_name[irq], name)) {
    bx_printf("IRQ %u not registered to %s but to %s\n", irq,
      name, irq_handler_name[irq]);
    return;
    }
  irq_handler_name[irq] = NULL;
}

  void
bx_devices_c::register_io_read_handler( void *this_ptr, bx_read_handler_t f,
                                        Bit32u addr, const char *name )
{
  unsigned handle;

  addr &= 0x0000ffff;

  /* first find existing handle for function or create new one */
  for (handle=0; handle < num_read_handles; handle++) {
    if (io_read_handler[handle].funct == f) break;
    }

  if (handle >= num_read_handles) {
    /* no existing handle found, create new one */
    if (num_read_handles >= BX_MAX_IO_DEVICES) {
      bx_printf("too many IO devices installed.\n");
      bx_panic("  try increasing BX_MAX_IO_DEVICES\n");
      }
    num_read_handles++;
    io_read_handler[handle].funct          = f;
    io_read_handler[handle].this_ptr       = this_ptr;
    io_read_handler[handle].handler_name   = name;
    }

  /* change table to reflect new handler id for that address */
  if (read_handler_id[addr] < BX_MAX_IO_DEVICES) {
    // another handler is already registered for that address
    // if it is not the Unmapped port handler, bail
    if ( strcmp( io_read_handler[read_handler_id[addr]].handler_name, "Unmapped" ) ) {
      bx_printf("IO device address conflict(read) at IO address %Xh\n",
        (unsigned) addr);
      bx_panic("  conflicting devices: %s & %s\n",
        io_read_handler[handle].handler_name, io_read_handler[read_handler_id[addr]].handler_name);
      }
    }
  read_handler_id[addr] = handle;
}



  void
bx_devices_c::register_io_write_handler( void *this_ptr, bx_write_handler_t f,
                                        Bit32u addr, const char *name )
{
  unsigned handle;

  addr &= 0x0000ffff;

  /* first find existing handle for function or create new one */
  for (handle=0; handle < num_write_handles; handle++) {
    if (io_write_handler[handle].funct == f) break;
    }

  if (handle >= num_write_handles) {
    /* no existing handle found, create new one */
    if (num_write_handles >= BX_MAX_IO_DEVICES) {
      bx_printf("too many IO devices installed.\n");
      bx_panic("  try increasing BX_MAX_IO_DEVICES\n");
      }
    num_write_handles++;
    io_write_handler[handle].funct          = f;
    io_write_handler[handle].this_ptr       = this_ptr;
    io_write_handler[handle].handler_name   = name;
    }

  /* change table to reflect new handler id for that address */
  if (write_handler_id[addr] < BX_MAX_IO_DEVICES) {
    // another handler is already registered for that address
    // if it is not the Unmapped port handler, bail
    if ( strcmp( io_write_handler[write_handler_id[addr]].handler_name, "Unmapped" ) ) {
      bx_printf("IO device address conflict(write) at IO address %Xh\n",
        (unsigned) addr);
      bx_panic("  conflicting devices: %s & %s\n",
        io_write_handler[handle].handler_name, io_write_handler[write_handler_id[addr]].handler_name);
      }
    }
  write_handler_id[addr] = handle;
}



/*
 * Read a byte of data from the IO memory address space
 */

  Bit32u
bx_devices_c::inp(Bit16u addr, unsigned io_len)
{
  Bit8u handle;
  Bit32u ret;

  BX_INSTR_INP(addr, io_len);

  handle = read_handler_id[addr];
  ret = (* io_read_handler[handle].funct)(io_read_handler[handle].this_ptr,
                           (Bit32u) addr, io_len);
  BX_DBG_IO_REPORT(addr, io_len, BX_READ, ret);
  return(ret);
}


/*
 * Write a byte of data to the IO memory address space.
 */

  void
bx_devices_c::outp(Bit16u addr, Bit32u value, unsigned io_len)
{
  Bit8u handle;

  BX_INSTR_OUTP(addr, io_len);

  BX_DBG_IO_REPORT(addr, io_len, BX_WRITE, value);
  handle = write_handler_id[addr];
  (* io_write_handler[handle].funct)(io_write_handler[handle].this_ptr,
                     (Bit32u) addr, value, io_len);
}
