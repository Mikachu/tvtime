#ifndef HAVE_CONSOLE_H
#define HAVE_CONSOLE_H

#include "tvtimeconf.h"

typedef struct console_s console_t;

console_t *console_new( config_t *cfg, int x, int y, int cols, int rows,
                        int fontsize, int video_width, int video_height,
                        double video_aspect );
void console_delete( console_t *con );
void console_printf( console_t *con, char *format, ... );
void console_gotoxy( console_t *con, int x, int y );

void console_setfg( console_t *con, unsigned int fg );
void console_setbg( console_t *con, unsigned int bg );
void console_toggle_console( console_t *con );
void console_scroll_n( console_t *con, int n );
void console_composite_packed422_scanline( console_t *con, 
                                           unsigned char *output,
                                           int width, int xpos, int scanline );

#endif
