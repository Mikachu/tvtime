
#ifndef MPEG2INPUT_H_INCLUDED
#define MPEG2INPUT_H_INCLUDED

#include <stdlib.h>

typedef struct mpeg2input_s mpeg2input_t;

mpeg2input_t *mpeg2input_new( const char *filename, int track, int accel );
void mpeg2input_delete( mpeg2input_t *instance );
uint8_t *mpeg2input_get_curframe( mpeg2input_t *instance );
int tvtime_mpeg2dec_get_width( mpeg2input_t *instance );
int tvtime_mpeg2dec_get_height( mpeg2input_t *instance );

#endif /* MPEG2INPUT_H_INCLUDED */
