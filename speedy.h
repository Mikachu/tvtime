
#ifndef SPEEDY_H_INCLUDED
#define SPEEDY_H_INCLUDED


void interpolate_packed422_scanline_c( unsigned char *output,
                                       unsigned char *top,
                                       unsigned char *bot, int width );
void interpolate_packed422_scanline_mmx( unsigned char *output,
                                         unsigned char *top,
                                         unsigned char *bot, int width );
void (*interpolate_packed422_scanline)( unsigned char *output,
                                        unsigned char *top,
                                        unsigned char *bot, int width );
void setup_speedy_calls( void );

#endif /* SPEEDY_H_INCLUDED */
