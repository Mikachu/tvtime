
#ifndef RVRREADER_H_INCLUDED
#define RVRREADER_H_INCLUDED

#include <ree.h>

typedef struct rvrreader_s rvrreader_t;

rvrreader_t *rvrreader_new( const char *filename );
void rvrreader_delete( rvrreader_t *reader );

ree_file_header_t *rvrreader_get_fileheader( rvrreader_t *reader );
int rvrreader_get_width( rvrreader_t *reader );
int rvrreader_get_height( rvrreader_t *reader );

int rvrreader_next_frame( rvrreader_t *reader );
int rvrreader_get_curframe( rvrreader_t *reader );
int rvrreader_decode_curframe( rvrreader_t *reader, unsigned char *data );

#endif /* RVRREADER_H_INCLUDED */
