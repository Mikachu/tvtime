
#ifndef TWOFRAME_H_INCLUDED
#define TWOFRAME_H_INCLUDED

void deinterlace_twoframe_packed422_scanline_mmxext( unsigned char *output,
                                                     unsigned char *t1,
                                                     unsigned char *m1,
                                                     unsigned char *b1,
                                                     unsigned char *t0,
                                                     unsigned char *m0,
                                                     unsigned char *b0,
                                                     int width );

#endif /* TWOFRAME_H_INCLUDED */
