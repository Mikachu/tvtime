
#ifndef OSD_H_INCLUDED
#define OSD_H_INCLUDED

typedef struct osd_string_s osd_string_t;
typedef struct osd_font_s osd_font_t;

osd_string_t *osd_string_new( osd_font_t *font );
void osd_string_delete( osd_string_t *osds );
void osd_string_show_text( osd_string_t *osds, const char *text, int timeout );
int osd_string_visible( osd_string_t *osds );
void osd_string_advance_frame( osd_string_t *osds );
void osd_string_composite_packed422( osd_string_t *osds, unsigned char *output,
                                     int width, int height, int stride,
                                     int xpos, int ypos, int rightjustified );

osd_font_t *osd_font_new( const char *filename );
void osd_font_delete( osd_font_t *osdf );

#endif /* OSD_H_INCLUDED */
