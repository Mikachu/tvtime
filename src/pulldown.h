
#ifndef PULLDOWN_H_INCLUDED
#define PULLDOWN_H_INCLUDED

#include "speedy.h"

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
                                  unsigned char *old, unsigned char *new, int w, int h, int os, int ns );

#endif /* PULLDOWN_H_INCLUDED */
