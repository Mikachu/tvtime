// -*- c++ -*-

static unsigned char*	     pDest;
static const unsigned char* pSrcP;
static const unsigned char* pSrc;
static const unsigned char* pBob;
static const unsigned char* pBobP;
static Q_INT64 Max_Mov;
static Q_INT64 DiffThres;
static Q_INT64 YMask;
static Q_INT64 UVMask;
static Q_INT64 TENS;
static Q_INT64 FOURS;
static Q_INT64 ONES;
static Q_INT64 Min_Vals;
static Q_INT64 Max_Vals;
static Q_INT64 ShiftMask;

Max_Mov   = 0x0404040404040404ull; 
DiffThres = 0x0f0f0f0f0f0f0f0full; 
YMask     = 0x00ff00ff00ff00ffull; // keeps only luma
UVMask    = 0xff00ff00ff00ff00ull; // keeps only chroma
TENS      = 0x0a0a0a0a0a0a0a0aull; 
FOURS     = 0x0404040404040404ull; 
ONES      = 0x0101010101010101ull; 
Min_Vals  = 0x0000000000000000ull;
Max_Vals  = 0x0000000000000000ull;
ShiftMask = 0xfefffefffefffeffull;

#if defined(DBL_RESIZE) || defined(USE_FOR_DSCALER)
int	    src_pitch2 = src_pitch;			// even & odd lines are not interleaved in DScaler
#else
int	    src_pitch2 = 2 * src_pitch;		// even & odd lines are interleaved in Avisynth
#endif

int	    dst_pitch2 = 2 * dst_pitch;
int     y;

#ifdef IS_SSE2
int     Last8 = (rowsize-16);			// ofs to last 16 bytes in row for SSE2
#else
int     Last8 = (rowsize-8);			// ofs to last 8 bytes in row
#endif

int		dst_pitchw = dst_pitch; // local stor so asm can ref
	pSrc  = pWeaveSrc;			// points 1 weave line above
	pSrcP = pWeaveSrcP;			// " 

#ifdef DBL_RESIZE
	        
#ifdef USE_VERTICAL_FILTER
	pDest = pWeaveDest + dst_pitch2;
#else
	pDest = pWeaveDest + 3*dst_pitch;
#endif

#else

#ifdef USE_VERTICAL_FILTER
	pDest = pWeaveDest + dst_pitch;
#else
	pDest = pWeaveDest + dst_pitch2;
#endif

#endif

	if (TopFirst)
	{
		pBob = pCopySrc + src_pitch2;      // remember one weave line just copied previously
		pBobP = pCopySrcP + src_pitch2;
	}
	else
	{
		pBob =  pCopySrc;
		pBobP =  pCopySrcP;
	}

#ifndef _pBob
#define _pBob       "%0"
#define _src_pitch2 "%1"
#define _ShiftMask  "%2"
#define _pDest      "%3"
#define _dst_pitchw "%4"
#define _Last8      "%5"
#define _pSrc       "%6"
#define _pSrcP      "%7"
#define _pBobP      "%8"
#define _DiffThres  "%9"
#define _Min_Vals   "%10"
#define _Max_Vals   "%11"
#define _FOURS      "%12"
#define _TENS       "%13"
#define _ONES       "%14"
#define _UVMask     "%15"
#define _Max_Mov    "%16"
#define _YMask      "%17"
#endif

	for (y=1; y < FldHeight-1; y++)	
	{
		// pretend it's indented -->>
        __asm__ __volatile__
            (
             // Loop general reg usage
             //
             // eax - pBobP, then pDest 
             // ebx - pBob
             // ecx - src_pitch2
             // edx - current offset
             // edi - prev weave pixels, 1 line up
             // esi - next weave pixels, 1 line up

             // Save ebx (-fPIC)
             "pushl %%ebx\n\t"
             
#ifdef IS_SSE2
             
             // sse2 code deleted for now

#else
             // simple bob first 8 bytes
             "movl		"_pBob",        %%ebx\n\t"
             "movl	"_src_pitch2",  %%ecx\n\t"

#ifdef USE_VERTICAL_FILTER
             "movq	    (%%ebx),        %%mm0\n\t"
             "movq	    (%%ebx, %%ecx), %%mm1\n\t" //, qword ptr[ebx+ecx]
             "movq	    %%mm0,          %%mm2\n\t"
             V_PAVGB ("%%mm2", "%%mm1", "%%mm3", _ShiftMask)		// halfway between
             V_PAVGB ("%%mm0", "%%mm2", "%%mm3", _ShiftMask)		// 1/4 way
             V_PAVGB ("%%mm1", "%%mm2", "%%mm3", _ShiftMask)		// 3/4 way
             "movl		"_pDest",       %%edi\n\t"
             "movl		"_dst_pitchw",  %%eax\n\t"
             V_MOVNTQ	("(%%edi)", "%%mm0")
             V_MOVNTQ	("(%%edi, %%eax)", "%%mm1") // qword ptr[edi+eax], mm1

             // simple bob last 8 bytes
             "movl		"_Last8", %%edx\n\t"
             "leal		(%%ebx, %%edx), %%esi\n\t"  // [ebx+edx]
             "movq	    (%%esi), %%mm0\n\t"
             "movq	    (%%esi, %%ecx), %%mm1\n\t"    // qword ptr[esi+ecx]
             "movq	    %%mm0, %%mm2\n\t"
             V_PAVGB ("%%mm2", "%%mm1", "%%mm3", _ShiftMask)		// halfway between
             V_PAVGB ("%%mm0", "%%mm2", "%%mm3", _ShiftMask)		// 1/4 way
             V_PAVGB ("%%mm1", "%%mm2", "%%mm3", _ShiftMask)		// 3/4 way
             "addl		%%edx, %%edi\n\t"						// last 8 bytes of dest
             V_MOVNTQ	("%%edi", "%%mm0")
             V_MOVNTQ	("(%%edi, %%eax)", "%%mm1") // qword ptr[edi+eax], mm1)

#else
             "movq	(%%ebx), %%mm0\n\t"
             //		pavgb	mm0, qword ptr[ebx+ecx]
             V_PAVGB ("%%mm0", "(%%ebx, %%ecx)", "%%mm2", _ShiftMask) // qword ptr[ebx+ecx], mm2, ShiftMask)
             "movl		"_pDest", %%edi\n\t"
             V_MOVNTQ	("(%%edi)", "%%mm0")

             // simple bob last 8 bytes
             "movl		"_Last8", %%edx\n\t"
             "leal		(%%ebx, %%edx), %%esi\n\t" //esi, [ebx+edx]
             "movq	    (%%esi), %%mm0\n\t"
             //		pavgb	mm0, qword ptr[esi+ecx]
             V_PAVGB	("%%mm0", "(%%esi, %%ecx)", "%%mm2", _ShiftMask) // qword ptr[esi+ecx], mm2, ShiftMask)
             V_MOVNTQ	("(%%edi, %%edx)", "%%mm0") // qword ptr[edi+edx], mm0)
#endif
             // now loop and get the middle qwords
             "movl		"_pSrc", %%esi\n\t"
             "movl		"_pSrcP", %%edi\n\t"
             "movl		$8, %%edx\n\t"				// curr offset into all lines

             "1:\n\t"	
             "movl		"_pBobP", %%eax\n\t"
             "addl		$8, %%edi\n\t"
             "addl		$8, %%esi\n\t"
             "addl		$8, %%ebx\n\t"
             "addl		%%edx, %%eax\n\t"

#ifdef USE_STRANGE_BOB
#include "StrangeBob.inc"
#else
#include "WierdBob.inc"
#endif

             // For non-SSE2:
             // through out most of the rest of this loop we will maintain
             //	mm4		our min bob value
             //	mm5		best weave pixels so far
             //  mm6		our max Bob value 
             //	mm7		best weighted pixel ratings so far
             
             // We will keep a slight bias to using the weave pixels
             // from the current location, by rating them by the min distance
             // from the Bob value instead of the avg distance from that value.
             // our best and only rating so far
             "pcmpeqb	%%mm7, %%mm7\n\t"			// ffff, say we didn't find anything good yet

#endif
