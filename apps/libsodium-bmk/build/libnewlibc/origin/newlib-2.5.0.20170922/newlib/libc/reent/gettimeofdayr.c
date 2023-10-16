/* Reentrant version of gettimeofday system call
   This implementation just calls the times/gettimeofday system calls.
   Gettimeofday may not be available on all targets.  It's presence
   here is dubious.  Consider it for internal use only.  */

#include <reent.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <_syslist.h>
#include <flexos/isolation.h>

/* Some targets provides their own versions of these functions.  Those
   targets should define REENTRANT_SYSCALLS_PROVIDED in TARGET_CFLAGS.  */

#ifdef _REENT_ONLY
#ifndef REENTRANT_SYSCALLS_PROVIDED
#define REENTRANT_SYSCALLS_PROVIDED
#endif
#endif

#ifdef REENTRANT_SYSCALLS_PROVIDED

int _dummy_gettimeofday_syscalls = 1;

#else

/* We use the errno variable used by the system dependent layer.  */
#undef errno
extern int errno;

/*
FUNCTION
	<<_gettimeofday_r>>---Reentrant version of gettimeofday

INDEX
	_gettimeofday_r

ANSI_SYNOPSIS
	#include <reent.h>
	#include <time.h>
	int _gettimeofday_r(struct _reent *<[ptr]>,
		struct timeval *<[ptimeval]>,
		void *<[ptimezone]>);

TRAD_SYNOPSIS
	#include <reent.h>
	#include <time.h>
	int _gettimeofday_r(<[ptr]>, <[ptimeval]>, <[ptimezone]>)
	struct _reent *<[ptr]>;
	struct timeval *<[ptimeval]>;
	void *<[ptimezone]>;

DESCRIPTION
	This is a reentrant version of <<gettimeofday>>.  It
	takes a pointer to the global data block, which holds
	<<errno>>.

	This function is only available for a few targets.
	Check libc.a to see if its available on yours.
*/

int
_DEFUN (_gettimeofday_r, (ptr, ptimeval, ptimezone),
     struct _reent *ptr _AND
     struct timeval *ptimeval _AND
     void *ptimezone)
{
  int ret;
  struct timeval _ptimeval;
  /* this argument is actually obsolete */
  UK_CTASSERT(ptimezone == NULL);

  errno = 0;
  flexos_nop_gate_r(0, 0, ret, _gettimeofday, &_ptimeval, NULL);
  ptimeval->tv_sec = _ptimeval.tv_sec;
  ptimeval->tv_usec = _ptimeval.tv_usec;
  if (ret == -1 && errno != 0)
    ptr->_errno = errno;
  return ret;
}

#endif /* ! defined (REENTRANT_SYSCALLS_PROVIDED) */
