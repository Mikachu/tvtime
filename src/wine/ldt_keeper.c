/*
 * Copyright (C) 2000-2003 the xine project
 * 
 * This file is part of xine, a free video player.
 * 
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id$
 *
 *
 * contents:
 *
 * Allocate an LDT entry for the TEB (thread environment block)
 * TEB is a the only thread specific structure provided to userspace
 * by MS Windows. 
 * Any W32 dll may access the TEB through FS:0, so we must provide it.
 *
 * Additional notes:
 * aviplay used to use the same LDT/TEB/FS to all his threads and did it
 * by calling these functions before any threads have been created. this
 * is a ugly hack, as the main code includes a plugin function at its
 * initialization.
 * Also, IMHO, that was slightly wrong. The TEB is supposed to be unique 
 * per W32 thread. The current xine implementation will allocate different
 * TEBs for the audio and video codecs.
 *
 */
 
 
/**
 * OLD AVIFILE COMMENT:
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * This file MUST be in main library because LDT must
 * be modified before program creates first thread
 * - avifile includes this file from C++ code
 * and initializes it at the start of player!
 * it might sound like a hack and it really is - but
 * as aviplay is deconding video with more than just one
 * thread currently it's necessary to do it this way
 * this might change in the future
 */

/* applied some modification to make make our xine friend more happy */
#include "ldt_keeper.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#ifdef __linux__
#include <asm/unistd.h>
#include <asm/ldt.h>
/* 2.5.xx+ calls this user_desc: */
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,47)
#define modify_ldt_ldt_s user_desc
#endif
/* prototype it here, so we won't depend on kernel headers */
#ifdef  __cplusplus
extern "C" {
#endif
int modify_ldt(int func, void *ptr, unsigned long bytecount);
#ifdef  __cplusplus
}
#endif
#else
#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include <machine/segments.h>
#include <machine/sysarch.h>
#endif

#ifdef __svr4__
#include <sys/segment.h>
#include <sys/sysi86.h>

/* solaris x86: add missing prototype for sysi86() */
#ifdef  __cplusplus
extern "C" {
#endif
int sysi86(int, void*);
#ifdef  __cplusplus
}
#endif

#ifndef NUMSYSLDTS             /* SunOS 2.5.1 does not define NUMSYSLDTS */
#define NUMSYSLDTS     6       /* Let's hope the SunOS 5.8 value is OK */
#endif

#define       TEB_SEL_IDX     NUMSYSLDTS
#endif

#define LDT_ENTRIES     8192
#define LDT_ENTRY_SIZE  8
#pragma pack(4)
struct modify_ldt_ldt_s {
        unsigned int  entry_number;
        unsigned long base_addr;
        unsigned int  limit;
        unsigned int  seg_32bit:1;
        unsigned int  contents:2;
        unsigned int  read_exec_only:1;
        unsigned int  limit_in_pages:1;
        unsigned int  seg_not_present:1;
        unsigned int  useable:1;
};

#define MODIFY_LDT_CONTENTS_DATA        0
#define MODIFY_LDT_CONTENTS_STACK       1
#define MODIFY_LDT_CONTENTS_CODE        2
#endif


/* user level (privilege level: 3) ldt (1<<2) segment selector */
#define       LDT_SEL(idx) ((idx) << 3 | 1 << 2 | 3)

/*
 * linuxthreads can use all LDT entries from 0 to PTHREAD_THREADS_MAX-1.
 * by default PTHREAD_THREADS_MAX = 1024, so unless one has recompiled
 * it's own glibc/linuxthreads this should be a safe value.
 */
#ifndef       TEB_SEL_IDX
#define       TEB_SEL_IDX     1024
#endif

static unsigned int teb_sel = LDT_SEL(TEB_SEL_IDX);

static ldt_fs_t global_ldt_fs;
static int      global_usage_count = 0;

#ifdef __cplusplus
extern "C"
#endif
void Setup_FS_Segment(void)
{
    __asm__ __volatile__(
	"movl %0,%%eax; movw %%ax, %%fs" : : "r" (teb_sel) : "%eax"
    );
}

void Check_FS_Segment(void)
{
    int fs;
     __asm__ __volatile__(
	"movw %%fs,%%ax; mov %%eax,%0" : "=r" (fs) :: "%eax"
    );
    fs = fs & 0xffff;
    
    if( fs != teb_sel ) {
      printf("ldt_keeper: FS segment is not set or has being lost!\n");
      printf("            Please report this error to xine-devel@lists.sourceforge.net\n");
      printf("            Aborting....\n");
      abort();
    }
}

#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
static void LDT_EntryToBytes( unsigned long *buffer, const struct modify_ldt_ldt_s *content )
{
    *buffer++ = ((content->base_addr & 0x0000ffff) << 16) |
	(content->limit & 0x0ffff);
    *buffer = (content->base_addr & 0xff000000) |
	((content->base_addr & 0x00ff0000)>>16) |
	(content->limit & 0xf0000) |
	(content->contents << 10) |
	((content->read_exec_only == 0) << 9) |
	((content->seg_32bit != 0) << 22) |
	((content->limit_in_pages != 0) << 23) |
	0xf000;
}
#endif

static int _modify_ldt(struct modify_ldt_ldt_s array)
{
    int ret;

#ifdef __linux__
    ret=modify_ldt(0x1, &array, sizeof(struct modify_ldt_ldt_s));
    if(ret<0)
    {
	perror("install_fs");
	printf("Couldn't install fs segment, expect segfault\n");
    }
#endif /*linux*/

#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    {
        unsigned long d[2];

        LDT_EntryToBytes( d, &array );
#if defined(__FreeBSD__) && defined(LDT_AUTO_ALLOC)
        ret = i386_set_ldt(LDT_AUTO_ALLOC, (union descriptor *)d, 1);
        array.entry_number = ret;
        teb_sel = LDT_SEL(ret);
#else
        ret = i386_set_ldt(array.entry_number, (union descriptor *)d, 1);
#endif
        if (ret < 0)
        {
            perror("install_fs");
	    printf("Couldn't install fs segment, expect segfault\n");
            printf("Did you reconfigure the kernel with \"options USER_LDT\"?\n");
        }
	printf("Set_LDT\n");
    }
#endif  /* __NetBSD__ || __FreeBSD__ || __OpenBSD__ */

#if defined(__svr4__)
    {
	struct ssd ssd;
	ssd.sel = teb_sel;
	ssd.bo = array.base_addr;
	ssd.ls = array.limit;
	ssd.acc1 = ((array.read_exec_only == 0) << 1) |
	    (array.contents << 2) |
	    0xf0;   /* P(resent) | DPL3 | S */
	ssd.acc2 = 0x4;   /* byte limit, 32-bit segment */
	if (sysi86(SI86DSCR, &ssd) < 0) {
	    perror("sysi86(SI86DSCR)");
	    printf("Couldn't install fs segment, expect segfault\n");
	}
    }
#endif

    return ret;
}

ldt_fs_t* Setup_LDT_Keeper(void)
{
    struct modify_ldt_ldt_s array;
    int ret;
    int ldt_already_set = 0;
    ldt_fs_t* ldt_fs = (ldt_fs_t*) malloc(sizeof(ldt_fs_t));

    if (!ldt_fs)
	return NULL;

#ifdef __linux__
    /*
     * LDT might be shared by different threads, so we must
     * check it here to avoid filling the segment descriptor again.
     */
    {
        unsigned char *ldt = malloc((TEB_SEL_IDX+1)*8);
        unsigned int limit;
        
        modify_ldt(0, ldt, (TEB_SEL_IDX+1)*8);
/*        
        printf("ldt_keeper: old LDT entry = [%x] [%x]\n",
                *(unsigned int *) (&ldt[TEB_SEL_IDX*8]),
                *(unsigned int *) (&ldt[TEB_SEL_IDX*8+4]) );
*/                
        limit = ((*(unsigned int *) (&ldt[TEB_SEL_IDX*8])) & 0xffff) |
                ((*(unsigned int *) (&ldt[TEB_SEL_IDX*8+4])) & 0xf0000);
        
        if( limit ) {
            if( limit == getpagesize()-1 ) {
                ldt_already_set = 1;
            } else {
#ifdef LOG
                printf("ldt_keeper: LDT entry seems to be used by someone else. [%x] [%x]\n",
                       *(unsigned int *) (&ldt[TEB_SEL_IDX*8]),
                       *(unsigned int *) (&ldt[TEB_SEL_IDX*8+4]) );
                printf("            Please report this message to xine-devel@lists.sourceforge.net\n");
#endif        
            }
        }
        free(ldt);
    }
#endif /*linux*/

    if( !ldt_already_set )
    {
#ifdef LOG
        printf("ldt_keeper: creating a new segment descriptor.\n");
#endif        
        ldt_fs->fd = open("/dev/zero", O_RDWR);
        if(ldt_fs->fd<0){
            perror( "Cannot open /dev/zero for READ+WRITE. Check permissions! error: ");
	    return NULL;
        }
    
        ldt_fs->fs_seg = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE, MAP_PRIVATE,
			      ldt_fs->fd, 0);
        if (ldt_fs->fs_seg == (void*)-1)
        {
	    perror("ERROR: Couldn't allocate memory for fs segment");
            close(ldt_fs->fd);
            free(ldt_fs);
	    return NULL;
        }
        *(void**)((char*)ldt_fs->fs_seg+0x18) = ldt_fs->fs_seg;
        array.base_addr=(int)ldt_fs->fs_seg;
        array.entry_number=TEB_SEL_IDX;
        array.limit=getpagesize()-1;
        array.seg_32bit=1;
        array.read_exec_only=0;
        array.seg_not_present=0;
        array.contents=MODIFY_LDT_CONTENTS_DATA;
        array.limit_in_pages=0;
    
        ret = _modify_ldt(array);
        
        ldt_fs->prev_struct = (char*)malloc(sizeof(char) * 8);
        *(void**)array.base_addr = ldt_fs->prev_struct;
        
        memcpy( &global_ldt_fs, ldt_fs, sizeof(ldt_fs_t) );
    } else {
#ifdef LOG
        printf("ldt_keeper: LDT entry already set, reusing.\n");
#endif        
        global_usage_count++;
        memcpy( ldt_fs, &global_ldt_fs, sizeof(ldt_fs_t) );
    }
    
    Setup_FS_Segment();
    
    return ldt_fs;
}

void Restore_LDT_Keeper(ldt_fs_t* ldt_fs)
{
    struct modify_ldt_ldt_s array;
    
    if (ldt_fs == NULL || ldt_fs->fs_seg == 0)
	return;

    if( global_usage_count ) {
#ifdef LOG
        printf("ldt_keeper: shared LDT, restore does nothing.\n");
#endif        
        /* shared LDT. only the last user can free. */
        global_usage_count--; 
    } else {
#ifdef LOG
        printf("ldt_keeper: freeing LDT entry.\n");
#endif        
        if (ldt_fs->prev_struct)
            free(ldt_fs->prev_struct);
        munmap((char*)ldt_fs->fs_seg, getpagesize());
        ldt_fs->fs_seg = 0;
        close(ldt_fs->fd);
    
        /* mark LDT entry as free again */
        memset(&array, 0, sizeof(struct modify_ldt_ldt_s));
        array.entry_number=TEB_SEL_IDX;
        _modify_ldt(array);
    }
    free(ldt_fs);
}
