/**
 * Copyright (C) 2002 Billy Biggs <vektor@dumbterm.net>.
 */

#ifndef PNGOUTPUT_H_INCLUDED
#define PNGOUTPUT_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pngoutput_s pngoutput_t;

/**
 * Creates a new PNG output file at the given width and height.  The
 * gamma should be the intended
 */
pngoutput_t *pngoutput_new( const char *filename, int width, int height,
                            int alpha, double gamma );

/**
 * Closes the PNG file, finishing the write.
 */
void pngoutput_delete( pngoutput_t *pngoutput );

/**
 * Writes a scanline (of the appropriate width) to the PNG file.
 */
void pngoutput_scanline( pngoutput_t *pngoutput, unsigned char *scanline );

#ifdef __cplusplus
};
#endif
#endif /* PNGOUTPUT_H_INCLUDED */
