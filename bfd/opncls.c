/* opncls.c -- open and close a BFD.
   Copyright (C) 1990 91, 92, 93, 94 Free Software Foundation, Inc.
   Written by Cygnus Support.

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "obstack.h"
extern void bfd_cache_init PARAMS ((bfd *));
FILE *bfd_open_file PARAMS ((bfd *));

/* fdopen is a loser -- we should use stdio exclusively.  Unfortunately
   if we do that we can't use fcntl.  */


#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

/* Return a new BFD.  All BFD's are allocated through this routine.  */

bfd *
_bfd_new_bfd ()
{
  bfd *nbfd;

  nbfd = (bfd *)bfd_zmalloc (sizeof (bfd));
  if (!nbfd)
    {
      bfd_error = no_memory;
      return 0;
    }

  bfd_check_init();
  if (!obstack_begin(&nbfd->memory, 128))
    {
      bfd_error = no_memory;
      return 0;
    }

  nbfd->arch_info = &bfd_default_arch_struct;

  nbfd->direction = no_direction;
  nbfd->iostream = NULL;
  nbfd->where = 0;
  nbfd->sections = (asection *)NULL;
  nbfd->format = bfd_unknown;
  nbfd->my_archive = (bfd *)NULL;
  nbfd->origin = 0;				
  nbfd->opened_once = false;
  nbfd->output_has_begun = false;
  nbfd->section_count = 0;
  nbfd->usrdata = (PTR)NULL;
  nbfd->cacheable = false;
  nbfd->flags = NO_FLAGS;
  nbfd->mtime_set = false;

  return nbfd;
}

/* Allocate a new BFD as a member of archive OBFD.  */

bfd *
_bfd_new_bfd_contained_in (obfd)
     bfd *obfd;
{
  bfd *nbfd;

  nbfd = _bfd_new_bfd();
  nbfd->xvec = obfd->xvec;
  nbfd->my_archive = obfd;
  nbfd->direction = read_direction;
  nbfd->target_defaulted = obfd->target_defaulted;
  return nbfd;
}

/*
SECTION
	Opening and closing BFDs

*/

/*
FUNCTION
	bfd_openr

SYNOPSIS
        bfd *bfd_openr(CONST char *filename, CONST char *target);

DESCRIPTION
	Open the file @var{filename} (using <<fopen>>) with the target
	@var{target}.  Return a pointer to the created BFD.

	Calls <<bfd_find_target>>, so @var{target} is interpreted as by
	that function.

	If <<NULL>> is returned then an error has occured.   Possible errors
	are <<no_memory>>, <<invalid_target>> or <<system_call>> error.
*/

bfd *
bfd_openr (filename, target)
     CONST char *filename;
     CONST char *target;
{
  bfd *nbfd;
  bfd_target *target_vec;

  nbfd = _bfd_new_bfd();
  if (nbfd == NULL) {
    bfd_error = no_memory;
    return NULL;
  }

  target_vec = bfd_find_target (target, nbfd);
  if (target_vec == NULL) {
    bfd_error = invalid_target;
    return NULL;
  }

  nbfd->filename = filename;
  nbfd->direction = read_direction;

  if (bfd_open_file (nbfd) == NULL) {
    bfd_error = system_call_error;	/* File didn't exist, or some such */
    bfd_release(nbfd,0);
    return NULL;
  }
  return nbfd;
}


/* Don't try to `optimize' this function:

   o - We lock using stack space so that interrupting the locking
       won't cause a storage leak.
   o - We open the file stream last, since we don't want to have to
       close it if anything goes wrong.  Closing the stream means closing
       the file descriptor too, even though we didn't open it.
 */
/*
FUNCTION
         bfd_fdopenr

SYNOPSIS
         bfd *bfd_fdopenr(CONST char *filename, CONST char *target, int fd);

DESCRIPTION
         <<bfd_fdopenr>> is to <<bfd_fopenr>> much like <<fdopen>> is to <<fopen>>.
	 It opens a BFD on a file already described by the @var{fd}
	 supplied.

	 When the file is later <<bfd_close>>d, the file descriptor will be closed.

	 If the caller desires that this file descriptor be cached by BFD
	 (opened as needed, closed as needed to free descriptors for
	 other opens), with the supplied @var{fd} used as an initial
	 file descriptor (but subject to closure at any time), call
	 bfd_set_cacheable(bfd, 1) on the returned BFD.  The default is to
	 assume no cacheing; the file descriptor will remain open until
	 <<bfd_close>>, and will not be affected by BFD operations on other
	 files.

         Possible errors are <<no_memory>>, <<invalid_target>> and <<system_call_error>>.
*/

bfd *
bfd_fdopenr (filename, target, fd)
     CONST char *filename;
     CONST char *target;
     int fd;
{
  bfd *nbfd;
  bfd_target *target_vec;
  int fdflags;

  bfd_error = system_call_error;

#ifdef NO_FCNTL
  fdflags = O_RDWR;			/* Assume full access */
#else
  fdflags = fcntl (fd, F_GETFL, NULL);
#endif
  if (fdflags == -1) return NULL;

  nbfd = _bfd_new_bfd();

  if (nbfd == NULL) {
    bfd_error = no_memory;
    return NULL;
  }

  target_vec = bfd_find_target (target, nbfd);
  if (target_vec == NULL) {
    bfd_error = invalid_target;
    return NULL;
  }
#if defined(VMS) || defined(__GO32__)
  nbfd->iostream = (char *)fopen(filename, FOPEN_RB);
#else
  /* (O_ACCMODE) parens are to avoid Ultrix header file bug */
  switch (fdflags & (O_ACCMODE)) {
  case O_RDONLY: nbfd->iostream = (char *) fdopen (fd, FOPEN_RB);   break;
  case O_WRONLY: nbfd->iostream = (char *) fdopen (fd, FOPEN_RUB);  break;
  case O_RDWR:   nbfd->iostream = (char *) fdopen (fd, FOPEN_RUB);  break;
  default: abort ();
  }
#endif
  if (nbfd->iostream == NULL) {
    (void) obstack_free (&nbfd->memory, (PTR)0);
    return NULL;
  }

  /* OK, put everything where it belongs */

  nbfd->filename = filename;

  /* As a special case we allow a FD open for read/write to
     be written through, although doing so requires that we end
     the previous clause with a preposition.  */
  /* (O_ACCMODE) parens are to avoid Ultrix header file bug */
  switch (fdflags & (O_ACCMODE)) {
  case O_RDONLY: nbfd->direction = read_direction; break;
  case O_WRONLY: nbfd->direction = write_direction; break;
  case O_RDWR: nbfd->direction = both_direction; break;
  default: abort ();
  }
				
  bfd_cache_init (nbfd);

  return nbfd;
}

/** bfd_openw -- open for writing.
  Returns a pointer to a freshly-allocated BFD on success, or NULL.

  See comment by bfd_fdopenr before you try to modify this function. */

/*
FUNCTION
	bfd_openw

SYNOPSIS
	bfd *bfd_openw(CONST char *filename, CONST char *target);

DESCRIPTION
	Create a BFD, associated with file @var{filename}, using the
	file format @var{target}, and return a pointer to it.

	Possible errors are <<system_call_error>>, <<no_memory>>,
	<<invalid_target>>.
*/

bfd *
bfd_openw (filename, target)
     CONST char *filename;
     CONST char *target;
{
  bfd *nbfd;
  bfd_target *target_vec;

  bfd_error = system_call_error;

  /* nbfd has to point to head of malloc'ed block so that bfd_close may
     reclaim it correctly. */

  nbfd = _bfd_new_bfd();
  if (nbfd == NULL) {
    bfd_error = no_memory;
    return NULL;
  }

  target_vec = bfd_find_target (target, nbfd);
  if (target_vec == NULL) return NULL;

  nbfd->filename = filename;
  nbfd->direction = write_direction;

  if (bfd_open_file (nbfd) == NULL) {
    bfd_error = system_call_error;	/* File not writeable, etc */
    (void) obstack_free (&nbfd->memory, (PTR)0);
    return NULL;
  }
  return nbfd;
}

/*

FUNCTION
	bfd_close

SYNOPSIS
	boolean bfd_close(bfd *abfd);

DESCRIPTION

	Close a BFD. If the BFD was open for writing,
	then pending operations are completed and the file written out
	and closed. If the created file is executable, then
	<<chmod>> is called to mark it as such.

	All memory attached to the BFD's obstacks is released.

	The file descriptor associated with the BFD is closed (even
	if it was passed in to BFD by <<bfd_fdopenr>>).

RETURNS
	<<true>> is returned if all is ok, otherwise <<false>>.
*/


boolean
bfd_close (abfd)
     bfd *abfd;
{
  boolean ret;

  if (!bfd_read_p(abfd))
    if (BFD_SEND_FMT (abfd, _bfd_write_contents, (abfd)) != true)
      return false;

  if (BFD_SEND (abfd, _close_and_cleanup, (abfd)) != true) return false;

  ret = bfd_cache_close(abfd);

  /* If the file was open for writing and is now executable,
     make it so */
  if (ret == true
      && abfd->direction == write_direction
      && abfd->flags & EXEC_P) {
    struct stat buf;
    stat(abfd->filename, &buf);
#ifndef S_IXUSR
#define S_IXUSR 0100	/* Execute by owner.  */
#endif
#ifndef S_IXGRP
#define S_IXGRP 0010	/* Execute by group.  */
#endif
#ifndef S_IXOTH
#define S_IXOTH 0001	/* Execute by others.  */
#endif

    chmod(abfd->filename, 0777  & (buf.st_mode | S_IXUSR | S_IXGRP | S_IXOTH));
  }
  (void) obstack_free (&abfd->memory, (PTR)0);
  (void) free(abfd);
  return ret;
}

/*
FUNCTION
	bfd_close_all_done

SYNOPSIS
	boolean bfd_close_all_done(bfd *);

DESCRIPTION
	Close a BFD.  Differs from <<bfd_close>>
	since it does not complete any pending operations.  This
	routine would be used if the application had just used BFD for
	swapping and didn't want to use any of the writing code.

	If the created file is executable, then <<chmod>> is called
	to mark it as such.

	All memory attached to the BFD's obstacks is released.

RETURNS
	<<true>> is returned if all is ok, otherwise <<false>>.

*/

boolean
bfd_close_all_done (abfd)
     bfd *abfd;
{
  boolean ret;

  ret = bfd_cache_close(abfd);

  /* If the file was open for writing and is now executable,
     make it so */
  if (ret == true
      && abfd->direction == write_direction
      && abfd->flags & EXEC_P) {
    struct stat buf;
    stat(abfd->filename, &buf);
#ifndef S_IXUSR
#define S_IXUSR 0100	/* Execute by owner.  */
#endif
#ifndef S_IXGRP
#define S_IXGRP 0010	/* Execute by group.  */
#endif
#ifndef S_IXOTH
#define S_IXOTH 0001	/* Execute by others.  */
#endif

    chmod(abfd->filename, 0x777 &(buf.st_mode | S_IXUSR | S_IXGRP | S_IXOTH));
  }
  (void) obstack_free (&abfd->memory, (PTR)0);
  (void) free(abfd);
  return ret;
}


/*
FUNCTION	
	bfd_alloc_size

SYNOPSIS
	bfd_size_type bfd_alloc_size(bfd *abfd);

DESCRIPTION
        Return the number of bytes in the obstacks connected to @var{abfd}.

*/

bfd_size_type
bfd_alloc_size (abfd)
     bfd *abfd;
{
  struct _obstack_chunk *chunk = abfd->memory.chunk;
  size_t size = 0;
  while (chunk) {
    size += chunk->limit - &(chunk->contents[0]);
    chunk = chunk->prev;
  }
  return size;
}



/*
FUNCTION
	bfd_create

SYNOPSIS
	bfd *bfd_create(CONST char *filename, bfd *templ);

DESCRIPTION
	Create a new BFD in the manner of
	<<bfd_openw>>, but without opening a file. The new BFD
	takes the target from the target used by @var{template}. The
	format is always set to <<bfd_object>>.

*/

bfd *
bfd_create (filename, templ)
     CONST char *filename;
     bfd *templ;
{
  bfd *nbfd = _bfd_new_bfd();
  if (nbfd == (bfd *)NULL) {
    bfd_error = no_memory;
    return (bfd *)NULL;
  }
  nbfd->filename = filename;
  if(templ) {
    nbfd->xvec = templ->xvec;
  }
  nbfd->direction = no_direction;
  bfd_set_format(nbfd, bfd_object);
  return nbfd;
}

/*
INTERNAL_FUNCTION
	bfd_alloc_by_size_t

SYNOPSIS
	PTR bfd_alloc_by_size_t(bfd *abfd, size_t wanted);

DESCRIPTION
	Allocate a block of @var{wanted} bytes of memory in the obstack
	attatched to <<abfd>> and return a pointer to it.
*/


PTR
bfd_alloc_by_size_t (abfd, size)
     bfd *abfd;
     size_t size;
{
  return obstack_alloc(&(abfd->memory), size);
}

void
bfd_alloc_grow (abfd, ptr, size)
     bfd *abfd;
     PTR ptr;
     size_t size;
{
  (void) obstack_grow(&(abfd->memory), ptr, size);
}

PTR
bfd_alloc_finish (abfd)
     bfd *abfd;
{
  return obstack_finish(&(abfd->memory));
}

PTR
bfd_alloc (abfd, size)
     bfd *abfd;
     size_t size;
{
  return bfd_alloc_by_size_t(abfd, (size_t)size);
}

PTR
bfd_zalloc (abfd, size)
     bfd *abfd;
     size_t size;
{
  PTR res;
  res = bfd_alloc(abfd, size);
  if (res)
    memset(res, 0, (size_t)size);
  return res;
}

PTR
bfd_realloc (abfd, old, size)
     bfd *abfd;
     PTR old;
     size_t size;
{
  PTR res = bfd_alloc(abfd, size);
  if (res)
    memcpy(res, old, (size_t)size);
  return res;
}
