

#ifndef TVTIMEOSD_H_INCLUDED
#define TVTIMEOSD_H_INCLUDED

typedef struct tvtime_osd_s tvtime_osd_t;

tvtime_osd_t *tvtime_osd_new( int width, int height, double frameaspect );
void tvtime_osd_delete( tvtime_osd_t *osd );

void tvtime_osd_show_channel_number( tvtime_osd_t *osd, const char *text );

void tvtime_osd_show_volume_bar( tvtime_osd_t *osd, int percentage );
void tvtime_osd_volume_muted( tvtime_osd_t *osd, int mutestate );

void tvtime_osd_show_data_bar( tvtime_osd_t *osd, const char *barname,
                               int percentage );
void tvtime_osd_composite_packed422( tvtime_osd_t *osd, unsigned char *output,
                                     int width, int height, int stride );
void tvtime_osd_advance_frame( tvtime_osd_t *osd );

#endif /* TVTIMEOSD_H_INCLUDED */
