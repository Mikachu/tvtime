
#ifndef SPEEDTOOLS_H_INCLUDED
#define SPEEDTOOLS_H_INCLUDED

#define PREFETCH_2048(x) \
    { int *pfetcha = (int *) x; \
        prefetchnta( pfetcha ); \
        prefetchnta( pfetcha + 64 ); \
        prefetchnta( pfetcha + 128 ); \
        prefetchnta( pfetcha + 192 ); \
        pfetcha += 256; \
        prefetchnta( pfetcha ); \
        prefetchnta( pfetcha + 64 ); \
        prefetchnta( pfetcha + 128 ); \
        prefetchnta( pfetcha + 192 ); }

#define READ_PREFETCH_2048(x) \
    { int *pfetcha = (int *) x; int pfetchtmp; \
        pfetchtmp = pfetcha[ 0 ] + pfetcha[ 16 ] + pfetcha[ 32 ] + pfetcha[ 48 ] + \
            pfetcha[ 64 ] + pfetcha[ 80 ] + pfetcha[ 96 ] + pfetcha[ 112 ] + \
            pfetcha[ 128 ] + pfetcha[ 144 ] + pfetcha[ 160 ] + pfetcha[ 176 ] + \
            pfetcha[ 192 ] + pfetcha[ 208 ] + pfetcha[ 224 ] + pfetcha[ 240 ]; \
        pfetcha += 256; \
        pfetchtmp = pfetcha[ 0 ] + pfetcha[ 16 ] + pfetcha[ 32 ] + pfetcha[ 48 ] + \
            pfetcha[ 64 ] + pfetcha[ 80 ] + pfetcha[ 96 ] + pfetcha[ 112 ] + \
            pfetcha[ 128 ] + pfetcha[ 144 ] + pfetcha[ 160 ] + pfetcha[ 176 ] + \
            pfetcha[ 192 ] + pfetcha[ 208 ] + pfetcha[ 224 ] + pfetcha[ 240 ]; }

#endif /* SPEEDTOOLS_H_INCLUDED */
