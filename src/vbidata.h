
#ifndef VBIDATA_H_INCLUDED
#define VBIDATA_H_INCLUDED

typedef struct vbidata_s vbidata_t;

vbidata_t *vbidata_new( const char *filename );
void vbidata_delete( vbidata_t *vbi );

void vbidata_process_frame( vbidata_t *vbi, int printdebug );

#endif /* VBIDATA_H_INCLUDED */
