#ifndef HAVE_VBISCREEN_H
#define HAVE_VBISCREEN_H

typedef struct vbiscreen_s vbiscreen_t;

vbiscreen_t *vbiscreen_new( int video_width, int video_height, 
                            double video_aspect );
void vbiscreen_delete( vbiscreen_t *vs );
void vbiscreen_set_mode( vbiscreen_t *vs, int caption, int style,
                         int indent, int ital, unsigned int colour, int row );
void vbiscreen_tab( vbiscreen_t *vs, int cols );
void vbiscreen_delete_to_end( vbiscreen_t *vs );
void vbiscreen_backspace( vbiscreen_t *vs );
void vbiscreen_erase_displayed( vbiscreen_t *vs );
void vbiscreen_erase_non_displayed( vbiscreen_t *vs );
void vbiscreen_carriage_return( vbiscreen_t *vs );
void vbiscreen_end_of_caption( vbiscreen_t *vs );
void vbiscreen_print( vbiscreen_t *vs, char c1, char c2 );
void vbiscreen_composite_packed422_scanline( vbiscreen_t *vs,
                                             unsigned char *output,
                                             int width, int xpos, 
                                             int scanline );
void vbiscreen_dump_screen_text( vbiscreen_t *vs );

#endif
