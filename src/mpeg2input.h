
#ifndef MPEG2INPUT_H_INCLUDED
#define MPEG2INPUT_H_INCLUDED

#include <stdlib.h>

typedef struct mpeg2input_s mpeg2input_t;

mpeg2input_t *mpeg2input_new( const char *filename, int track, int accel );
void mpeg2input_delete( mpeg2input_t *instance );
uint8_t *mpeg2input_get_curframe( mpeg2input_t *mpegin );
uint8_t *mpeg2input_get_lastframe( mpeg2input_t *mpegin );
uint8_t *mpeg2input_get_secondlastframe( mpeg2input_t *mpegin );
int mpeg2input_next_frame( mpeg2input_t *mpegin );
int mpeg2input_get_width( mpeg2input_t *mpegin );
int mpeg2input_get_height( mpeg2input_t *mpegin );

#endif /* MPEG2INPUT_H_INCLUDED */
