
#ifndef VBIDATA_H_INCLUDED
#define VBIDATA_H_INCLUDED

#include "console.h"

typedef struct vbidata_s vbidata_t;

vbidata_t *vbidata_new( const char *filename, console_t *con  );
void vbidata_delete( vbidata_t *vbi );
void vbidata_reset( vbidata_t *vbi );

void vbidata_process_frame( vbidata_t *vbi, int printdebug );

#endif /* VBIDATA_H_INCLUDED */
