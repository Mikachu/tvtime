

#ifndef VIDEOFILTER_H_INCLUDED
#define VIDEOFILTER_H_INCLUDED

typedef struct videofilter_s videofilter_t;

videofilter_t *videofilter_new( void );
void videofilter_delete( videofilter_t *vf );

void videofilter_set_bt8x8_correction( videofilter_t *vf, int correct );
void videofilter_set_full_extent_correction( videofilter_t *vf, int correct );
void videofilter_set_luma_power( videofilter_t *vf, double power );

int videofilter_active_on_scanline( videofilter_t *vf, int scanline );
void videofilter_packed422_scanline( videofilter_t *vf, unsigned char *data,
                                     int width, int xpos, int scanline );

#endif /* VIDEOFILTER_H_INCLUDED */
