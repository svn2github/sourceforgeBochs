/////////////////////////////////////////////////////////////////////////
// $Id$
/////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2002,2003  Mike Nordell
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

#ifndef BX_IODEV_PCIVGA_H
#define BX_IODEV_PCIVGA_H

#if BX_USE_PCIVGA_SMF
#  define BX_PCIVGA_THIS thePciVgaAdapter->
#  define BX_PCIVGA_SMF static
#else
#  define BX_PCIVGA_THIS this->
#  define BX_PCIVGA_SMF
#endif

class bx_pcivga_c : public bx_devmodel_c, public bx_pci_device_stub_c {
public:
  bx_pcivga_c();
  virtual ~bx_pcivga_c();
  virtual void init(void);
  virtual void reset(unsigned type);
  virtual void register_state(void);
  virtual void after_restore_state(void);

  BX_PCIVGA_SMF bx_bool mem_read_handler(bx_phy_address addr, unsigned len, void *data, void *param);
  BX_PCIVGA_SMF bx_bool mem_write_handler(bx_phy_address addr, unsigned len, void *data, void *param);

  virtual Bit32u pci_read_handler(Bit8u address, unsigned io_len);
  virtual void   pci_write_handler(Bit8u address, Bit32u value, unsigned io_len);

private:
  bx_bool vbe_present;
};

#endif
