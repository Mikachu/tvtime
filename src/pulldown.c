
#include <stdio.h>
#include <limits.h>
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

#define HISTORY_SIZE 5

static int tophistory[ 5 ];
static int bothistory[ 5 ];

static int tophistory_diff[ 5 ];
static int bothistory_diff[ 5 ];

static int histpos = 0;

static void fill_history( int tff )
{
    if( tff ) {
        tophistory[ 0 ] = INT_MAX; bothistory[ 0 ] = INT_MAX;
        tophistory[ 1 ] =       0; bothistory[ 1 ] = INT_MAX;
        tophistory[ 2 ] = INT_MAX; bothistory[ 2 ] = INT_MAX;
        tophistory[ 3 ] = INT_MAX; bothistory[ 3 ] =       0;
        tophistory[ 4 ] = INT_MAX; bothistory[ 3 ] = INT_MAX;

        tophistory_diff[ 0 ] = 0; bothistory_diff[ 0 ] = 0;
        tophistory_diff[ 1 ] = 1; bothistory_diff[ 1 ] = 0;
        tophistory_diff[ 2 ] = 0; bothistory_diff[ 2 ] = 0;
        tophistory_diff[ 3 ] = 0; bothistory_diff[ 3 ] = 1;
        tophistory_diff[ 4 ] = 0; bothistory_diff[ 3 ] = 0;
    } else {
        tophistory[ 0 ] = INT_MAX; bothistory[ 0 ] = INT_MAX;
        tophistory[ 1 ] = INT_MAX; bothistory[ 1 ] =       0;
        tophistory[ 2 ] = INT_MAX; bothistory[ 2 ] = INT_MAX;
        tophistory[ 3 ] =       0; bothistory[ 3 ] = INT_MAX;
        tophistory[ 4 ] = INT_MAX; bothistory[ 3 ] = INT_MAX;

        tophistory_diff[ 0 ] = 0; bothistory_diff[ 0 ] = 0;
        tophistory_diff[ 1 ] = 0; bothistory_diff[ 1 ] = 1;
        tophistory_diff[ 2 ] = 0; bothistory_diff[ 2 ] = 0;
        tophistory_diff[ 3 ] = 1; bothistory_diff[ 3 ] = 0;
        tophistory_diff[ 4 ] = 0; bothistory_diff[ 3 ] = 0;
    }

    histpos = 0;
}


int determine_pulldown_offset_history( int top_repeat, int bot_repeat, int tff, int *realbest )
{
    int avgbot = 0;
    int avgtop = 0;
    int best = 0;
    int min = -1;
    int minpos = 0;
    int minbot = 0;
    int j;
    int ret;
    int mintopval = -1;
    int mintoppos = -1;
    int minbotval = -1;
    int minbotpos = -1;

    tophistory[ histpos ] = top_repeat;
    bothistory[ histpos ] = bot_repeat;

    for( j = 0; j < HISTORY_SIZE; j++ ) {
        avgtop += tophistory[ j ];
        avgbot += bothistory[ j ];
    }
    avgtop /= 5;
    avgbot /= 5;

    for( j = 0; j < HISTORY_SIZE; j++ ) {
        // int cur = (tophistory[ j ] - avgtop);
        int cur = tophistory[ j ];
        if( cur < min || min < 0 ) {
            min = cur;
            minpos = j;
        }
        if( cur < mintopval || mintopval < 0 ) {
            mintopval = cur;
            mintoppos = j;
        }
    }

    for( j = 0; j < HISTORY_SIZE; j++ ) {
        // int cur = (bothistory[ j ] - avgbot);
        int cur = bothistory[ j ];
        if( cur < min || min < 0 ) {
            min = cur;
            minpos = j;
            minbot = 1;
        }
        if( cur < minbotval || minbotval < 0 ) {
            minbotval = cur;
            minbotpos = j;
        }
    }

    if( minbot ) {
        best = tff ? ( minpos + 2 ) : ( minpos + 4 );
    } else {
        best = tff ? ( minpos + 4 ) : ( minpos + 2 );
    }
    best = best % HISTORY_SIZE;
    *realbest = 1 << ( ( histpos + (2*HISTORY_SIZE) - best ) % HISTORY_SIZE );

    best = (minbotpos + 2) % 5;
    ret  = 1 << ( ( histpos + (2*HISTORY_SIZE) - best ) % HISTORY_SIZE );
    best = (mintoppos + 4) % 5;
    ret |= 1 << ( ( histpos + (2*HISTORY_SIZE) - best ) % HISTORY_SIZE );

    histpos = (histpos + 1) % HISTORY_SIZE;
    return ret;
}

static int reference = 0;

int determine_pulldown_offset_history_new( int top_repeat, int bot_repeat, int tff, int predicted )
{
    int avgbot = 0;
    int avgtop = 0;
    int i, j;
    int ret;
    int mintopval = -1;
    int mintoppos = -1;
    int min2topval = -1;
    int min2toppos = -1;
    int minbotval = -1;
    int minbotpos = -1;
    int min2botval = -1;
    int min2botpos = -1;
    int predicted_pos = 0;

    tophistory[ histpos ] = top_repeat;
    bothistory[ histpos ] = bot_repeat;

    for( j = 0; j < HISTORY_SIZE; j++ ) {
        avgtop += tophistory[ j ];
        avgbot += bothistory[ j ];
    }
    avgtop /= 5;
    avgbot /= 5;

    for( i = 0; i < 5; i++ ) { if( (1<<i) == predicted ) { predicted_pos = i; break; } }

    fprintf( stderr, "top: %8d bot: %8d\ttop-avg: %8d bot-avg: %8d (%d)\n", top_repeat, bot_repeat, top_repeat - avgtop, bot_repeat - avgbot, (5 + predicted_pos - reference) % 5 );

    for( j = 0; j < HISTORY_SIZE; j++ ) {
        int cur = tophistory[ j ];
        if( cur < mintopval || mintopval < 0 ) {
            min2topval = mintopval;
            min2toppos = mintoppos;
            mintopval = cur;
            mintoppos = j;
        } else if( cur < min2topval || min2topval < 0 ) {
            min2topval = cur;
            min2toppos = j;
        }
    }

    for( j = 0; j < HISTORY_SIZE; j++ ) {
        int cur = bothistory[ j ];
        if( cur < minbotval || minbotval < 0 ) {
            min2botval = minbotval;
            min2botpos = minbotpos;
            minbotval = cur;
            minbotpos = j;
        } else if( cur < min2botval || min2botval < 0 ) {
            min2botval = cur;
            min2botpos = j;
        }
    }

    tophistory_diff[ histpos ] = ((mintoppos == histpos) || (min2toppos == histpos));
    bothistory_diff[ histpos ] = ((minbotpos == histpos) || (min2botpos == histpos));

    ret = 0;
    for( i = 0; i < 5; i++ ) {
        int valid = 1;
        for( j = 0; j < 5; j++ ) {
            // if( tff_top_pattern[ j ] && !tophistory_diff[ (i + j) % 5 ] && tophistory[ (i + j) % 5 ] != mintopval ) {
            if( tff_top_pattern[ j ] && (tophistory[ (i + j) % 5 ] > avgtop || !tophistory_diff[ (i + j) % 5 ]) ) {
                valid = 0;
                break;
            }
            // if( tff_bot_pattern[ j ] && !bothistory_diff[ (i + j) % 5 ] && bothistory[ (i + j) % 5 ] != minbotval ) {
            if( tff_bot_pattern[ j ] && (bothistory[ (i + j) % 5 ] > avgbot || !bothistory_diff[ (i + j) % 5 ]) ) {
                valid = 0;
                break;
            }
        }
        if( valid ) ret |= (1<<(((5-i)+histpos)%5));
    }

    /*
    fprintf( stderr, "ret: %d %d %d %d %d\n",
             PULLDOWN_OFFSET_1 & ret,
             PULLDOWN_OFFSET_2 & ret,
             PULLDOWN_OFFSET_3 & ret,
             PULLDOWN_OFFSET_4 & ret,
             PULLDOWN_OFFSET_5 & ret );
    */

    histpos = (histpos + 1) % HISTORY_SIZE;
    reference = (reference + 1) % 5;

    if( !ret ) {
        /* No pulldown sequence is valid, return an error. */
        return 0;
    } else if( !(predicted & ret) ) {
        /**
         * We have a valid sequence, but it doesn't match our prediction.
         * Return the first 'valid' sequence in the list.
         */
        for( i = 0; i < 5; i++ ) { if( ret & (1<<i) ) return (1<<i); }
    }

    /**
     * The predicted phase is still valid.
     */
    return predicted;
}

int determine_pulldown_offset_short_history_new( int top_repeat, int bot_repeat, int tff, int predicted )
{
    int avgbot = 0;
    int avgtop = 0;
    int i, j;
    int ret;
    int mintopval = -1;
    int mintoppos = -1;
    int min2topval = -1;
    int min2toppos = -1;
    int minbotval = -1;
    int minbotpos = -1;
    int min2botval = -1;
    int min2botpos = -1;
    int predicted_pos = 0;

    tophistory[ histpos ] = top_repeat;
    bothistory[ histpos ] = bot_repeat;

    for( j = 0; j < 3; j++ ) {
        avgtop += tophistory[ (histpos + 5 - j) % 5 ];
        avgbot += bothistory[ (histpos + 5 - j) % 5 ];
    }
    avgtop /= 3;
    avgbot /= 3;

    for( i = 0; i < 5; i++ ) { if( (1<<i) == predicted ) { predicted_pos = i; break; } }

    /*
    fprintf( stderr, "top: %8d bot: %8d\ttop-avg: %8d bot-avg: %8d (%d)\n",
             top_repeat, bot_repeat, top_repeat - avgtop, bot_repeat - avgbot,
             (5 + predicted_pos - reference) % 5 );
    */

    for( j = 0; j < 3; j++ ) {
        int cur = tophistory[ (histpos + 5 - j) % 5 ];
        if( cur < mintopval || mintopval < 0 ) {
            min2topval = mintopval;
            min2toppos = mintoppos;
            mintopval = cur;
            mintoppos = j;
        } else if( cur < min2topval || min2topval < 0 ) {
            min2topval = cur;
            min2toppos = j;
        }
    }

    for( j = 0; j < 3; j++ ) {
        int cur = bothistory[ (histpos + 5 - j) % 5 ];
        if( cur < minbotval || minbotval < 0 ) {
            min2botval = minbotval;
            min2botpos = minbotpos;
            minbotval = cur;
            minbotpos = j;
        } else if( cur < min2botval || min2botval < 0 ) {
            min2botval = cur;
            min2botpos = j;
        }
    }

    tophistory_diff[ histpos ] = ((mintoppos == histpos) || (min2toppos == histpos));
    bothistory_diff[ histpos ] = ((minbotpos == histpos) || (min2botpos == histpos));

    ret = 0;
    for( i = 0; i < 5; i++ ) {
        int valid = 1;
        for( j = 0; j < 3; j++ ) {
            // if( tff_top_pattern[ j ] && !tophistory_diff[ (i + j) % 5 ] && tophistory[ (i + j) % 5 ] != mintopval ) {
            // if( tff_top_pattern[ j ] && (tophistory[ (i + j) % 5 ] > avgtop || !tophistory_diff[ (i + j) % 5 ]) ) {
            if( tff_top_pattern[ (i + 5 - j) % 5 ] && tophistory[ (histpos + 5 - j) % 5 ] > avgtop ) {
            // if( tff_top_pattern[ (i + 5 - j) % 5 ] && !tophistory_diff[ (histpos + 5 - j) % 5 ] && tophistory[ (histpos + 5 - j) % 5 ] != mintopval ) {
                valid = 0;
                break;
            }
            // if( tff_bot_pattern[ j ] && !bothistory_diff[ (i + j) % 5 ] && bothistory[ (i + j) % 5 ] != minbotval ) {
            // if( tff_bot_pattern[ j ] && (bothistory[ (i + j) % 5 ] > avgbot || !bothistory_diff[ (i + j) % 5 ]) ) {
            if( tff_bot_pattern[ (i + 5 - j) % 5 ] && bothistory[ (histpos + 5 - j) % 5 ] > avgbot ) {
            // if( tff_bot_pattern[ (i + 5 - j) % 5 ] && !bothistory_diff[ (histpos + 5 - j) % 5 ] && bothistory[ (histpos + 5 - j) % 5 ] != minbotval ) {
                valid = 0;
                break;
            }
        }
        if( valid ) ret |= (1<<i);
    }

    /*
    fprintf( stderr, "ret: %d %d %d %d %d\n",
             PULLDOWN_OFFSET_1 & ret,
             PULLDOWN_OFFSET_2 & ret,
             PULLDOWN_OFFSET_3 & ret,
             PULLDOWN_OFFSET_4 & ret,
             PULLDOWN_OFFSET_5 & ret );
    */

    histpos = (histpos + 1) % HISTORY_SIZE;
    reference = (reference + 1) % 5;

    if( !ret ) {
        /* No pulldown sequence is valid, return an error. */
        return 0;
    } else if( !(predicted & ret) ) {
        /**
         * We have a valid sequence, but it doesn't match our prediction.
         * Return the first 'valid' sequence in the list.
         */
        for( i = 0; i < 5; i++ ) { if( ret & (1<<i) ) return (1<<i); }
    }

    /**
     * The predicted phase is still valid.
     */
    return predicted;
}
