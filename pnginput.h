/**
 * Copyright (C) 2002 Billy Biggs <vektor@dumbterm.net>.
 */

#ifndef PNGINPUT_H_INCLUDED
#define PNGINPUT_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pnginput_s pnginput_t;

pnginput_t *pnginput_new( const char *filename );
void pnginput_delete( pnginput_t *pnginput );
unsigned int pnginput_get_width( pnginput_t *pnginput );
unsigned int pnginput_get_height( pnginput_t *pnginput );
unsigned char *pnginput_get_scanline( pnginput_t *pnginput, int num );

#ifdef __cplusplus
};
#endif
#endif /* PNGINPUT_H_INCLUDED */
