
#include <stdio.h>
#include "pulldown.h"

/**
 * Possible pulldown offsets.
 */
#define PULLDOWN_OFFSET_1 (1<<0)
#define PULLDOWN_OFFSET_2 (1<<1)
#define PULLDOWN_OFFSET_3 (1<<2)
#define PULLDOWN_OFFSET_4 (1<<3)
#define PULLDOWN_OFFSET_5 (1<<4)

/* Offset                  1     2     3      4      5   */
/* Field Pattern          [T B  T][B  T][B   T B]  [T B] */
/* Action                 Copy  Save  Merge  Copy  Copy  */
/*                              Bot   Top                */
int tff_top_pattern[] = { 0,    1,    0,     0,    0     };
int tff_bot_pattern[] = { 0,    0,    0,     1,    0     };

/* Offset                  1     2     3      4      5   */
/* Field Pattern          [B T  B][T  B][T   B T]  [B T] */
/* Action                 Copy  Save  Merge  Copy  Copy  */
/*                              Top   Bot                */
int bff_top_pattern[] = { 0,    0,    0,     1,    0     };
int bff_bot_pattern[] = { 0,    1,    0,     0,    0     };

/* Timestamp mangling                                            */
/* From the DVD :         0  +  3003+ 6006 + 9009+ 12012 = 15015 */
/* In 24fps time:         0  +      + 3754 + 7508+ 11262 = 15016 */

/**
 * Flag Pattern     Treat as
 * on DVD           last offset
 * ============================
 * T B T            bff 3
 * B T              bff 4
 * B T B            tff 3
 * T B              tff 4
 */

int determine_pulldown_offset( int top_repeat, int bot_repeat, int tff,
                               int last_offset )
{
    int predicted_offset;
    int pd_patterns = 0;
    int offset = -1;
    int exact = -1;
    int i;

    predicted_offset = last_offset << 1;
    if( predicted_offset > PULLDOWN_OFFSET_5 ) predicted_offset = PULLDOWN_OFFSET_1;

    /**
     * Detect our pattern.
     */
    for( i = 0; i < 5; i++ ) {

        /**
         *  Truth table:
         *
         *  ref repeat,  frame repeat    valid
         *  ===========+==============+=======
         *   0           0            ->  1
         *   0           1            ->  1
         *   1           0            ->  0
         *   1           1            ->  1
         */

        if( tff ) {
            if(    ( !tff_top_pattern[ i ] || top_repeat )
                && ( !tff_bot_pattern[ i ] || bot_repeat ) ) {

                pd_patterns |= ( 1 << i );
                offset = i;
            }
        } else {
            if(    ( !bff_top_pattern[ i ] || top_repeat )
                && ( !bff_bot_pattern[ i ] || bot_repeat ) ) {

                pd_patterns |= ( 1 << i );
                offset = i;
            }
            if( bff_top_pattern[ i ] == top_repeat && bff_bot_pattern[ i ] == bot_repeat ) {
                exact = i;
            }
        }
    }

    offset = 1 << offset;

    /**
     * Check if the 3:2 pulldown pattern we previously decided on is
     * valid for this set.  If so, we use that.
     */
    if( pd_patterns & predicted_offset ) offset = predicted_offset;
    if( ( top_repeat || bot_repeat ) && exact > 0 ) offset = ( 1 << exact );

    return offset;
}


int pulldown_offset_frame( unsigned char *lastluma, unsigned char *lastcb, unsigned char *lastcr,
                           unsigned char *curluma, unsigned char *curcb, unsigned char *curcr,
                           int width, int height, int tff, int lastoffset )
{
    int top = 0;
    int bot = 0;

/*
    if( lastluma ) {
        top = compare_field( curluma, width * 2, lastluma, width * 2, width, height /2 );
        bot = compare_field( curluma + width, width * 2, lastluma + width, width * 2, width, height /2 );
    }
*/

    return determine_pulldown_offset( top, bot, tff, lastoffset );
}

/*
                if( buf->pdoffset == PULLDOWN_OFFSET_2 ) {
                    fprintf( stderr, "drop\n" );
                    * Drop. *
                    dropflag = 1;
                    framemgr_free_frame( frame );
                } else if( buf->pdoffset == PULLDOWN_OFFSET_3 && buf->lastframe ) {
                    videoframe_t *mergeresult;
                    fprintf( stderr, "merge\n" );

                    * Merge. *
                    mergeresult = framemgr_get_frame( buf->mgr, 0 );
                    queueframe = mergeresult;
                    merge_field_and_copy( videoframe_get_luma( mergeresult ),
                                          videoframe_get_luma( frame ),
                                          videoframe_get_luma( buf->lastframe ),
                                          tff ? FIELD_TOP : FIELD_BOTTOM,
                                          width, height );
                    merge_field_and_copy( videoframe_get_cb( mergeresult ),
                                          videoframe_get_cb( frame ),
                                          videoframe_get_cb( buf->lastframe ),
                                          tff ? FIELD_TOP : FIELD_BOTTOM,
                                          width/2, height/2 );
                    merge_field_and_copy( videoframe_get_cr( mergeresult ),
                                          videoframe_get_cr( frame ),
                                          videoframe_get_cr( buf->lastframe ),
                                          tff ? FIELD_TOP : FIELD_BOTTOM,
                                          width/2, height/2 );
            }
        }
*/

