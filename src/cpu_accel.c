/*
 * cpu_accel.c
 * Copyright (C) 2000-2002 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <inttypes.h>

#include "mm_accel.h"

#ifdef ARCH_X86
static inline uint32_t arch_accel (void)
{
    uint32_t eax, ebx, ecx, edx;
    int AMD;
    uint32_t caps;

#ifndef PIC
#define cpuid(op,eax,ebx,ecx,edx)	\
    __asm__ ("cpuid"			\
	     : "=a" (eax),		\
	       "=b" (ebx),		\
	       "=c" (ecx),		\
	       "=d" (edx)		\
	     : "a" (op)			\
	     : "cc")
#else	/* PIC version : save ebx */
#define cpuid(op,eax,ebx,ecx,edx)	\
    __asm__ ("pushl %%ebx\n\t"		\
	     "cpuid\n\t"		\
	     "movl %%ebx,%1\n\t"	\
	     "popl %%ebx"		\
	     : "=a" (eax),		\
	       "=r" (ebx),		\
	       "=c" (ecx),		\
	       "=d" (edx)		\
	     : "a" (op)			\
	     : "cc")
#endif

    __asm__ ("pushfl\n\t"
	     "pushfl\n\t"
	     "popl %0\n\t"
	     "movl %0,%1\n\t"
	     "xorl $0x200000,%0\n\t"
	     "pushl %0\n\t"
	     "popfl\n\t"
	     "pushfl\n\t"
	     "popl %0\n\t"
	     "popfl"
	     : "=r" (eax),
	       "=r" (ebx)
	     :
	     : "cc");

    if (eax == ebx)		/* no cpuid */
	return 0;

    cpuid (0x00000000, eax, ebx, ecx, edx);
    if (!eax)			/* vendor string only */
	return 0;

    AMD = (ebx == 0x68747541) && (ecx == 0x444d4163) && (edx == 0x69746e65);

    cpuid (0x00000001, eax, ebx, ecx, edx);
    if (! (edx & 0x00800000))	/* no MMX */
	return 0;

    caps = MM_ACCEL_X86_MMX;
    if (edx & 0x02000000)	/* SSE - identical to AMD MMX extensions */
	caps = MM_ACCEL_X86_MMX | MM_ACCEL_X86_MMXEXT;

    cpuid (0x80000000, eax, ebx, ecx, edx);
    if (eax < 0x80000001)	/* no extended capabilities */
	return caps;

    cpuid (0x80000001, eax, ebx, ecx, edx);

    if (edx & 0x80000000)
	caps |= MM_ACCEL_X86_3DNOW;

    if (AMD && (edx & 0x00400000))	/* AMD MMX extensions */
	caps |= MM_ACCEL_X86_MMXEXT;

    return caps;
}
#endif /* ARCH_X86 */

#ifdef ARCH_PPC
#include <signal.h>
#include <setjmp.h>

static sigjmp_buf jmpbuf;
static volatile sig_atomic_t canjump = 0;

static RETSIGTYPE sigill_handler (int sig)
{
    if (!canjump) {
	signal (sig, SIG_DFL);
	raise (sig);
    }

    canjump = 0;
    siglongjmp (jmpbuf, 1);
}

static inline uint32_t arch_accel (void)
{
    signal (SIGILL, sigill_handler);
    if (sigsetjmp (jmpbuf, 1)) {
	signal (SIGILL, SIG_DFL);
	return 0;
    }

    canjump = 1;

    asm volatile ("mtspr 256, %0\n\t"
		  "vand %%v0, %%v0, %%v0"
		  :
		  : "r" (-1));

    signal (SIGILL, SIG_DFL);
    return MM_ACCEL_PPC_ALTIVEC;
}
#endif /* ARCH_PPC */

uint32_t mm_accel (void)
{
#if defined (ARCH_X86) || defined (ARCH_PPC)
    static int got_accel = 0;
    static uint32_t accel;

    if (!got_accel) {
	got_accel = 1;
	accel = arch_accel ();
    }

    return accel;
#else
    return 0;
#endif
}
