/**
 * Copyright (c) 2001, 2002, 2003 Billy Biggs <vektor@dumbterm.net>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef PULLDOWN_H_INCLUDED
#define PULLDOWN_H_INCLUDED

#include <stdint.h>
#include "speedy.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Possible pulldown offsets.
 */
#define PULLDOWN_OFFSET_1 (1<<0)
#define PULLDOWN_OFFSET_2 (1<<1)
#define PULLDOWN_OFFSET_3 (1<<2)
#define PULLDOWN_OFFSET_4 (1<<3)
#define PULLDOWN_OFFSET_5 (1<<4)

/**
 * Actions that we can return.
 */
#define PULLDOWN_ACTION_COPY1 (1<<0)
#define PULLDOWN_ACTION_DROP2 (1<<1)
#define PULLDOWN_ACTION_MRGE3 (1<<2)
#define PULLDOWN_ACTION_COPY4 (1<<3)
#define PULLDOWN_ACTION_COPY5 (1<<4)
#define PULLDOWN_ACTION_INTRP (1<<5)

int determine_pulldown_offset( int top_repeat, int bot_repeat, int tff, int last_offset );
int determine_pulldown_offset_history( int top_repeat, int bot_repeat, int tff, int *realbest );
int determine_pulldown_offset_history_new( int top_repeat, int bot_repeat, int tff, int predicted );
int determine_pulldown_offset_short_history_new( int top_repeat, int bot_repeat, int tff, int predicted );
int determine_pulldown_offset_dalias( pulldown_metrics_t *old_peak, pulldown_metrics_t *old_relative,
                                      pulldown_metrics_t *old_mean, pulldown_metrics_t *new_peak,
                                      pulldown_metrics_t *new_relative, pulldown_metrics_t *new_mean );

void diff_factor_packed422_frame( pulldown_metrics_t *peak, pulldown_metrics_t *rel, pulldown_metrics_t *mean,
                                  uint8_t *old, uint8_t *new, int w, int h, int os, int ns );

#ifdef __cplusplus
};
#endif
#endif /* PULLDOWN_H_INCLUDED */
