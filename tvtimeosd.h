

#ifndef TVTIMEOSD_H_INCLUDED
#define TVTIMEOSD_H_INCLUDED

typedef struct tvtime_osd_s tvtime_osd_t;

tvtime_osd_t *tvtime_osd_new( int width, int height, double frameaspect );
void tvtime_osd_delete( tvtime_osd_t *osd );


#endif /* TVTIMEOSD_H_INCLUDED */
