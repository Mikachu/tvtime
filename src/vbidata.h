
#ifndef VBIDATA_H_INCLUDED
#define VBIDATA_H_INCLUDED

#include "vbiscreen.h"
#include "tvtimeosd.h"

typedef struct vbidata_s vbidata_t;

#define CAPTURE_OFF 0
#define CAPTURE_CC1 1
#define CAPTURE_CC2 2
#define CAPTURE_CC3 4
#define CAPTURE_CC4 5
#define CAPTURE_T1  6
#define CAPTURE_T2  7
#define CAPTURE_T3  8
#define CAPTURE_T4  9

vbidata_t *vbidata_new( const char *filename, vbiscreen_t *vs, 
                        tvtime_osd_t* osd, int verbose  );

void vbidata_delete( vbidata_t *vbi );
void vbidata_reset( vbidata_t *vbi );
void vbidata_capture_mode( vbidata_t *vbi, int mode );
void vbidata_process_frame( vbidata_t *vbi, int printdebug );

#endif /* VBIDATA_H_INCLUDED */
