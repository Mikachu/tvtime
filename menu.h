#ifndef MENU_H_INCLUDED
#define MENU_H_INCLUDED

#include "input.h"
#include "tvtimeconf.h"

typedef struct menu_s menu_t;

menu_t *menu_new( input_t *in, config_t *cfg, int width, 
                  int height, double aspect );


void menu_init( menu_t *m );

int menu_callback( menu_t *m, InputEvent command, int arg );

void menu_composite_packed422( menu_t *m, unsigned char *output,
                               int width, int height, int stride );

#endif
