
#ifndef CREDITS_H_INCLUDED
#define CREDITS_H_INCLUDED

typedef struct credits_s credits_t;

credits_t *credits_new( const char *filename, int output_height );
void credits_delete( void );
void credits_restart( credits_t *credits, double speed );
void credits_composite_packed422_scanline( credits_t *credits,
                                           unsigned char *output,
                                           int width, int xpos,
                                           int scanline );
void credits_advance_frame( credits_t *credits );


#endif /* CREDITS_H_INCLUDED */
