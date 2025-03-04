/* Create descriptor for processing file.
   Copyright (C) 1998-2010, 2012, 2014, 2015, 2016 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <system.h>
#include "libelfP.h"
#include "common.h"

static size_t
get_shnum (void *map_address, unsigned char *e_ident, int fildes,
	   int64_t offset, size_t maxsize)
{
  size_t result;
  union
  {
    Elf32_Ehdr *e32;
    Elf64_Ehdr *e64;
    void *p;
  } ehdr;
  union
  {
    Elf32_Ehdr e32;
    Elf64_Ehdr e64;
  } ehdr_mem;
  bool is32 = e_ident[EI_CLASS] == ELFCLASS32;

  if ((is32 && maxsize < sizeof (Elf32_Ehdr))
      || (!is32 && maxsize < sizeof (Elf64_Ehdr)))
    {
       __libelf_seterrno (ELF_E_INVALID_ELF);
      return (size_t) -1l;
    }

  /* Make the ELF header available.  */
  if (e_ident[EI_DATA] == MY_ELFDATA
      && (ALLOW_UNALIGNED
	  || (((size_t) e_ident
	       & ((is32 ? __alignof__ (Elf32_Ehdr) : __alignof__ (Elf64_Ehdr))
		  - 1)) == 0)))
    ehdr.p = e_ident;
  else
    {
      /* We already read the ELF header.  We have to copy the header
	 since we possibly modify the data here and the caller
	 expects the memory it passes in to be preserved.  */
      ehdr.p = &ehdr_mem;

      if (is32)
	{
	  if (ALLOW_UNALIGNED)
	    {
	      ehdr_mem.e32.e_shnum = ((Elf32_Ehdr *) e_ident)->e_shnum;
	      ehdr_mem.e32.e_shoff = ((Elf32_Ehdr *) e_ident)->e_shoff;
	    }
	  else
	    memcpy (&ehdr_mem, e_ident, sizeof (Elf32_Ehdr));

	  if (e_ident[EI_DATA] != MY_ELFDATA)
	    {
	      CONVERT (ehdr_mem.e32.e_shnum);
	      CONVERT (ehdr_mem.e32.e_shoff);
	    }
	}
      else
	{
	  if (ALLOW_UNALIGNED)
	    {
	      ehdr_mem.e64.e_shnum = ((Elf64_Ehdr *) e_ident)->e_shnum;
	      ehdr_mem.e64.e_shoff = ((Elf64_Ehdr *) e_ident)->e_shoff;
	    }
	  else
	    memcpy (&ehdr_mem, e_ident, sizeof (Elf64_Ehdr));

	  if (e_ident[EI_DATA] != MY_ELFDATA)
	    {
	      CONVERT (ehdr_mem.e64.e_shnum);
	      CONVERT (ehdr_mem.e64.e_shoff);
	    }
	}
    }

  if (is32)
    {
      /* Get the number of sections from the ELF header.  */
      result = ehdr.e32->e_shnum;

      if (unlikely (result == 0) && ehdr.e32->e_shoff != 0)
	{
	  if (unlikely (ehdr.e32->e_shoff >= maxsize)
	      || unlikely (maxsize - ehdr.e32->e_shoff < sizeof (Elf32_Shdr)))
	    /* Cannot read the first section header.  */
	    return 0;

	  if (likely (map_address != NULL) && e_ident[EI_DATA] == MY_ELFDATA
	      && (ALLOW_UNALIGNED
		  || (((size_t) ((char *) map_address + ehdr.e32->e_shoff))
		      & (__alignof__ (Elf32_Shdr) - 1)) == 0))
	    /* We can directly access the memory.  */
	    result = ((Elf32_Shdr *) ((char *) map_address + ehdr.e32->e_shoff
				      + offset))->sh_size;
	  else
	    {
	      Elf32_Word size;
	      ssize_t r;

	      if (likely (map_address != NULL))
		/* gcc will optimize the memcpy to a simple memory
		   access while taking care of alignment issues.  */
		memcpy (&size, &((Elf32_Shdr *) ((char *) map_address
						 + ehdr.e32->e_shoff
						 + offset))->sh_size,
			sizeof (Elf32_Word));
	      else
		if (unlikely ((r = pread_retry (fildes, &size,
						sizeof (Elf32_Word),
						offset + ehdr.e32->e_shoff
						+ offsetof (Elf32_Shdr,
							    sh_size)))
			      != sizeof (Elf32_Word)))
		  {
		    if (r < 0)
		      __libelf_seterrno (ELF_E_INVALID_FILE);
		    else
		      __libelf_seterrno (ELF_E_INVALID_ELF);
		    return (size_t) -1l;
		  }

	      if (e_ident[EI_DATA] != MY_ELFDATA)
		CONVERT (size);

	      result = size;
	    }
	}

      /* If the section headers were truncated, pretend none were there.  */
      if (ehdr.e32->e_shoff > maxsize
	  || maxsize - ehdr.e32->e_shoff < sizeof (Elf32_Shdr) * result)
	result = 0;
    }
  else
    {
      /* Get the number of sections from the ELF header.  */
      result = ehdr.e64->e_shnum;

      if (unlikely (result == 0) && ehdr.e64->e_shoff != 0)
	{
	  if (unlikely (ehdr.e64->e_shoff >= maxsize)
	      || unlikely (ehdr.e64->e_shoff + sizeof (Elf64_Shdr) > maxsize))
	    /* Cannot read the first section header.  */
	    return 0;

	  Elf64_Xword size;
	  if (likely (map_address != NULL) && e_ident[EI_DATA] == MY_ELFDATA
	      && (ALLOW_UNALIGNED
		  || (((size_t) ((char *) map_address + ehdr.e64->e_shoff))
		      & (__alignof__ (Elf64_Shdr) - 1)) == 0))
	    /* We can directly access the memory.  */
	    size = ((Elf64_Shdr *) ((char *) map_address + ehdr.e64->e_shoff
				    + offset))->sh_size;
	  else
	    {
	      ssize_t r;
	      if (likely (map_address != NULL))
		/* gcc will optimize the memcpy to a simple memory
		   access while taking care of alignment issues.  */
		memcpy (&size, &((Elf64_Shdr *) ((char *) map_address
						 + ehdr.e64->e_shoff
						 + offset))->sh_size,
			sizeof (Elf64_Xword));
	      else
		if (unlikely ((r = pread_retry (fildes, &size,
						sizeof (Elf64_Xword),
						offset + ehdr.e64->e_shoff
						+ offsetof (Elf64_Shdr,
							    sh_size)))
			      != sizeof (Elf64_Xword)))
		  {
		    if (r < 0)
		      __libelf_seterrno (ELF_E_INVALID_FILE);
		    else
		      __libelf_seterrno (ELF_E_INVALID_ELF);
		    return (size_t) -1l;
		  }

	      if (e_ident[EI_DATA] != MY_ELFDATA)
		CONVERT (size);
	    }

	  /* Although sh_size is an Elf64_Xword and can contain a 64bit
	     value, we only expect an 32bit value max.  GElf_Word is
	     32bit unsigned.  */
	  if (size > ~((GElf_Word) 0))
	    {
	      /* Invalid value, it is too large.  */
	      __libelf_seterrno (ELF_E_INVALID_ELF);
	      return (size_t) -1l;
	    }

	  result = size;
	}

      /* If the section headers were truncated, pretend none were there.  */
      if (ehdr.e64->e_shoff > maxsize
	  || maxsize - ehdr.e64->e_shoff < sizeof (Elf64_Shdr) * result)
	result = 0;
    }

  return result;
}


/* Create descriptor for ELF file in memory.  */
static Elf *
file_read_elf (int fildes, void *map_address, unsigned char *e_ident,
	       int64_t offset, size_t maxsize, Elf_Cmd cmd, Elf *parent)
{
  /* Verify the binary is of the class we can handle.  */
  if (unlikely ((e_ident[EI_CLASS] != ELFCLASS32
		 && e_ident[EI_CLASS] != ELFCLASS64)
		/* We also can only handle two encodings.  */
		|| (e_ident[EI_DATA] != ELFDATA2LSB
		    && e_ident[EI_DATA] != ELFDATA2MSB)))
    {
      /* Cannot handle this.  */
      __libelf_seterrno (ELF_E_INVALID_ELF);
      return NULL;
    }

  /* Determine the number of sections.  Returns -1 and sets libelf errno
     if the file handle or elf file is invalid.  Returns zero if there
     are no section headers (or they cannot be read).  */
  size_t scncnt = get_shnum (map_address, e_ident, fildes, offset, maxsize);
  if (scncnt == (size_t) -1l)
    /* Could not determine the number of sections.  */
    return NULL;

  /* Check for too many sections.  */
  if (e_ident[EI_CLASS] == ELFCLASS32)
    {
      if (scncnt > SIZE_MAX / (sizeof (Elf_Scn) + sizeof (Elf32_Shdr)))
	{
	  __libelf_seterrno (ELF_E_INVALID_ELF);
	  return NULL;
	}
    }
  else if (scncnt > SIZE_MAX / (sizeof (Elf_Scn) + sizeof (Elf64_Shdr)))
    {
      __libelf_seterrno (ELF_E_INVALID_ELF);
      return NULL;
    }

  /* We can now allocate the memory.  Even if there are no section headers,
     we allocate space for a zeroth section in case we need it later.  */
  const size_t scnmax = (scncnt ? scncnt : (cmd == ELF_C_RDWR || cmd == ELF_C_RDWR_MMAP)
			 ? 1 : 0);
  Elf *elf = allocate_elf (fildes, map_address, offset, maxsize, cmd, parent,
			   ELF_K_ELF, scnmax * sizeof (Elf_Scn));
  if (elf == NULL)
    /* Not enough memory.  allocate_elf will have set libelf errno.  */
    return NULL;

  assert ((unsigned int) scncnt == scncnt);
  assert (offsetof (struct Elf, state.elf32.scns)
	  == offsetof (struct Elf, state.elf64.scns));
  elf->state.elf32.scns.cnt = scncnt;
  elf->state.elf32.scns.max = scnmax;

  /* Some more or less arbitrary value.  */
  elf->state.elf.scnincr = 10;

  /* Make the class easily available.  */
  elf->class = e_ident[EI_CLASS];

  if (e_ident[EI_CLASS] == ELFCLASS32)
    {
      /* This pointer might not be directly usable if the alignment is
	 not sufficient for the architecture.  */
      Elf32_Ehdr *ehdr = (Elf32_Ehdr *) ((char *) map_address + offset);

      /* This is a 32-bit binary.  */
      if (map_address != NULL && e_ident[EI_DATA] == MY_ELFDATA
	  && (ALLOW_UNALIGNED
	      || (((uintptr_t) ehdr) & (__alignof__ (Elf32_Ehdr) - 1)) == 0))
	{
	  /* We can use the mmapped memory.  */
	  elf->state.elf32.ehdr = ehdr;
	}
      else
	{
	  /* Copy the ELF header.  */
	  elf->state.elf32.ehdr = memcpy (&elf->state.elf32.ehdr_mem, e_ident,
					  sizeof (Elf32_Ehdr));

	  if (e_ident[EI_DATA] != MY_ELFDATA)
	    {
	      CONVERT (elf->state.elf32.ehdr_mem.e_type);
	      CONVERT (elf->state.elf32.ehdr_mem.e_machine);
	      CONVERT (elf->state.elf32.ehdr_mem.e_version);
	      CONVERT (elf->state.elf32.ehdr_mem.e_entry);
	      CONVERT (elf->state.elf32.ehdr_mem.e_phoff);
	      CONVERT (elf->state.elf32.ehdr_mem.e_shoff);
	      CONVERT (elf->state.elf32.ehdr_mem.e_flags);
	      CONVERT (elf->state.elf32.ehdr_mem.e_ehsize);
	      CONVERT (elf->state.elf32.ehdr_mem.e_phentsize);
	      CONVERT (elf->state.elf32.ehdr_mem.e_phnum);
	      CONVERT (elf->state.elf32.ehdr_mem.e_shentsize);
	      CONVERT (elf->state.elf32.ehdr_mem.e_shnum);
	      CONVERT (elf->state.elf32.ehdr_mem.e_shstrndx);
	    }
	}

      /* Don't precache the phdr pointer here.
	 elf32_getphdr will validate it against the size when asked.  */

      Elf32_Off e_shoff = elf->state.elf32.ehdr->e_shoff;
      if (map_address != NULL && e_ident[EI_DATA] == MY_ELFDATA
	  && cmd != ELF_C_READ_MMAP /* We need a copy to be able to write.  */
	  && (ALLOW_UNALIGNED
	      || (((uintptr_t) ((char *) ehdr + e_shoff)
		   & (__alignof__ (Elf32_Shdr) - 1)) == 0)))
	{
	  if (unlikely (scncnt > 0 && e_shoff >= maxsize)
	      || unlikely (maxsize - e_shoff
			   < scncnt * sizeof (Elf32_Shdr)))
	    {
	    free_and_out:
	      free (elf);
	      __libelf_seterrno (ELF_E_INVALID_ELF);
	      return NULL;
	    }
	  elf->state.elf32.shdr
	    = (Elf32_Shdr *) ((char *) ehdr + e_shoff);

	  for (size_t cnt = 0; cnt < scncnt; ++cnt)
	    {
	      elf->state.elf32.scns.data[cnt].index = cnt;
	      elf->state.elf32.scns.data[cnt].elf = elf;
	      elf->state.elf32.scns.data[cnt].shdr.e32 =
		&elf->state.elf32.shdr[cnt];
	      if (likely (elf->state.elf32.shdr[cnt].sh_offset < maxsize)
		  && likely (elf->state.elf32.shdr[cnt].sh_size
			     <= maxsize - elf->state.elf32.shdr[cnt].sh_offset))
		elf->state.elf32.scns.data[cnt].rawdata_base =
		  elf->state.elf32.scns.data[cnt].data_base =
		  ((char *) map_address + offset
		   + elf->state.elf32.shdr[cnt].sh_offset);
	      elf->state.elf32.scns.data[cnt].list = &elf->state.elf32.scns;

	      /* If this is a section with an extended index add a
		 reference in the section which uses the extended
		 index.  */
	      if (elf->state.elf32.shdr[cnt].sh_type == SHT_SYMTAB_SHNDX
		  && elf->state.elf32.shdr[cnt].sh_link < scncnt)
		elf->state.elf32.scns.data[elf->state.elf32.shdr[cnt].sh_link].shndx_index
		  = cnt;

	      /* Set the own shndx_index field in case it has not yet
		 been set.  */
	      if (elf->state.elf32.scns.data[cnt].shndx_index == 0)
		elf->state.elf32.scns.data[cnt].shndx_index = -1;
	    }
	}
      else
	{
	  for (size_t cnt = 0; cnt < scncnt; ++cnt)
	    {
	      elf->state.elf32.scns.data[cnt].index = cnt;
	      elf->state.elf32.scns.data[cnt].elf = elf;
	      elf->state.elf32.scns.data[cnt].list = &elf->state.elf32.scns;
	    }
	}

      /* So far only one block with sections.  */
      elf->state.elf32.scns_last = &elf->state.elf32.scns;
    }
  else
    {
      /* This pointer might not be directly usable if the alignment is
	 not sufficient for the architecture.  */
      Elf64_Ehdr *ehdr = (Elf64_Ehdr *) ((char *) map_address + offset);

      /* This is a 64-bit binary.  */
      if (map_address != NULL && e_ident[EI_DATA] == MY_ELFDATA
	  && (ALLOW_UNALIGNED
	      || (((uintptr_t) ehdr) & (__alignof__ (Elf64_Ehdr) - 1)) == 0))
	{
	  /* We can use the mmapped memory.  */
	  elf->state.elf64.ehdr = ehdr;
	}
      else
	{
	  /* Copy the ELF header.  */
	  elf->state.elf64.ehdr = memcpy (&elf->state.elf64.ehdr_mem, e_ident,
					  sizeof (Elf64_Ehdr));

	  if (e_ident[EI_DATA] != MY_ELFDATA)
	    {
	      CONVERT (elf->state.elf64.ehdr_mem.e_type);
	      CONVERT (elf->state.elf64.ehdr_mem.e_machine);
	      CONVERT (elf->state.elf64.ehdr_mem.e_version);
	      CONVERT (elf->state.elf64.ehdr_mem.e_entry);
	      CONVERT (elf->state.elf64.ehdr_mem.e_phoff);
	      CONVERT (elf->state.elf64.ehdr_mem.e_shoff);
	      CONVERT (elf->state.elf64.ehdr_mem.e_flags);
	      CONVERT (elf->state.elf64.ehdr_mem.e_ehsize);
	      CONVERT (elf->state.elf64.ehdr_mem.e_phentsize);
	      CONVERT (elf->state.elf64.ehdr_mem.e_phnum);
	      CONVERT (elf->state.elf64.ehdr_mem.e_shentsize);
	      CONVERT (elf->state.elf64.ehdr_mem.e_shnum);
	      CONVERT (elf->state.elf64.ehdr_mem.e_shstrndx);
	    }
	}

      /* Don't precache the phdr pointer here.
	 elf64_getphdr will validate it against the size when asked.  */

      Elf64_Off e_shoff = elf->state.elf64.ehdr->e_shoff;
      if (map_address != NULL && e_ident[EI_DATA] == MY_ELFDATA
	  && cmd != ELF_C_READ_MMAP /* We need a copy to be able to write.  */
	  && (ALLOW_UNALIGNED
	      || (((uintptr_t) ((char *) ehdr + e_shoff)
		   & (__alignof__ (Elf64_Shdr) - 1)) == 0)))
	{
	  if (unlikely (scncnt > 0 && e_shoff >= maxsize)
	      || unlikely (maxsize - e_shoff
			   < scncnt * sizeof (Elf64_Shdr)))
	    goto free_and_out;
	  elf->state.elf64.shdr
	    = (Elf64_Shdr *) ((char *) ehdr + e_shoff);

	  for (size_t cnt = 0; cnt < scncnt; ++cnt)
	    {
	      elf->state.elf64.scns.data[cnt].index = cnt;
	      elf->state.elf64.scns.data[cnt].elf = elf;
	      elf->state.elf64.scns.data[cnt].shdr.e64 =
		&elf->state.elf64.shdr[cnt];
	      if (likely (elf->state.elf64.shdr[cnt].sh_offset < maxsize)
		  && likely (elf->state.elf64.shdr[cnt].sh_size
			     <= maxsize - elf->state.elf64.shdr[cnt].sh_offset))
		elf->state.elf64.scns.data[cnt].rawdata_base =
		  elf->state.elf64.scns.data[cnt].data_base =
		  ((char *) map_address + offset
		   + elf->state.elf64.shdr[cnt].sh_offset);
	      elf->state.elf64.scns.data[cnt].list = &elf->state.elf64.scns;

	      /* If this is a section with an extended index add a
		 reference in the section which uses the extended
		 index.  */
	      if (elf->state.elf64.shdr[cnt].sh_type == SHT_SYMTAB_SHNDX
		  && elf->state.elf64.shdr[cnt].sh_link < scncnt)
		elf->state.elf64.scns.data[elf->state.elf64.shdr[cnt].sh_link].shndx_index
		  = cnt;

	      /* Set the own shndx_index field in case it has not yet
		 been set.  */
	      if (elf->state.elf64.scns.data[cnt].shndx_index == 0)
		elf->state.elf64.scns.data[cnt].shndx_index = -1;
	    }
	}
      else
	{
	  for (size_t cnt = 0; cnt < scncnt; ++cnt)
	    {
	      elf->state.elf64.scns.data[cnt].index = cnt;
	      elf->state.elf64.scns.data[cnt].elf = elf;
	      elf->state.elf64.scns.data[cnt].list = &elf->state.elf64.scns;
	    }
	}

      /* So far only one block with sections.  */
      elf->state.elf64.scns_last = &elf->state.elf64.scns;
    }

  return elf;
}


Elf *
internal_function
__libelf_read_mmaped_file (int fildes, void *map_address,  int64_t offset,
			   size_t maxsize, Elf_Cmd cmd, Elf *parent)
{
  /* We have to find out what kind of file this is.  We handle ELF
     files and archives.  To find out what we have we must look at the
     header.  The header for an ELF file is EI_NIDENT bytes in size,
     the header for an archive file SARMAG bytes long.  */
  unsigned char *e_ident = (unsigned char *) map_address + offset;

  /* See what kind of object we have here.  */
  Elf_Kind kind = determine_kind (e_ident, maxsize);

  switch (kind)
    {
    case ELF_K_ELF:
      return file_read_elf (fildes, map_address, e_ident, offset, maxsize,
			    cmd, parent);

    default:
      break;
    }

  /* This case is easy.  Since we cannot do anything with this file
     create a dummy descriptor.  */
  return allocate_elf (fildes, map_address, offset, maxsize, cmd, parent,
		       ELF_K_NONE, 0);
}


static Elf *
read_unmmaped_file (int fildes, int64_t offset, size_t maxsize, Elf_Cmd cmd,
		    Elf *parent)
{
  /* We have to find out what kind of file this is.  We handle ELF
     files and archives.  To find out what we have we must read the
     header.  The identification header for an ELF file is EI_NIDENT
     bytes in size, but we read the whole ELF header since we will
     need it anyway later.  For archives the header in SARMAG bytes
     long.  Read the maximum of these numbers.

     XXX We have to change this for the extended `ar' format some day.

     Use a union to ensure alignment.  We might later access the
     memory as a ElfXX_Ehdr.  */
  union
  {
    Elf64_Ehdr ehdr;
    unsigned char header[sizeof (Elf64_Ehdr)];
  } mem;

  /* Read the head of the file.  */
  ssize_t nread = pread_retry (fildes, mem.header,
			       MIN (sizeof (Elf64_Ehdr),
				    maxsize),
			       offset);
  if (unlikely (nread == -1))
    {
      /* We cannot even read the head of the file.  Maybe FILDES is associated
	 with an unseekable device.  This is nothing we can handle.  */
      __libelf_seterrno (ELF_E_INVALID_FILE);
      return NULL;
    }

  /* See what kind of object we have here.  */
  Elf_Kind kind = determine_kind (mem.header, nread);

  switch (kind)
    {
    case ELF_K_ELF:
      /* Make sure at least the ELF header is contained in the file.  */
      if ((size_t) nread >= (mem.header[EI_CLASS] == ELFCLASS32
			     ? sizeof (Elf32_Ehdr) : sizeof (Elf64_Ehdr)))
	return file_read_elf (fildes, NULL, mem.header, offset, maxsize, cmd,
			      parent);
      FALLTHROUGH;

    default:
      break;
    }

  /* This case is easy.  Since we cannot do anything with this file
     create a dummy descriptor.  */
  return allocate_elf (fildes, NULL, offset, maxsize, cmd, parent,
		       ELF_K_NONE, 0);
}


/* Open a file for reading.  If possible we will try to mmap() the file.  */
static struct Elf *
read_file (int fildes, int64_t offset, size_t maxsize,
	   Elf_Cmd cmd, Elf *parent)
{
  void *map_address = NULL;
  int use_mmap = (cmd == ELF_C_READ_MMAP || cmd == ELF_C_RDWR_MMAP
		  || cmd == ELF_C_WRITE_MMAP
		  || cmd == ELF_C_READ_MMAP_PRIVATE);

  if (parent == NULL)
    {
      if (maxsize == ~((size_t) 0))
	{
	  /* We don't know in the moment how large the file is.
	     Determine it now.  */
	  struct stat st;

	  if (fstat (fildes, &st) == 0
	      && (sizeof (size_t) >= sizeof (st.st_size)
		  || st.st_size <= ~((size_t) 0)))
	    maxsize = (size_t) st.st_size;
	}
    }
  else
    {
      /* The parent is already loaded.  Use it.  */
      assert (maxsize != ~((size_t) 0));
    }

  if (use_mmap)
    {
      if (parent == NULL)
	{
	  /* We try to map the file ourself.  */
	  map_address = mmap (NULL, maxsize, (cmd == ELF_C_READ_MMAP
					      ? PROT_READ
					      : PROT_READ|PROT_WRITE),
			      cmd == ELF_C_READ_MMAP_PRIVATE
			      || cmd == ELF_C_READ_MMAP
			      ? MAP_PRIVATE : MAP_SHARED,
			      fildes, offset);

	  if (map_address == MAP_FAILED)
	    map_address = NULL;
	}
      else
	{
	  map_address = parent->map_address;
	}
    }

  /* If we have the file in memory optimize the access.  */
  if (map_address != NULL)
    {
      assert (map_address != MAP_FAILED);

      struct Elf *result = __libelf_read_mmaped_file (fildes, map_address,
						      offset, maxsize, cmd,
						      parent);

      /* If something went wrong during the initialization unmap the
	 memory if we mmaped here.  */
      if (result == NULL
	  && (parent == NULL
	      || parent->map_address != map_address))
	munmap (map_address, maxsize);
      else if (parent == NULL)
	/* Remember that we mmap()ed the memory.  */
	result->flags |= ELF_F_MMAPPED;

      return result;
    }

  /* Otherwise we have to do it the hard way.  We read as much as necessary
     from the file whenever we need information which is not available.  */
  return read_unmmaped_file (fildes, offset, maxsize, cmd, parent);
}


/* We were asked to return a clone of an existing descriptor.  This
   function must be called with the lock on the parent descriptor
   being held. */
static Elf *
dup_elf (int fildes, Elf_Cmd cmd, Elf *ref)
{
  if (fildes == -1)
    /* Allow the user to pass -1 as the file descriptor for the new file.  */
    fildes = ref->fildes;
  /* The file descriptor better should be the same.  If it was disconnected
     already (using `elf_cntl') we do not test it.  */
  else if (unlikely (ref->fildes != -1 && fildes != ref->fildes))
    {
      __libelf_seterrno (ELF_E_FD_MISMATCH);
      return NULL;
    }

  /* The mode must allow reading.  I.e., a descriptor creating with a
     command different then ELF_C_READ, ELF_C_WRITE and ELF_C_RDWR is
     not allowed.  */
  if (unlikely (ref->cmd != ELF_C_READ && ref->cmd != ELF_C_READ_MMAP
		&& ref->cmd != ELF_C_WRITE && ref->cmd != ELF_C_WRITE_MMAP
		&& ref->cmd != ELF_C_RDWR && ref->cmd != ELF_C_RDWR_MMAP
		&& ref->cmd != ELF_C_READ_MMAP_PRIVATE))
    {
      __libelf_seterrno (ELF_E_INVALID_OP);
      return NULL;
    }

  /* Now it is time to distinguish between reading normal files and
     archives.  Normal files can easily be handled be incrementing the
     reference counter and return the same descriptor.  */
	++ref->ref_count;
	return ref;
}


/* Return descriptor for empty file ready for writing.  */
static struct Elf *
write_file (int fd, Elf_Cmd cmd)
{
  /* We simply create an empty `Elf' structure.  */
#define NSCNSALLOC	10
  Elf *result = allocate_elf (fd, NULL, 0, 0, cmd, NULL, ELF_K_ELF,
			      NSCNSALLOC * sizeof (Elf_Scn));

  if (result != NULL)
    {
      /* We have to write to the file in any case.  */
      result->flags = ELF_F_DIRTY;

      /* Some more or less arbitrary value.  */
      result->state.elf.scnincr = NSCNSALLOC;

      /* We have allocated room for some sections.  */
      assert (offsetof (struct Elf, state.elf32.scns)
	      == offsetof (struct Elf, state.elf64.scns));
      result->state.elf.scns_last = &result->state.elf32.scns;
      result->state.elf32.scns.max = NSCNSALLOC;
    }

  return result;
}

/* Lock if necessary before dup an archive.  */
static inline Elf *
lock_dup_elf (int fildes, Elf_Cmd cmd, Elf *ref)
{
  /* We need wrlock to dup an archive.  */
  if (ref->kind == ELF_K_AR)
    {
      rwlock_unlock (ref->lock);
      rwlock_wrlock (ref->lock);
    }
    /* Duplicate the descriptor.  */
  return dup_elf (fildes, cmd, ref);
}

/* Return a descriptor for the file belonging to FILDES.  */
Elf *
elf_begin (int fildes, Elf_Cmd cmd, Elf *ref)
{
  Elf *retval;

  if (unlikely (__libelf_version != EV_CURRENT))
    {
      /* Version wasn't set so far.  */
      __libelf_seterrno (ELF_E_NO_VERSION);
      return NULL;
    }

  if (ref != NULL)
    /* Make sure the descriptor is not suddenly going away.  */
    rwlock_rdlock (ref->lock);
  else if (unlikely (!is_valid_fd(fildes) && errno == EBADF))
    {
      /* We cannot do anything productive without a file descriptor.  */
      __libelf_seterrno (ELF_E_INVALID_FILE);
      return NULL;
    }

  switch (cmd)
    {
    case ELF_C_NULL:
      /* We simply return a NULL pointer.  */
      retval = NULL;
      break;

    case ELF_C_READ_MMAP_PRIVATE:
      /* If we have a reference it must also be opened this way.  */
      if (unlikely (ref != NULL && ref->cmd != ELF_C_READ_MMAP_PRIVATE))
	{
	  __libelf_seterrno (ELF_E_INVALID_CMD);
	  retval = NULL;
	  break;
	}
      FALLTHROUGH;

    case ELF_C_READ:
    case ELF_C_READ_MMAP:
      if (ref != NULL)
	retval = lock_dup_elf (fildes, cmd, ref);
      else
	/* Create descriptor for existing file.  */
	retval = read_file (fildes, 0, ~((size_t) 0), cmd, NULL);
      break;

    case ELF_C_RDWR:
    case ELF_C_RDWR_MMAP:
      /* If we have a REF object it must also be opened using this
	 command.  */
      if (ref != NULL)
	{
	  if (unlikely (ref->cmd != ELF_C_RDWR && ref->cmd != ELF_C_RDWR_MMAP
			&& ref->cmd != ELF_C_WRITE
			&& ref->cmd != ELF_C_WRITE_MMAP))
	    {
	      /* This is not ok.  REF must also be opened for writing.  */
	      __libelf_seterrno (ELF_E_INVALID_CMD);
	      retval = NULL;
	    }
	  else
	    retval = lock_dup_elf (fildes, cmd, ref);
	}
      else
	/* Create descriptor for existing file.  */
	retval = read_file (fildes, 0, ~((size_t) 0), cmd, NULL);
      break;

    case ELF_C_WRITE:
    case ELF_C_WRITE_MMAP:
      /* We ignore REF and prepare a descriptor to write a new file.  */
      retval = write_file (fildes, cmd);
      break;

    default:
      __libelf_seterrno (ELF_E_INVALID_CMD);
      retval = NULL;
      break;
    }

  /* Release the lock.  */
  if (ref != NULL)
    rwlock_unlock (ref->lock);

  return retval;
}
INTDEF(elf_begin)
