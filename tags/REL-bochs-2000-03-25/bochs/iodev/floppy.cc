//  Copyright (C) 2000  MandrakeSoft S.A.
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




#if BX_USE_FD_SMF
bx_floppy_ctrl_c bx_floppy;
#define this (&bx_floppy)
#endif



/* for main status register */
#define FD_MS_MRQ  0x80
#define FD_MS_DIO  0x40
#define FD_MS_NDMA 0x20
#define FD_MS_BUSY 0x10
#define FD_MS_ACTD 0x08
#define FD_MS_ACTC 0x04
#define FD_MS_ACTB 0x02
#define FD_MS_ACTA 0x01

#define FROM_FLOPPY 10
#define TO_FLOPPY   11

#define FLOPPY_DMA_CHAN 2



bx_floppy_ctrl_c::bx_floppy_ctrl_c(void)
{
}

bx_floppy_ctrl_c::~bx_floppy_ctrl_c(void)
{
  // nothing for now
}


  void
bx_floppy_ctrl_c::init(bx_devices_c *d, bx_cmos_c *cmos)
{
  BX_FD_THIS devices = d;

  BX_FD_THIS devices->register_irq(6, "Floppy Drive");
  for (unsigned addr=0x03F2; addr<=0x03F7; addr++) {
    BX_FD_THIS devices->register_io_read_handler(this, read_handler,
                                      addr, "Floppy Drive");
    BX_FD_THIS devices->register_io_write_handler(this, write_handler,
                                      addr, "Floppy Drive");
    }


  cmos->s.reg[0x10] = 0x00; /* start out with: no drive 0, no drive 1 */

  BX_FD_THIS s.num_supported_floppies = 0;

  //
  // Floppy A setup
  //
  BX_FD_THIS s.media[0].sectors_per_track = 0;
  BX_FD_THIS s.media[0].tracks            = 0;
  BX_FD_THIS s.media[0].heads             = 0;
  BX_FD_THIS s.media[0].sectors           = 0;
  BX_FD_THIS s.media[0].type              = BX_FLOPPY_NONE;
  BX_FD_THIS s.media[0].fd = -1;
  BX_FD_THIS s.media_present[0] = 0;


  switch (bx_options.floppya.type) {
    case BX_FLOPPY_NONE:
      cmos->s.reg[0x10] = (cmos->s.reg[0x10] & 0x0f) | 0x00;
      break;
    case BX_FLOPPY_1_2:
      cmos->s.reg[0x10] = (cmos->s.reg[0x10] & 0x0f) | 0x20;
      break;
    case BX_FLOPPY_720K:
      cmos->s.reg[0x10] = (cmos->s.reg[0x10] & 0x0f) | 0x30;
      break;
    case BX_FLOPPY_1_44:
      cmos->s.reg[0x10] = (cmos->s.reg[0x10] & 0x0f) | 0x40;
      break;
    case BX_FLOPPY_2_88:
      cmos->s.reg[0x10] = (cmos->s.reg[0x10] & 0x0f) | 0x50;
      break;
    default:
      bx_panic("floppy: unknown floppya type\n");
    }

  if (bx_options.floppya.type != BX_FLOPPY_NONE) {
    BX_FD_THIS s.num_supported_floppies++;
    if ( bx_options.floppya.initial_status == BX_INSERTED) {
      if (evaluate_media(bx_options.floppya.type, bx_options.floppya.path,
                   & BX_FD_THIS s.media[0]))
        BX_FD_THIS s.media_present[0] = 1;
      }
    }


  //
  // Floppy B setup
  //
  BX_FD_THIS s.media[1].sectors_per_track = 0;
  BX_FD_THIS s.media[1].tracks            = 0;
  BX_FD_THIS s.media[1].heads             = 0;
  BX_FD_THIS s.media[1].sectors           = 0;
  BX_FD_THIS s.media[1].type              = BX_FLOPPY_NONE;
  BX_FD_THIS s.media[1].fd = -1;
  BX_FD_THIS s.media_present[1] = 0;

  switch (bx_options.floppyb.type) {
    case BX_FLOPPY_NONE:
      cmos->s.reg[0x10] = (cmos->s.reg[0x10] & 0xf0) | 0x00;
      break;
    case BX_FLOPPY_1_2:
      cmos->s.reg[0x10] = (cmos->s.reg[0x10] & 0xf0) | 0x02;
      break;
    case BX_FLOPPY_720K:
      cmos->s.reg[0x10] = (cmos->s.reg[0x10] & 0xf0) | 0x03;
      break;
    case BX_FLOPPY_1_44:
      cmos->s.reg[0x10] = (cmos->s.reg[0x10] & 0xf0) | 0x04;
      break;
    case BX_FLOPPY_2_88:
      cmos->s.reg[0x10] = (cmos->s.reg[0x10] & 0xf0) | 0x05;
      break;
    default:
      bx_panic("floppy: unknown floppyb type\n");
    }

  if (bx_options.floppyb.type != BX_FLOPPY_NONE) {
    BX_FD_THIS s.num_supported_floppies++;
    if ( bx_options.floppyb.initial_status == BX_INSERTED) {
      if (evaluate_media(bx_options.floppyb.type, bx_options.floppyb.path,
                   & BX_FD_THIS s.media[1]))
        BX_FD_THIS s.media_present[1] = 1;
      }
    }



  /* CMOS Equipment Byte register */
  if (BX_FD_THIS s.num_supported_floppies > 0)
    cmos->s.reg[0x14] = (cmos->s.reg[0x14] & 0x3e) |
                        ((BX_FD_THIS s.num_supported_floppies-1) << 6) |
                        1;
  else
    cmos->s.reg[0x14] = (cmos->s.reg[0x14] & 0x3e);


  BX_FD_THIS s.floppy_timer_index =
    bx_pc_system.register_timer( this, timer_handler,
      bx_options.floppy_command_delay, 0,0);

  bx_printf("bx_options.floppy_command_delay = %u\n",
    (unsigned) bx_options.floppy_command_delay);
}



  void
bx_floppy_ctrl_c::reset(unsigned source)
{
  Bit32u i;

  BX_FD_THIS s.data_rate = 0; /* 500 Kbps */

  BX_FD_THIS s.command_complete = 1; /* waiting for new command */
  BX_FD_THIS s.command_index = 0;
  BX_FD_THIS s.command_size = 0;
  BX_FD_THIS s.pending_command = 0;

  BX_FD_THIS s.result_index = 0;
  BX_FD_THIS s.result_size = 0;

  /* data register ready, not in DMA mode */
  BX_FD_THIS s.main_status_reg = FD_MS_MRQ;
  BX_FD_THIS s.status_reg0 = 0;
  BX_FD_THIS s.status_reg1 = 0;
  BX_FD_THIS s.status_reg2 = 0;
  BX_FD_THIS s.status_reg3 = 0;

  // software reset (via DOR port 0x3f2 bit 2) does not change DOR
  if (source == BX_RESET_HARDWARE) {
    BX_FD_THIS s.DOR = 0x0c;
    // motor off, drive 3..0
    // DMA/INT enabled
    // normal operation
    // drive select 0

    // DIR affected only by hard reset
    BX_FD_THIS s.DIR |= 0x80; // disk changed
    }

  for (i=0; i<4; i++) {
    BX_FD_THIS s.cylinder[i] = 0;
    BX_FD_THIS s.head[i] = 0;
    BX_FD_THIS s.sector[i] = 0;
    }

  BX_FD_THIS s.floppy_buffer_index = 0;

}


  // static IO port read callback handler
  // redirects to non-static class handler to avoid virtual functions

  Bit32u
bx_floppy_ctrl_c::read_handler(void *this_ptr, Bit32u address, unsigned io_len)
{
#if !BX_USE_FD_SMF
  bx_floppy_ctrl_c *class_ptr = (bx_floppy_ctrl_c *) this_ptr;

  return( class_ptr->read(address, io_len) );
}


  /* reads from the floppy io ports */
  Bit32u
bx_floppy_ctrl_c::read(Bit32u address, unsigned io_len)
{
#else
  UNUSED(this_ptr);
#endif  // !BX_USE_FD_SMF
  Bit8u status, value;

  if (io_len > 1)
    bx_panic("floppy: io read from address %08x, len=%u\n",
             (unsigned) address, (unsigned) io_len);

// ???
//if (bx_cpu.cs.selector.value != 0xf000) {
//  bx_printf("BIOS: floppy: read access to port %04x\n", (unsigned) address);
//  }

  if (bx_dbg.floppy)
    bx_printf("floppy: read access to port %04x\n", (unsigned) address);

  switch (address) {
#if BX_DMA_FLOPPY_IO
    case 0x3F2: // diskette controller digital output register
      value = BX_FD_THIS s.DOR;
      return(value);
      break;

    case 0x3F4: /* diskette controller main status register */
      status = BX_FD_THIS s.main_status_reg;
      return(status);
      break;

    case 0x3F5: /* diskette controller data */
      if (BX_FD_THIS s.result_size == 0) {
        bx_panic("floppy: diskette controller:port3f5: no results to read\n");
        }

      value = BX_FD_THIS s.result[BX_FD_THIS s.result_index++];
      if (BX_FD_THIS s.result_index >= BX_FD_THIS s.result_size) {
        BX_FD_THIS s.result_size = 0;
        BX_FD_THIS s.result_index = 0;
        BX_FD_THIS s.main_status_reg = FD_MS_MRQ;
        }
      return(value);
      break;
#endif  // #if BX_DMA_FLOPPY_IO

    case 0x3F6: // Reserved for future floppy controllers
                // This address shared with the hard drive controller
      value = BX_FD_THIS devices->hard_drive->read_handler(BX_FD_THIS devices->hard_drive, address, io_len);
      return( value );
      break;

    case 0x3F7: // diskette controller digital input register
      // This address shared with the hard drive controller:
      //   Bit  7   : floppy
      //   Bits 6..0: hard drive
      value = BX_FD_THIS devices->hard_drive->read_handler(BX_FD_THIS devices->hard_drive, address, io_len);
      value &= 0x7f;
      // add in diskette change line
      value |= (BX_FD_THIS s.DIR & 0x80);
      return( value );
      break;
    }

#if BX_DMA_FLOPPY_IO
  bx_panic("floppy: io_read: bailing\n");
  return(0);
#endif  // #if BX_DMA_FLOPPY_IO
}


  // static IO port write callback handler
  // redirects to non-static class handler to avoid virtual functions

  void
bx_floppy_ctrl_c::write_handler(void *this_ptr, Bit32u address, Bit32u value, unsigned io_len)
{
#if !BX_USE_FD_SMF
  bx_floppy_ctrl_c *class_ptr = (bx_floppy_ctrl_c *) this_ptr;

  class_ptr->write(address, value, io_len);
}

  /* writes to the floppy io ports */
  void
bx_floppy_ctrl_c::write(Bit32u address, Bit32u value, unsigned io_len)
{
#else
  UNUSED(this_ptr);
#endif  // !BX_USE_FD_SMF
  Bit8u dma_and_interrupt_enable;
  Bit8u normal_operation, prev_normal_operation;
  Bit8u drive_select;
  Bit8u motor_on_drive0, motor_on_drive1;

  if (io_len > 1)
    bx_panic("floppy: io write to address %08x, len=%u\n",
             (unsigned) address, (unsigned) io_len);

// ???
//if (bx_cpu.cs.selector.value != 0xf000) {
//  bx_printf("BIOS: floppy: write access to port %04x, value=%02x\n",
//      (unsigned) address, (unsigned) value);
//  }

  if (bx_dbg.floppy)
    bx_printf("floppy: write access to port %04x, value=%02x\n",
      (unsigned) address, (unsigned) value);

  switch (address) {
#if BX_DMA_FLOPPY_IO
    case 0x3F2: /* diskette controller digital output register */
      motor_on_drive1 = value & 0x20;
      motor_on_drive0 = value & 0x10;
      dma_and_interrupt_enable = value & 0x08;
      if (!dma_and_interrupt_enable)
        bx_printf("floppy: DMA and interrupt capabilities disabled\n");
      normal_operation = value & 0x04;
      drive_select = value & 0x03;

      prev_normal_operation = BX_FD_THIS s.DOR & 0x04;
      BX_FD_THIS s.DOR = value;

      if (prev_normal_operation==0 && normal_operation) {
        // transition from RESET to NORMAL
#if 0

        BX_FD_THIS s.main_status_reg = FD_MS_BUSY;
        BX_FD_THIS s.pending_command = 0xfe; // RESET pending

        bx_pc_system.activate_timer( BX_FD_THIS s.floppy_timer_index,
             bx_options.floppy_command_delay, 0 );
#endif
        }
      else if (prev_normal_operation && normal_operation==0) {
        // transition from NORMAL to RESET
        BX_FD_THIS s.main_status_reg = FD_MS_BUSY;
        BX_FD_THIS s.pending_command = 0xfe; // RESET pending

        bx_pc_system.activate_timer( BX_FD_THIS s.floppy_timer_index,
             bx_options.floppy_command_delay, 0 );
        }
      if (bx_dbg.floppy) {
        bx_printf("floppy: io_write: digital output register\n");
        bx_printf("  motor on, drive1 = %d\n", motor_on_drive1 > 0);
        bx_printf("  motor on, drive0 = %d\n", motor_on_drive0 > 0);
        bx_printf("  dma_and_interrupt_enable=%02x\n",
          (unsigned) dma_and_interrupt_enable);
        bx_printf("  normal_operation=%02x\n",
          (unsigned) normal_operation);
        bx_printf("  drive_select=%02x\n",
          (unsigned) drive_select);
        }
      if (drive_select>1) {
        bx_panic("floppy: io_write: drive_select>1\n");
        }
      break;

    case 0x3f4: /* diskette controller data rate select register */
      bx_panic("floppy: io_write: data rate select register\n");
      break;

    case 0x3F5: /* diskette controller data */
      if (bx_dbg.floppy)
        bx_printf("floppy: command = %02x\n", (unsigned) value);
      if (BX_FD_THIS s.command_complete) {
        if (BX_FD_THIS s.pending_command!=0)
          bx_panic("floppy: io: 3f5: receiving new comm, old one (%02x) pending\n",
            (unsigned) BX_FD_THIS s.pending_command);
        BX_FD_THIS s.command[0] = value;
        BX_FD_THIS s.command_complete = 0;
        BX_FD_THIS s.command_index = 1;
        /* read/write command in progress */
        BX_FD_THIS s.main_status_reg = FD_MS_MRQ | FD_MS_BUSY;
        switch (value) {
          case 0x03: /* specify */
            BX_FD_THIS s.command_size = 3;
            break;
          case 0x04: // get status
            BX_FD_THIS s.command_size = 2;
            break;
          case 0x07: /* recalibrate */
            BX_FD_THIS s.command_size = 2;
            break;
          case 0x08: /* sense interrupt status */
            BX_FD_THIS s.command_size = 1;
            floppy_command();
            BX_FD_THIS s.command_complete = 1;
            break;
          case 0x0f: /* seek */
            BX_FD_THIS s.command_size = 3;
            break;
          case 0x4a: /* read ID */
            BX_FD_THIS s.command_size = 2;
            break;
          case 0xc5: /* write normal data */
            BX_FD_THIS s.command_size = 9;
            break;
          case 0xe6: /* read normal data */
            BX_FD_THIS s.command_size = 9;
            break;

          case 0x13: // Configure command (Enhanced)
            BX_FD_THIS s.command_size = 3;
            break;

          case 0x0e: // dump registers (Enhanced drives)
          case 0x10: // Version command, standard controller returns 80h
          case 0x18: // National Semiconductor version command; return 80h
            // These commands are not implemented on the standard
            // controller and return an error.  They are available on
            // the enhanced controller.
            BX_FD_THIS s.command_size = 1;
            BX_FD_THIS s.result[0] = 0x80;
            BX_FD_THIS s.result_size = 1;
            BX_FD_THIS s.result_index = 0;
            BX_FD_THIS s.main_status_reg = FD_MS_MRQ | FD_MS_DIO | FD_MS_BUSY;
            BX_FD_THIS s.command_complete = 1;
            break;

          default:
            bx_panic("floppy: io write:3f5: unsupported case 0x%02x\n",
              (unsigned) value);
            break;
          }
        }
      else {
        BX_FD_THIS s.command[BX_FD_THIS s.command_index++] =
          value;
        if (BX_FD_THIS s.command_index ==
            BX_FD_THIS s.command_size) {
          /* read/write command not in progress any more */
          floppy_command();
          BX_FD_THIS s.command_complete = 1;
          }
        }
      if (bx_dbg.floppy)
        bx_printf("floppy: io_write: diskette controller data\n");
      return;
      break;
#endif  // #if BX_DMA_FLOPPY_IO

    case 0x3F6: /* diskette controller (reserved) */
      if (bx_dbg.floppy)
        bx_printf("floppy: io_write: reserved register unsupported\n");
      // this address shared with the hard drive controller
      BX_FD_THIS devices->hard_drive->write_handler(BX_FD_THIS devices->hard_drive, address, value, io_len);
      break;

#if BX_DMA_FLOPPY_IO
    case 0x3F7: /* diskette controller configuration control register */
      if (bx_dbg.floppy)
        bx_printf("floppy: io_write: config control register\n");
      BX_FD_THIS s.data_rate = value & 0x03;
      if (bx_dbg.floppy)
        switch (BX_FD_THIS s.data_rate) {
          case 0: bx_printf("  500 Kbps\n"); break;
          case 1: bx_printf("  300 Kbps\n"); break;
          case 2: bx_printf("  250 Kbps\n"); break;
          case 3: bx_printf("  1 Mbps\n"); break;
          }
      return;
      break;

   default:
      bx_panic("floppy: io_write: unknown port %04h\n", (unsigned) address);
      break;
#endif  // #if BX_DMA_FLOPPY_IO
    }
}



  void
bx_floppy_ctrl_c::floppy_command(void)
{
#if BX_PROVIDE_CPU_MEMORY==0
  bx_panic("floppy_command(): uses DMA: not supported for"
           " external environment\n");
#else
  unsigned i;
  Bit8u step_rate_time;
  Bit8u head_unload_time;
  Bit8u head_load_time;
  Bit8u motor_on;
  Bit8u head, drive, cylinder, sector, eot;
  Bit8u sector_size, data_length;
  Bit32u logical_sector;


  if (bx_dbg.floppy) {
    bx_printf("FLOPPY COMMAND: ");
    for (i=0; i<BX_FD_THIS s.command_size; i++)
      bx_printf("[%02x] ", (unsigned) BX_FD_THIS s.command[i]);
    bx_printf("\n");
    }

#if 0
  /* execute phase of command is in progress (non DMA mode) */
  BX_FD_THIS s.main_status_reg |= 20;
#endif

  switch (BX_FD_THIS s.command[0]) {
    case 0x03: // specify
//bx_printf("floppy_command specify\n");
      // execution: specified parameters are loaded
      // result: no result bytes, no interrupt
      step_rate_time = BX_FD_THIS s.command[1] >> 4;
      head_unload_time = BX_FD_THIS s.command[1] & 0x0f;
      head_load_time = BX_FD_THIS s.command[2] >> 1;
      BX_FD_THIS s.main_status_reg = FD_MS_MRQ;
      return;
      break;

    case 0x04: // get status
      BX_FD_THIS s.result[0] = 0x00;
      BX_FD_THIS s.result_size = 1;
      BX_FD_THIS s.result_index = 0;
      BX_FD_THIS s.main_status_reg = FD_MS_MRQ | FD_MS_DIO | FD_MS_BUSY;
      return;
      break;

    case 0x07: // recalibrate
//bx_printf("floppy_command recalibrate\n");
      drive = (BX_FD_THIS s.command[1] & 0x03);
      BX_FD_THIS s.DOR &= 0xfc;
      BX_FD_THIS s.DOR |= drive;
      if (bx_dbg.floppy)
        bx_printf("floppy_command(): recalibrate drive %u\n",
          (unsigned) drive);
      if (drive > 1)
        bx_panic("floppy_command(): drive > 1\n");
      //motor_on = BX_FD_THIS s.DOR & 0xf0;
      motor_on = ( (BX_FD_THIS s.DOR>>(drive+4))
                     & 0x01 );
      if (motor_on == 0) {
        bx_printf("floppy_command(): recal drive with motor off\n");
        }
      if (drive==0)
        BX_FD_THIS s.DOR |= 0x10; // turn on MOTA
      else
        BX_FD_THIS s.DOR |= 0x20; // turn on MOTB
      BX_FD_THIS s.cylinder[drive] = 0;
      bx_pc_system.activate_timer( BX_FD_THIS s.floppy_timer_index,
        bx_options.floppy_command_delay, 0 );
      /* command head to track 0
       * controller set to non-busy
       * error condition noted in Status reg 0's equipment check bit
       * seek end bit set to 1 in Status reg 0 regardless of outcome
       */
      /* data reg not ready, controller busy */
      BX_FD_THIS s.main_status_reg = FD_MS_DIO | FD_MS_BUSY;
      BX_FD_THIS s.pending_command = 0x07; // recalibrate pending
      return;
      break;

    case 0x08: /* sense interrupt status */
//bx_printf("floppy_command sense interrupt status\n");
      /* execution:
       *   get status
       * result:
       *   no interupt
       *   byte0 = status reg0
       *   byte1 = current cylinder number (0 to 79)
       */
      /*BX_FD_THIS s.status_reg0 = ;*/
      drive = BX_FD_THIS s.DOR & 0x03;
      BX_FD_THIS s.result[0] = 0x20 | drive;
      BX_FD_THIS s.result[1] = BX_FD_THIS s.cylinder[drive];
      BX_FD_THIS s.result_size = 2;
      BX_FD_THIS s.result_index = 0;

      /* read ready */
      BX_FD_THIS s.main_status_reg = FD_MS_MRQ | FD_MS_DIO | FD_MS_BUSY;
      if (bx_dbg.floppy)
        bx_printf("sense interrupt status\n");
      return;
      break;

    case 0x0f: /* seek */
//bx_printf("floppy_command seek\n");
      /* command:
       *   byte0 = 0F
       *   byte1 = drive & head select
       *   byte2 = cylinder number
       * execution:
       *   postion head over specified cylinder
       * result:
       *   no result bytes, issues an interrupt
       */
      drive = BX_FD_THIS s.command[1] & 0x03;
      BX_FD_THIS s.DOR &= 0xfc;
      BX_FD_THIS s.DOR |= drive;

      BX_FD_THIS s.head[drive] = (BX_FD_THIS s.command[1] >> 2) & 0x01;
      BX_FD_THIS s.cylinder[drive] = BX_FD_THIS s.command[2];
      if (drive > 1)
        bx_panic("floppy_command(): seek: drive>1\n");
      /* ??? should also check cylinder validity */
      bx_pc_system.activate_timer( BX_FD_THIS s.floppy_timer_index,
        bx_options.floppy_command_delay, 0 );
      /* data reg not ready, controller busy */
      BX_FD_THIS s.main_status_reg = FD_MS_DIO | FD_MS_BUSY;
      BX_FD_THIS s.pending_command = 0x0f; /* seek pending */
      return;
      break;

    case 0x13: // Configure
      bx_printf("floppy io: configure (mode=%02xh, pretrack=%02xh)\n",
(unsigned)(BX_FD_THIS s.command[2]), (unsigned)(BX_FD_THIS s.command[3]));
      BX_FD_THIS s.result_size = 0;
      BX_FD_THIS s.result_index = 0;
      BX_FD_THIS s.pending_command = 0;
      BX_FD_THIS s.main_status_reg = FD_MS_MRQ | FD_MS_DIO | FD_MS_BUSY;
      return;
      break;

    case 0x4a: // read ID
//bx_printf("floppy_command read ID\n"); // ???
      drive = BX_FD_THIS s.command[1] & 0x03;
      BX_FD_THIS s.DOR &= 0xfc;
      BX_FD_THIS s.DOR |= drive;

      motor_on = (BX_FD_THIS s.DOR>>(drive+4)) & 0x01;
      if (motor_on == 0)
        bx_panic("floppy_command(): 4a: motor not on\n");
      if (drive > 1)
        bx_panic("floppy io: 4a: bad drive #\n");
      BX_FD_THIS s.result_size = 7;
      BX_FD_THIS s.result_index = 0;
      BX_FD_THIS s.result[0] = 0; /* ??? */
      BX_FD_THIS s.result[1] = 0;
      BX_FD_THIS s.result[2] = 0;
      BX_FD_THIS s.result[3] = BX_FD_THIS s.cylinder[drive];
      BX_FD_THIS s.result[4] = 0; /* head */
      BX_FD_THIS s.result[5] = 0; /* sector at completion */
      BX_FD_THIS s.result[6] = 2;
      bx_pc_system.activate_timer( BX_FD_THIS s.floppy_timer_index,
        bx_options.floppy_command_delay, 0 );
      /* data reg not ready, controller busy */
      BX_FD_THIS s.main_status_reg = FD_MS_DIO | FD_MS_BUSY;
      BX_FD_THIS s.pending_command = 0x4a; /* read ID pending */
      return;
      break;


    case 0xe6: // read normal data
//bx_printf("floppy_command read normal data\n");
    case 0xc5: // write normal data
//bx_printf("floppy_command write normal data\n");
      if ( (BX_FD_THIS s.DOR & 0x08) == 0 )
        bx_panic("floppy: read/write command with DMA and int disabled\n");
      drive = BX_FD_THIS s.command[1] & 0x03;
      BX_FD_THIS s.DOR &= 0xfc;
      BX_FD_THIS s.DOR |= drive;

      motor_on = (BX_FD_THIS s.DOR>>(drive+4)) & 0x01;
      if (motor_on == 0)
        bx_panic("floppy_command(): read/write: motor not on\n");
      head = BX_FD_THIS s.command[3] & 0x01;
      cylinder = BX_FD_THIS s.command[2]; /* 0..79 depending */
      sector = BX_FD_THIS s.command[4];   /* 1..36 depending */
      eot = BX_FD_THIS s.command[6];      /* 1..36 depending */
      sector_size = BX_FD_THIS s.command[5];
      data_length = BX_FD_THIS s.command[8];
      if (bx_dbg.floppy) {
        bx_printf("\n\nread/write normal data\n");
        bx_printf("BEFORE\n");
        bx_printf("  drive    = %u\n", (unsigned) drive);
        bx_printf("  head     = %u\n", (unsigned) head);
        bx_printf("  cylinder = %u\n", (unsigned) cylinder);
        bx_printf("  sector   = %u\n", (unsigned) sector);
        bx_printf("  eot      = %u\n", (unsigned) eot);
        }
      if (drive > 1)
        bx_panic("floppy io: bad drive #\n");
      if (head > 1)
        bx_panic("floppy io: bad head #\n");

      if ( BX_FD_THIS s.media_present[drive] == 0 ) {
        // media not in drive, return error

        bx_printf("floppy_command: attempt to read/write sector %u,"
                     " sectors/track=%u\n", (unsigned) sector,
                     (unsigned) BX_FD_THIS s.media[drive].sectors_per_track);
        BX_FD_THIS s.result_size = 7;
        BX_FD_THIS s.result_index = 0;
        BX_FD_THIS s.result[0] = 0x40 | (BX_FD_THIS s.head[drive]<<2) | drive; // abnormal termination
        BX_FD_THIS s.result[1] = 0x25; // 0010 0101
        BX_FD_THIS s.result[2] = 0x31; // 0011 0001
        BX_FD_THIS s.result[3] = BX_FD_THIS s.cylinder[drive];
        BX_FD_THIS s.result[4] = BX_FD_THIS s.head[drive];
        BX_FD_THIS s.result[5] = BX_FD_THIS s.sector[drive];
        BX_FD_THIS s.result[6] = 2; // sector size = 512

        BX_FD_THIS s.pending_command = 0;
        BX_FD_THIS s.main_status_reg = FD_MS_MRQ | FD_MS_DIO | FD_MS_BUSY;
        BX_FD_THIS devices->pic->trigger_irq(6);
        return;
        }

      if (sector_size != 0x02) { // 512 bytes
        bx_panic("floppy: sector_size not 512\n");
        }
      if ( cylinder >= BX_FD_THIS s.media[drive].tracks ) {
        bx_printf("\nfloppy io: normal read/write: params out of range\n");
        bx_printf("*** sector # %02xh\n", (unsigned) sector);
        bx_printf("*** cylinder #%02xh\n", (unsigned) cylinder);
        bx_printf("*** eot #%02xh\n", (unsigned) eot);
        bx_printf("*** head #%02xh\n", (unsigned) head);
        bx_panic("bailing\n");
        return;
        }

      if (sector > BX_FD_THIS s.media[drive].sectors_per_track) {
        // requested sector > last sector on track
        bx_printf("floppy_command: attempt to read/write sector %u,"
                     " sectors/track=%u\n", (unsigned) sector,
                     (unsigned) BX_FD_THIS s.media[drive].sectors_per_track);
        // set controller to where drive would have left off
        // after it discovered the sector was past EOT
        BX_FD_THIS s.cylinder[drive] = cylinder;
        BX_FD_THIS s.head[drive]     = head;
        BX_FD_THIS s.sector[drive]   = BX_FD_THIS s.media[drive].sectors_per_track;

        BX_FD_THIS s.result_size = 7;

        BX_FD_THIS s.result_index = 0;
        // 0100 0HDD abnormal termination
        BX_FD_THIS s.result[0] = 0x40 | (BX_FD_THIS s.head[drive]<<2) | drive;
        // 1000 0101 end of cyl/NDAT/NID
        BX_FD_THIS s.result[1] = 0x86;
        // 0000 0000
        BX_FD_THIS s.result[2] = 0x00;
        BX_FD_THIS s.result[3] = BX_FD_THIS s.cylinder[drive];
        BX_FD_THIS s.result[4] = BX_FD_THIS s.head[drive];
        BX_FD_THIS s.result[5] = BX_FD_THIS s.sector[drive];
        BX_FD_THIS s.result[6] = 2; // sector size = 512

        bx_pc_system.activate_timer( BX_FD_THIS s.floppy_timer_index,
          bx_options.floppy_command_delay, 0 );
        /* data reg not ready, controller busy */
        BX_FD_THIS s.main_status_reg = FD_MS_DIO | FD_MS_BUSY;
        BX_FD_THIS s.pending_command = BX_FD_THIS s.command[0];
        return;
        }

#if 0
      if (eot != BX_FD_THIS s.media[drive].sectors_per_track)
        bx_printf("floppy io: bad eot #%02xh\n", (unsigned) eot);
#endif

      if (cylinder != BX_FD_THIS s.cylinder[drive])
        bx_printf("floppy io: cylinder request != current cylinder\n");

      logical_sector = (cylinder * 2 * BX_FD_THIS s.media[drive].sectors_per_track) +
                       (head * BX_FD_THIS s.media[drive].sectors_per_track) +
                       (sector - 1);

      if (logical_sector >= BX_FD_THIS s.media[drive].sectors) {
        bx_panic("floppy io: logical sector out of bounds\n");
        }

      BX_FD_THIS s.cylinder[drive] = cylinder;
      BX_FD_THIS s.sector[drive]   = sector;
      BX_FD_THIS s.head[drive]     = head;

      if (BX_FD_THIS s.command[0] == 0xe6) { // read
        floppy_xfer(drive, logical_sector*512, BX_FD_THIS s.floppy_buffer,
                    512, FROM_FLOPPY);
        BX_FD_THIS s.floppy_buffer_index = 0;

        bx_pc_system.set_DRQ(FLOPPY_DMA_CHAN, 1);

        /* data reg not ready, controller busy */
        BX_FD_THIS s.main_status_reg = FD_MS_MRQ | FD_MS_DIO | FD_MS_BUSY;
        BX_FD_THIS s.pending_command = BX_FD_THIS s.command[0];
        return;
        }
      else if (BX_FD_THIS s.command[0] == 0xc5) { // write
        BX_FD_THIS s.floppy_buffer_index = 0;

        bx_pc_system.set_DRQ(FLOPPY_DMA_CHAN, 1);

        /* data reg not ready, controller busy */
        BX_FD_THIS s.main_status_reg = FD_MS_MRQ | FD_MS_DIO | FD_MS_BUSY;
        BX_FD_THIS s.pending_command = BX_FD_THIS s.command[0];
        return;
        }
      else
        bx_panic("floppy_command(): unknown read/write command\n");

      return;
      break;

    default:
      bx_panic("floppy_command(): unknown function\n");
    }
  bx_panic("\nfloppy_command()\n");
#endif
}

  void
bx_floppy_ctrl_c::floppy_xfer(Bit8u drive, Bit32u offset, Bit8u *buffer,
            Bit32u bytes, Bit8u direction)
{
  int ret;

  if (drive > 1)
    bx_panic("floppy_xfer: drive > 1\n");

  if (bx_dbg.floppy) {
    bx_printf("drive=%u\n", (unsigned) drive);
    bx_printf("offset=%u\n", (unsigned) offset);
    bx_printf("bytes=%u\n", (unsigned) bytes);
    bx_printf("direction=%s\n", (direction==FROM_FLOPPY)? "from" : "to");
    }

#ifdef macintosh
  if (strcmp(bx_options.floppya.path, SuperDrive))
#endif
    {
    ret = lseek(BX_FD_THIS s.media[drive].fd, offset, SEEK_SET);
    if (ret < 0) {
      bx_panic("could not perform lseek() on floppy image file\n");
      }
    }

  if (direction == FROM_FLOPPY) {
#ifdef macintosh
    if (!strcmp(bx_options.floppya.path, SuperDrive))
      ret = fd_read((char *) buffer, offset, bytes);
    else
#endif
      ret = ::read(BX_FD_THIS s.media[drive].fd, (bx_ptr_t) buffer, bytes);
    if (ret < int(bytes)) {
      /* ??? */
      if (ret > 0) {
        bx_printf("floppy: partial read() on floppy image returns %u/%u\n",
          (unsigned) ret, (unsigned) bytes);
        memset(buffer + ret, 0, bytes - ret);
        }
      else {
        bx_printf("floppy: read() on floppy image returns 0\n");
        memset(buffer, 0, bytes);
        }
      }
    }

  else { // TO_FLOPPY
#ifdef macintosh
    if (!strcmp(bx_options.floppya.path, SuperDrive))
      ret = fd_write((char *) buffer, offset, bytes);
    else
#endif
      ret = ::write(BX_FD_THIS s.media[drive].fd, (bx_ptr_t) buffer, bytes);
    if (ret < int(bytes)) {
      bx_panic("could not perform write() on floppy image file\n");
      }
    }
}



  void
bx_floppy_ctrl_c::timer_handler(void *this_ptr)
{
#if defined(SIMX86)
  printf("Floppy timer\n");
#endif

  bx_floppy_ctrl_c *class_ptr = (bx_floppy_ctrl_c *) this_ptr;

  class_ptr->timer();
}

  void
bx_floppy_ctrl_c::timer()
{
  switch ( BX_FD_THIS s.pending_command ) {
    case 0x07: // recal
      BX_FD_THIS s.pending_command = 0;
      /* write ready, not busy */
      BX_FD_THIS s.main_status_reg = FD_MS_MRQ;
      BX_FD_THIS devices->pic->trigger_irq(6);
      goto reset_changeline;
      break;

    case 0x0f: // seek
      BX_FD_THIS s.pending_command = 0;
      /* write ready, not busy */
      BX_FD_THIS s.main_status_reg = FD_MS_MRQ;
      BX_FD_THIS devices->pic->trigger_irq(6);
      goto reset_changeline;
      break;


    case 0x4a: /* read ID */
    case 0xc5: // write normal data
    case 0xe6: // read normal data
      BX_FD_THIS s.pending_command = 0;
      /* read ready, busy */
      BX_FD_THIS s.main_status_reg = FD_MS_MRQ | FD_MS_DIO;
      BX_FD_THIS devices->pic->trigger_irq(6);
      break;

    case 0xfe: // (contrived) RESET
      reset(BX_RESET_SOFTWARE);
      BX_FD_THIS s.pending_command = 0;
      BX_FD_THIS s.main_status_reg = FD_MS_MRQ;
      BX_FD_THIS devices->pic->trigger_irq(6);
      break;

    default:
      bx_panic("floppy:timer(): unknown case %02x\n",
        (unsigned) BX_FD_THIS s.pending_command);
    }
  return;

reset_changeline:
  unsigned drive = BX_FD_THIS s.DOR & 0x3;
  if (drive > 1) return;
  if (BX_FD_THIS s.media_present[drive])
    BX_FD_THIS s.DIR &= ~0x80; // clear disk change line
}

  void
bx_floppy_ctrl_c::dma_write(Bit8u *data_byte)
{
  // A DMA write is from I/O to Memory
  // We need to return then next data byte from the floppy buffer
  // to be transfered via the DMA to memory. (read block from floppy)


  *data_byte = BX_FD_THIS s.floppy_buffer[BX_FD_THIS s.floppy_buffer_index++];

  if (BX_FD_THIS s.floppy_buffer_index >= 512) {
    Bit8u drive;

    drive = BX_FD_THIS s.DOR & 0x03;
    increment_sector(); // increment to next sector before retrieving next one
    BX_FD_THIS s.floppy_buffer_index = 0;
    if (bx_pc_system.TC) { // Terminal Count line, done
      BX_FD_THIS s.pending_command = 0;
      BX_FD_THIS s.main_status_reg = FD_MS_MRQ | FD_MS_DIO | FD_MS_BUSY;
      BX_FD_THIS s.result_size = 7;
      BX_FD_THIS s.result_index = 0;
      BX_FD_THIS s.result[0] = (BX_FD_THIS s.head[drive] << 2) | drive;
      BX_FD_THIS s.result[1] = 0;
      BX_FD_THIS s.result[2] = 0;
      BX_FD_THIS s.result[3] = BX_FD_THIS s.cylinder[drive];
      BX_FD_THIS s.result[4] = BX_FD_THIS s.head[drive];
      BX_FD_THIS s.result[5] = BX_FD_THIS s.sector[drive];
      BX_FD_THIS s.result[6] = 2;

      if (bx_dbg.floppy) {
        bx_printf("<<READ DONE>>\n");
        bx_printf("AFTER\n");
        bx_printf("  drive    = %u\n", (unsigned) drive);
        bx_printf("  head     = %u\n", (unsigned) BX_FD_THIS s.head[drive]);
        bx_printf("  cylinder = %u\n", (unsigned) BX_FD_THIS s.cylinder[drive]);
        bx_printf("  sector   = %u\n", (unsigned) BX_FD_THIS s.sector[drive]);
        }

      BX_FD_THIS devices->pic->trigger_irq(6);
      bx_pc_system.set_DRQ(FLOPPY_DMA_CHAN, 0);
      }
    else { // more data to transfer
      Bit32u logical_sector;
      logical_sector = (BX_FD_THIS s.cylinder[drive] * 2 *
                        BX_FD_THIS s.media[drive].sectors_per_track) +
                       (BX_FD_THIS s.head[drive] *
                        BX_FD_THIS s.media[drive].sectors_per_track) +
                       (BX_FD_THIS s.sector[drive] - 1);
      floppy_xfer(drive, logical_sector*512, BX_FD_THIS s.floppy_buffer,
                  512, FROM_FLOPPY);
      }
    }
}

  void
bx_floppy_ctrl_c::dma_read(Bit8u *data_byte)
{
  // A DMA read is from Memory to I/O
  // We need to write the data_byte which was already transfered from memory
  // via DMA to I/O (write block to floppy)

  Bit8u drive;
  Bit32u logical_sector;

  BX_FD_THIS s.floppy_buffer[BX_FD_THIS s.floppy_buffer_index++] = *data_byte;

  if (BX_FD_THIS s.floppy_buffer_index >= 512) {
    drive = BX_FD_THIS s.DOR & 0x03;
    logical_sector = (BX_FD_THIS s.cylinder[drive] * 2 * BX_FD_THIS s.media[drive].sectors_per_track) +
                     (BX_FD_THIS s.head[drive] * BX_FD_THIS s.media[drive].sectors_per_track) +
                     (BX_FD_THIS s.sector[drive] - 1);
    floppy_xfer(drive, logical_sector*512, BX_FD_THIS s.floppy_buffer,
                512, TO_FLOPPY);
    increment_sector(); // increment to next sector after writing current one
    BX_FD_THIS s.floppy_buffer_index = 0;
    if (bx_pc_system.TC) { // Terminal Count line, done
      BX_FD_THIS s.pending_command = 0;
      BX_FD_THIS s.main_status_reg = FD_MS_MRQ | FD_MS_DIO | FD_MS_BUSY;
      BX_FD_THIS s.result_size = 7;
      BX_FD_THIS s.result_index = 0;
      BX_FD_THIS s.result[0] = (BX_FD_THIS s.head[drive] << 2) | drive;
      BX_FD_THIS s.result[1] = 0;
      BX_FD_THIS s.result[2] = 0;
      BX_FD_THIS s.result[3] = BX_FD_THIS s.cylinder[drive];
      BX_FD_THIS s.result[4] = BX_FD_THIS s.head[drive];
      BX_FD_THIS s.result[5] = BX_FD_THIS s.sector[drive];
      BX_FD_THIS s.result[6] = 2;
      if (bx_dbg.floppy) {
        bx_printf("<<WRITE DONE>>\n");
        bx_printf("AFTER\n");
        bx_printf("  drive    = %u\n", (unsigned) drive);
        bx_printf("  head     = %u\n", (unsigned) BX_FD_THIS s.head[drive]);
        bx_printf("  cylinder = %u\n", (unsigned) BX_FD_THIS s.cylinder[drive]);
        bx_printf("  sector   = %u\n", (unsigned) BX_FD_THIS s.sector[drive]);
        }

      BX_FD_THIS devices->pic->trigger_irq(6);
      bx_pc_system.set_DRQ(FLOPPY_DMA_CHAN, 0);
      }
    else { // more data to transfer
      } // else
    } // if BX_FD_THIS s.floppy_buffer_index >= 512
}



  void
bx_floppy_ctrl_c::increment_sector(void)
{
  Bit8u drive;

  drive = BX_FD_THIS s.DOR & 0x03;

  // values after completion of data xfer
  // ??? calculation depends on base_count being multiple of 512
  BX_FD_THIS s.sector[drive] ++;
  if (BX_FD_THIS s.sector[drive] > BX_FD_THIS s.media[drive].sectors_per_track) {
    BX_FD_THIS s.sector[drive] -= BX_FD_THIS s.media[drive].sectors_per_track;
    BX_FD_THIS s.head[drive] ++;
    if (BX_FD_THIS s.head[drive] > 1) {
      BX_FD_THIS s.head[drive] = 0;
      BX_FD_THIS s.cylinder[drive] ++;
      if (BX_FD_THIS s.cylinder[drive] >= BX_FD_THIS s.media[drive].tracks) {
        // Set to 1 past last possible cylinder value.
        // I notice if I set it to tracks-1, prama linux won't boot.
        BX_FD_THIS s.cylinder[drive] = BX_FD_THIS s.media[drive].tracks;
        bx_printf("increment_sector: clamping cylinder to max\n");
        }
      }
    }
}

  unsigned
bx_floppy_ctrl_c::set_media_status(unsigned drive, unsigned status)
{
  char *path;
  unsigned type;

  // if setting to the current value, nothing to do
  if (status == BX_FD_THIS s.media_present[drive])
    return(status);

  if (status == 0) {
    // eject floppy
    if (BX_FD_THIS s.media[drive].fd >= 0) {
      close( BX_FD_THIS s.media[drive].fd );
      BX_FD_THIS s.media[drive].fd = -1;
      }
    BX_FD_THIS s.media_present[drive] = 0;
    BX_FD_THIS s.DIR |= 0x80; // disk changed line
    return(0);
    }
  else {
    // insert floppy
    if (drive == 0) {
      path = bx_options.floppya.path;
      type = bx_options.floppya.type;
      }
    else {
      path = bx_options.floppyb.path;
      type = bx_options.floppyb.type;
      }
    if (evaluate_media(type, path, & BX_FD_THIS s.media[drive])) {
      BX_FD_THIS s.media_present[drive] = 1;
      BX_FD_THIS s.DIR |= 0x80; // disk changed line
      return(1);
      }
    else {
      BX_FD_THIS s.media_present[drive] = 0;
      return(0);
      }
    }
}

  unsigned
bx_floppy_ctrl_c::get_media_status(unsigned drive)
{
  return( BX_FD_THIS s.media_present[drive] );
}

  Boolean
bx_floppy_ctrl_c::evaluate_media(unsigned type, char *path, floppy_t *media)
{
  struct stat stat_buf;
  int ret;
  char sTemp[1024];

  if (type == BX_FLOPPY_NONE)
    return(0);

  // open media file (image file or device)
#ifdef macintosh
  media->fd = 0;
  if (strcmp(bx_options.floppya.path, SuperDrive))
#endif
    media->fd = open(path, O_RDWR
#ifdef O_BINARY
                   | O_BINARY
#endif
                  );

  if (media->fd < 0) {
    //perror("floppy open of a:\n");
    sprintf(sTemp,"floppy open of %s:\n",path);
    perror(sTemp);
    return(0);
    }

#ifdef macintosh
  if (!strcmp(bx_options.floppya.path, SuperDrive))
    ret = fd_stat(&stat_buf);
  else
#endif
    ret = fstat(media->fd, &stat_buf);
  if (ret) {
    perror("fstat'ing floppy 0 drive image file");
    bx_panic("fstat() returns error!\n");
    return(0);
    }

  if ( S_ISREG(stat_buf.st_mode) ) {
    // regular file
    switch (type) {
      case BX_FLOPPY_720K: // 720K 3.5"
        media->type              = BX_FLOPPY_720K;
        media->sectors_per_track = 9;
        media->tracks            = 80;
        media->heads             = 2;
        break;
      case BX_FLOPPY_1_2: // 1.2M 5.25"
        media->type              = BX_FLOPPY_1_2;
        media->sectors_per_track = 15;
        media->tracks            = 80;
        media->heads             = 2;
        break;
      case BX_FLOPPY_1_44: // 1.44M 3.5"
        media->type              = BX_FLOPPY_1_44;
        if (stat_buf.st_size <= 1474560) {
          media->sectors_per_track = 18;
          media->tracks            = 80;
          media->heads             = 2;
          }
        else if (stat_buf.st_size == 1720320) {
          media->sectors_per_track = 21;
          media->tracks            = 80;
          media->heads             = 2;
          }
	else if (stat_buf.st_size == 1763328) {
          media->sectors_per_track = 21;
          media->tracks            = 82;
          media->heads             = 2;
	  }
        else {
          fprintf(stderr, "# floppy: evaluate_media: file '%s' of unknown size %lu\n",
            path, (unsigned long) stat_buf.st_size);
          bx_printf("floppy: evaluate_media: file '%s' of unknown size %lu\n",
            path, (unsigned long) stat_buf.st_size);
          return(0);
          }
        break;
      case BX_FLOPPY_2_88: // 2.88M 3.5"
        media->type              = BX_FLOPPY_2_88;
        media->sectors_per_track = 36;
        media->tracks            = 80;
        media->heads             = 2;
        break;
      default:
        bx_panic("floppy: evaluate_media: unknown media type\n");
      }
    media->sectors = media->heads * media->tracks * media->sectors_per_track;
    return(1); // success
    }

  else if ( S_ISCHR(stat_buf.st_mode)
#if BX_WITH_MACOS == 0
#ifdef S_ISBLK
            || S_ISBLK(stat_buf.st_mode)
#endif
#endif
           ) {
    // character or block device
    // assume media is formatted to typical geometry for drive
    switch (type) {
      case BX_FLOPPY_720K: // 720K 3.5"
        media->type              = BX_FLOPPY_720K;
        media->sectors_per_track = 9;
        media->tracks            = 80;
        media->heads             = 2;
        break;
      case BX_FLOPPY_1_2: // 1.2M 5.25"
        media->type              = BX_FLOPPY_1_2;
        media->sectors_per_track = 15;
        media->tracks            = 80;
        media->heads             = 2;
        break;
      case BX_FLOPPY_1_44: // 1.44M 3.5"
        media->type              = BX_FLOPPY_1_44;
        media->sectors_per_track = 18;
        media->tracks            = 80;
        media->heads             = 2;
        break;
      case BX_FLOPPY_2_88: // 2.88M 3.5"
        media->type              = BX_FLOPPY_2_88;
        media->sectors_per_track = 36;
        media->tracks            = 80;
        media->heads             = 2;
        break;
      default:
        bx_panic("floppy: evaluate_media: unknown media type\n");
      }
    media->sectors = media->heads * media->tracks * media->sectors_per_track;
    return(1); // success
    }
  else {
    // unknown file type
    fprintf(stderr, "# floppy: unknown mode type\n");
    bx_printf("floppy: unknown mode type\n");
    return(0);
    }
}
