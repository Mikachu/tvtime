/**
 * Copyright (c) 2003 Billy Biggs <vektor@dumbterm.net>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/**
 * Uses code from:
 *
 *  linux/arch/i386/kernel/setup.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *
 * Found in linux 2.4.20.
 *
 * Also helped from code in 'cpuinfo.c' found in mplayer.
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

static double measure_cpu_mhz( void )
{
    uint64_t tsc_start, tsc_end;
    struct timeval tv_start, tv_end;
    int usec_delay;

    rdtscll( tsc_start );
    gettimeofday( &tv_start, 0 );
    usleep( 100000 );
    rdtscll( tsc_end );
    gettimeofday( &tv_end, 0 );

    usec_delay = 1000000 * (tv_end.tv_sec - tv_start.tv_sec) + (tv_end.tv_usec - tv_start.tv_usec);

    return (((double) (tsc_end - tsc_start)) / ((double) usec_delay));
}

typedef struct cpuid_regs {
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
} cpuid_regs_t;

static cpuid_regs_t cpuid( int func ) {
    cpuid_regs_t regs;
#define CPUID ".byte 0x0f, 0xa2; "
    asm("movl %4,%%eax; " CPUID
        "movl %%eax,%0; movl %%ebx,%1; movl %%ecx,%2; movl %%edx,%3"
            : "=m" (regs.eax), "=m" (regs.ebx), "=m" (regs.ecx), "=m" (regs.edx)
            : "g" (func)
            : "%eax", "%ebx", "%ecx", "%edx");
    return regs;
}

#define X86_VENDOR_INTEL 0
#define X86_VENDOR_CYRIX 1
#define X86_VENDOR_AMD 2
#define X86_VENDOR_UMC 3
#define X86_VENDOR_NEXGEN 4
#define X86_VENDOR_CENTAUR 5
#define X86_VENDOR_RISE 6
#define X86_VENDOR_TRANSMETA 7
#define X86_VENDOR_NSC 8
#define X86_VENDOR_UNKNOWN 0xff

struct cpu_model_info {
    int vendor;
    int family;
    char *model_names[16];
};

/* Naming convention should be: <Name> [(<Codename>)] */
/* This table only is used unless init_<vendor>() below doesn't set it; */
/* in particular, if CPUID levels 0x80000002..4 are supported, this isn't used */
static struct cpu_model_info cpu_models[] = {
    { X86_VENDOR_INTEL,    4,
      { "486 DX-25/33", "486 DX-50", "486 SX", "486 DX/2", "486 SL", 
        "486 SX/2", NULL, "486 DX/2-WB", "486 DX/4", "486 DX/4-WB", NULL, 
        NULL, NULL, NULL, NULL, NULL }},
    { X86_VENDOR_INTEL,    5,
      { "Pentium 60/66 A-step", "Pentium 60/66", "Pentium 75 - 200",
        "OverDrive PODP5V83", "Pentium MMX", NULL, NULL,
        "Mobile Pentium 75 - 200", "Mobile Pentium MMX", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL }},
    { X86_VENDOR_INTEL,    6,
      { "Pentium Pro A-step", "Pentium Pro", NULL, "Pentium II (Klamath)", 
        NULL, "Pentium II (Deschutes)", "Mobile Pentium II",
        "Pentium III (Katmai)", "Pentium III (Coppermine)", NULL,
        "Pentium III (Cascades)", NULL, NULL, NULL, NULL }},
    { X86_VENDOR_AMD,    4,
      { NULL, NULL, NULL, "486 DX/2", NULL, NULL, NULL, "486 DX/2-WB",
        "486 DX/4", "486 DX/4-WB", NULL, NULL, NULL, NULL, "Am5x86-WT",
        "Am5x86-WB" }},
    { X86_VENDOR_AMD,    5, /* Is this this really necessary?? */
      { "K5/SSA5", "K5",
        "K5", "K5", NULL, NULL,
        "K6", "K6", "K6-2",
        "K6-3", NULL, NULL, NULL, NULL, NULL, NULL }},
    { X86_VENDOR_AMD,    6, /* Is this this really necessary?? */
      { "Athlon", "Athlon",
        "Athlon", NULL, "Athlon", NULL,
        NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL }},
    { X86_VENDOR_UMC,    4,
      { NULL, "U5D", "U5S", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL }},
    { X86_VENDOR_NEXGEN,    5,
      { "Nx586", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL }},
    { X86_VENDOR_RISE,    5,
      { "iDragon", NULL, "iDragon", NULL, NULL, NULL, NULL,
        NULL, "iDragon II", "iDragon II", NULL, NULL, NULL, NULL, NULL, NULL }},
};

/* Look up CPU names by table lookup. */
static char *table_lookup_model( int vendor, int family, int model )
{
    struct cpu_model_info *info = cpu_models;
    int i;

    if( model >= 16 ) {
        return NULL; /* Range check */
    }

    for( i = 0; i < sizeof(cpu_models)/sizeof(struct cpu_model_info); i++ ) {
        if( info->vendor == vendor && info->family == family ) {
            return info->model_names[ model ];
        }
        info++;
    }

    return NULL; /* Not found */
}

static int get_cpu_vendor( const char *idstr )
{
    if( !strcmp( idstr, "GenuineIntel" ) ) return X86_VENDOR_INTEL;
    if( !strcmp( idstr, "AuthenticAMD" ) ) return X86_VENDOR_AMD;
    if( !strcmp( idstr, "CyrixInstead" ) ) return X86_VENDOR_CYRIX;
    if( !strcmp( idstr, "Geode by NSC" ) ) return X86_VENDOR_NSC;
    if( !strcmp( idstr, "UMC UMC UMC " ) ) return X86_VENDOR_UMC;
    if( !strcmp( idstr, "CentaurHauls" ) ) return X86_VENDOR_CENTAUR;
    if( !strcmp( idstr, "NexGenDriven" ) ) return X86_VENDOR_NEXGEN;
    if( !strcmp( idstr, "RiseRiseRise" ) ) return X86_VENDOR_RISE;
    if( !strcmp( idstr, "GenuineTMx86" ) || !strcmp( idstr, "TransmetaCPU" ) ) return X86_VENDOR_TRANSMETA;
    return X86_VENDOR_UNKNOWN;
}


static void store32( char *d, unsigned int v )
{
    d[0] =  v        & 0xff;
    d[1] = (v >>  8) & 0xff;
    d[2] = (v >> 16) & 0xff;
    d[3] = (v >> 24) & 0xff;
}

void cpuinfo_print_info( void )
{
    cpuid_regs_t regs, regs_ext;
    unsigned int max_cpuid;
    unsigned int max_ext_cpuid;
    unsigned int amd_flags;
    int family, model, stepping;
    char idstr[13];
    char *model_name;
    char processor_name[49];
    int i;

    regs = cpuid(0);
    max_cpuid = regs.eax;

    store32(idstr+0, regs.ebx);
    store32(idstr+4, regs.edx);
    store32(idstr+8, regs.ecx);
    idstr[12] = 0;

    regs = cpuid( 1 );
    family = (regs.eax >> 8) & 0xf;
    model = (regs.eax >> 4) & 0xf;
    stepping = regs.eax & 0xf;

    model_name = table_lookup_model( get_cpu_vendor( idstr ), family, model );

    regs_ext = cpuid((1<<31) + 0);
    max_ext_cpuid = regs_ext.eax;

    if (max_ext_cpuid >= (1<<31) + 1) {
        regs_ext = cpuid((1<<31) + 1);
        amd_flags = regs_ext.edx;

        if (max_ext_cpuid >= (1<<31) + 4) {
            for (i = 2; i <= 4; i++) {
                regs_ext = cpuid((1<<31) + i);
                store32(processor_name + (i-2)*16, regs_ext.eax);
                store32(processor_name + (i-2)*16 + 4, regs_ext.ebx);
                store32(processor_name + (i-2)*16 + 8, regs_ext.ecx);
                store32(processor_name + (i-2)*16 + 12, regs_ext.edx);
            }
            processor_name[48] = 0;
            model_name = processor_name;
        }
    } else {
        amd_flags = 0;
    }

    /* Is this dangerous? */
    while( isspace( *model_name ) ) model_name++;

    fprintf( stderr, "cpuinfo: CPU %s, family %d, model %d, stepping %d.\n", model_name, family, model, stepping );
    fprintf( stderr, "cpuinfo: CPU measured at %.3fMHz.\n", measure_cpu_mhz() );
}

