/* Hash table for DWARF .debug_types section content.
   Copyright (C) 2010 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifndef _DWARF_SIG8_HASH_H
#define _DWARF_SIG8_HASH_H	1

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "libdw.h"

#define NAME Dwarf_Sig8_Hash
#define TYPE struct Dwarf_CU *

#include <dynamicsizehash.h>

#endif	/* dwarf_sig8_hash.h */
