
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include "vbidata.h"
#include "console.h"

#define DO_LINE 11
static char outbuf[2048];
static int pll = 0;

struct vbidata_s
{
    int fd;
    unsigned char buf[ 65536 ];
    console_t *con;
};


/* this is NOT exactly right */
//static char *ccode = " !\"#$%&'()\0341+,-./0123456789:;<=>?@"
static char *ccode = " !\"#$%&'()a+,-./0123456789:;<=>?@"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
//                     "abcdefghijklmnopqrstuvwxyz"
//                     "[\0351]\0355\0363\0372abcdefghijklmnopqr"
                     "[e]iouabcdefghijklmnopqr"
//                     "stuvwxyz\0347\0367\0245\0244\0240";
                     "stuvwxyzcoNn ";
static char *wccode = "\0256\0260\0275\0277T\0242\0243#\0340 "
                      "\0350\0354\0362\0371";

static char *extcode1 = "\0301\0311\0323\0332\0334\0374"
                       "`\0241*'-\0251S*\"\"\0300\0302"
                       "\0307\0310\0312\0313\0353\0316\0317\0357"
                       "\0324\0331\0371\0333\0253\0273";

static char *extcode2 = "\0303\0343\0315\0314\0354\0322\0362\0325"
                        "{}\\^_|~\0304\0344\0326\0366\0337\0245\0244|"
                        "\0305\0345\0330\0370++++";

int parityok(int n)
{				/* check parity for 2 bytes packed in n */
    int j, k;
    for (k = 0, j = 0; j < 7; j++)
        if (n & (1 << j))
            k++;
    if ((k & 1) && (n & 0x80))
        return 0;
    for (k = 0, j = 8; j < 15; j++)
        if (n & (1 << j))
            k++;
    if ((k & 1) && (n & 0x8000))
        return 0;
    return 1;
}

int decodebit(unsigned char *data, int threshold)
{
    return ((data[0] + data[1] + data[2] + data[3] + data[4] + data[5] +
             data[6] + data[7] + data[8] + data[9] + data[10] + data[11] +
             data[12] + data[13] + data[14] + data[15] + data[16] + data[17] +
             data[18] + data[19] + data[20] + data[21] + data[22] + data[23] +
             data[24] + data[25] + data[26] + data[27] + data[28] + data[29] +
             data[30] + data[31])>>5 > threshold);
}


int ccdecode(unsigned char *vbiline)
{
    int max = 0, maxval = 0, minval = 255, i = 0, clk = 0, tmp = 0;
    int sample, packedbits = 0;

    for (i=0; i<250; i++) {
        sample = vbiline[i];
        if (sample - maxval > 10)
            (maxval = sample, max = i);
        if (sample < minval)
            minval = sample;
        if (maxval - sample > 40)
            break;
    }
    sample = ((maxval + minval) >> 1);
    pll = max;

    /* found clock lead-in, double-check start */
#ifndef PAL_DECODE
    i = max + 478;
#else
    i = max + 538;
#endif
    if (!decodebit(&vbiline[i], sample))
        return 0;
#ifndef PAL_DECODE
    tmp = i + 57;		/* tmp = data bit zero */
#else
    tmp = i + 71;
#endif
    for (i = 0; i < 16; i++) {
#ifndef PAL_DECODE
        clk = tmp + i * 57;
#else
        clk = tmp + i * 71;
#endif
        if (decodebit(&vbiline[clk], sample)) {
            packedbits |= 1 << i;
        }
    }
    if (parityok(packedbits))
        return packedbits;
    return 0;
}				/* ccdecode */

static char xds_packet[ 2048 ];
static int xds_cursor = 0;

const char *movies[] = { "N/A", "G", "PG", "PG-13", "R", 
                         "NC-17", "X", "Not Rated" };
const char *usa_tv[] = { "Not Rated", "TV-Y", "TV-Y7", "TV-G", 
                         "TV-PG", "TV-14", "TV-MA", "Not Rated" };
const char *cane_tv[] = { "Exempt", "C", "C8+", "G", "PG", 
                          "14+", "18+", "Reserved" };
const char *canf_tv[] = { "Exempt", "G", "8 ans +", "13 ans +", 
                          "16 ans +", "18 ans +", "Reserved", 
                          "Reserved" };


const char *months[] = { 0, "Jan", "Feb", "Mar", "Apr", "May",
    "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

static const char *eia608_program_type[ 96 ] =
{
	"education",
	"entertainment",
	"movie",
	"news",
	"religious",
	"sports",
	"other",
	"action",
	"advertisement",
	"animated",
	"anthology",
	"automobile",
	"awards",
	"baseball",
	"basketball",
	"bulletin",
	"business",
	"classical",
	"college",
	"combat",
	"comedy",
	"commentary",
	"concert",
	"consumer",
	"contemporary",
	"crime",
	"dance",
	"documentary",
	"drama",
	"elementary",
	"erotica",
	"exercise",
	"fantasy",
	"farm",
	"fashion",
	"fiction",
	"food",
	"football",
	"foreign",
	"fund raiser",
	"game/quiz",
	"garden",
	"golf",
	"government",
	"health",
	"high school",
	"history",
	"hobby",
	"hockey",
	"home",
	"horror",
	"information",
	"instruction",
	"international",
	"interview",
	"language",
	"legal",
	"live",
	"local",
	"math",
	"medical",
	"meeting",
	"military",
	"miniseries",
	"music",
	"mystery",
	"national",
	"nature",
	"police",
	"politics",
	"premiere",
	"prerecorded",
	"product",
	"professional",
	"public",
	"racing",
	"reading",
	"repair",
	"repeat",
	"review",
	"romance",
	"science",
	"series",
	"service",
	"shopping",
	"soap opera",
	"special",
	"suspense",
	"talk",
	"technical",
	"tennis",
	"travel",
	"variety",
	"video",
	"weather",
	"western"
};


static void parse_xds_packet( const char *packet, int length )
{
    int sum = 0;
    int i;

    /* Check the checksum for validity of the packet. */
    for( i = 0; i < length - 1; i++ ) {
        sum += packet[ i ];
    }
    if( (((~sum) & 0x7f) + 1) != packet[ length - 1 ] ) {
        return;
    }

    /* Stick a null at the end, and cut off the last two characters. */
    packet[ length - 2 ] = '\0';
    length -= 2;

    if( packet[ 0 ] < 0x03 && packet[ 1 ] == 0x03 ) {
        fprintf( stderr, "Current program name: '%s'\n", packet + 2 );
    } else if( packet[ 0 ] < 0x05 && packet[ 1 ] == 0x03 ) {
        fprintf( stderr, "Future program name: '%s'\n", packet + 2 );
    } else if( packet[ 0 ] == 0x05 && packet[ 1 ] == 0x01 ) {
        fprintf( stderr, "Network name: '%s'\n", packet + 2 );
    } else if( packet[ 0 ] == 0x01 && packet[ 1 ] == 0x05 ) {
        int movie_rating = packet[ 2 ] & 7;
        int scheme = (packet[ 2 ] & 56) >> 3;
        int tv_rating = packet[ 3 ] & 7;
        int VSL = packet[ 3 ] & 56;
        const char * str;

        switch( VSL | scheme ) {
        case 3: /* Canadian English TV */
            str = cane_tv[ tv_rating ];
            break;
        case 7: /* Canadian French TV */
            str = canf_tv[ tv_rating ];
            break;
        case 19: /* Reserved */
        case 31:
            str = "";
            break;
        default:
            if( ((VSL | scheme) & 3) == 1 ) {
                /* USA TV */
                str = usa_tv[ tv_rating ];
            } else {
                /* MPAA Movie Rating */
                str = movies[ movie_rating ];
            }
            break;
        }
        fprintf( stderr, "Show rating: %s", str );
        if( ((VSL | scheme) & 3) == 1 || ((VSL | scheme) & 3) == 0 ) {
            /* show VSLD for the americans */
            if( (VSL | scheme) & 32 ) {
                fprintf( stderr, " V" );
            }
            if( (VSL | scheme) & 16 ) {
                fprintf( stderr, " S" );
            }
            if( (VSL | scheme) & 8 ) {
                fprintf( stderr, " L" );
            }
            if( (VSL | scheme) & 4 ) {
                fprintf( stderr, " D" );
            }
        }
        fprintf( stderr, "\n" );
    } else if( packet[ 0 ] == 0x05 && packet[ 1 ] == 0x02 ) {
        fprintf( stderr, "Network call letters: '%s'\n", packet + 2 );
    } else if( packet[ 0 ] == 0x01 && packet[ 1 ] == 0x01 ) {
                        int month = packet[5];// & 15;
                        int day = packet[4];// & 31;
                        int hour = packet[3];// & 31;
                        int min = packet[2];// & 63;
        fprintf( stderr, "Program Start: %02d %s, %02d:%02d\n",
                 day & 31, months[month & 15], hour & 31, min & 63 );
                 // packet[ 3 ], packet[ 4 ], packet[ 5 ], packet[ 6 ] );
                 //packet[ 5 ] & 31, packet[ 6 ], packet[ 4 ] & 31, packet[ 3 ] & 63 );
    } else if( packet[ 0 ] == 0x01 && packet[ 1 ] == 0x04 ) {
        fprintf( stderr, "Program type: " );
        for( i = 0; i < length - 2; i++ ) {
            int cur = packet[ 2 + i ] - 0x20;
            if( cur >= 0 && cur < 96 ) {
                fprintf( stderr, "%s%s", i ? ", " : "", eia608_program_type[ cur ] );
            }
        }
        fprintf( stderr, "\n" );
    } else if( packet[ 0 ] < 0x03 && packet[ 1 ] >= 0x10 && packet[ 1 ] <= 0x17 ) {
        fprintf( stderr, "Program Description: Line %d", packet[1] & 0xf );
        fprintf( stderr, "%s\n", packet + 2 );
        
    } else if( packet[ 0 ] < 0x03 && packet[ 1 ] == 0x02 ) {
        fprintf( stderr, "Program Length: Length: %02d:%02d", packet[ 3 ] & 63, 
                 packet[ 2 ] & 63 ); 
        if( length > 4 ) {
            fprintf( stderr, " Elapsed: %02d:%02d", packet[ 5 ] & 63, 
                     packet[ 4 ] & 63 );
        }
        if( length > 6 ) {
            fprintf( stderr, ".%02d", packet[ 6 ] & 63 );
        }
        fprintf( stderr, "\n" );
    } else {
        /* unknown */

        fprintf( stderr, "Unknown XDS packet, class " );
        switch( packet[ 0 ] ) {
        case 0x1: fprintf( stderr, "CURRENT start\n" ); break;
        case 0x2: fprintf( stderr, "CURRENT continue\n" ); break;

        case 0x3: fprintf( stderr, "FUTURE start\n" ); break;
        case 0x4: fprintf( stderr, "FUTURE continue\n" ); break;

        case 0x5: fprintf( stderr, "CHANNEL start\n" ); break;
        case 0x6: fprintf( stderr, "CHANNEL continue\n" ); break;

        case 0x7: fprintf( stderr, "MISC start\n" ); break;
        case 0x8: fprintf( stderr, "MISC continue\n" ); break;

        case 0x9: fprintf( stderr, "PUB start\n" ); break;
        case 0xa: fprintf( stderr, "PUB continue\n" ); break;

        case 0xb: fprintf( stderr, "RES start\n" ); break;
        case 0xc: fprintf( stderr, "RES continue\n" ); break;

        case 0xd: fprintf( stderr, "UNDEF start\n" ); break;
        case 0xe: fprintf( stderr, "UNDEF continue\n" ); break;
        }
        for( i = 0; i < length; i++ ) {
            fprintf( stderr, "0x%02x ", packet[ i ] );
        }
        fprintf( stderr, "\n" );
    }
}

static int xds_decode( int b1, int b2 )
{
    if( xds_cursor > 2046 ) {
        xds_cursor = 0;
    }

    if( !xds_cursor && b1 > 0xf ) {
        return 0;
    }

    xds_packet[ xds_cursor ] = b1;
    xds_packet[ xds_cursor + 1 ] = b2;
    xds_cursor += 2;

    if( b1 == 0xf ) {
        parse_xds_packet( xds_packet, xds_cursor );
        xds_cursor = 0;
    }

    return 1;
}

int ProcessLine( vbidata_t *vbi, unsigned char *s, int bottom )
{
    int w1, b1, b2;
    static int lastchar = 0, mode = 0, incc=0;
    static int nocc = 0, lastcc=0;
    int m=0, n=0;

    if( !vbi ) return 0;

    m = strlen(outbuf);
    w1 = ccdecode(s);
    if (!w1)
        nocc++;

    b1 = w1 & 0x7f;
    b2 = (w1 >> 8) & 0x7f;

    if( !b1 && !b2 ) return 0;

    if( b1 >= 0x10 && b1 <= 0x1F && b2 >= 0x20 && b2 <= 0x7F ) {
//        fprintf( stderr, "control code: 0x%02x 0x%02x\n", b1, b2 );
        fprintf( stderr, "Channel: %d ", (b1 & 8) >> 4 );
        if( b1 & 2 ) {
            fprintf( stderr, "Tab Offset: %d ", b2 & 2 );
            return 0;
        }
        fprintf( stderr, "Field: %d Cmd: ", b1 & 1 );
        switch( b2 & 7 ) {
        case 0:
            fprintf( stderr, "Resume Caption Loading\n");
            lastcc = incc;
            incc=1;
            if ( *outbuf ) {
                fprintf(stderr, "%s\n", outbuf);
                if( vbi->con ) {
                    console_printf( vbi->con, "%s\n", outbuf );
                }

                *outbuf = 0;
                mode = 0;
            }

            break;
        case 1:
            lastchar = 1;
            fprintf( stderr, "Backspace\n");
            if( *outbuf ) {
                outbuf[ strlen(outbuf) - 1 ] = 0;
            }
            break;
        case 2:
        case 3:
            fprintf( stderr, "Reserved\n");
            break;
        case 4:
            fprintf( stderr, "Delete to End of Row\n");
            lastcc = incc;
            incc=0;
            *outbuf = 0;
            mode = 0;
            break;
        case 5:
            fprintf( stderr, "Roll Up Captions, 2 rows\n");
            lastcc = incc;
            incc=1;
            if ( *outbuf ) {
                fprintf(stderr, "%s\n", outbuf);
                if( vbi->con ) {
                    console_printf( vbi->con, "%s\n", outbuf );
                }

                *outbuf = 0;
                mode = 0;
            }

            break;
        case 6:
            fprintf( stderr, "Roll Up Captions, 3 rows\n");
            lastcc = incc;
            incc=1;
            if ( *outbuf ) {
                fprintf(stderr, "%s\n", outbuf);
                if( vbi->con ) {
                    console_printf( vbi->con, "%s\n", outbuf );
                }

                *outbuf = 0;
                mode = 0;
            }

            break;
        case 7:
            fprintf( stderr, "Roll Up Captions, 4 rows\n");
            lastcc = incc;
            incc=1;
            if ( *outbuf ) {
                fprintf(stderr, "%s\n", outbuf);
                if( vbi->con ) {
                    console_printf( vbi->con, "%s\n", outbuf );
                }

                *outbuf = 0;
                mode = 0;
            }

            break;
        case 8:
            fprintf( stderr, "Flash On\n");
            break;
        case 9:
            fprintf( stderr, "Resume Direct Captioning\n");
            break;
        case 10:
            fprintf( stderr, "Text Restart\n");
            break;
        case 11:
            fprintf( stderr, "Resume Text Display\n");
            break;
        case 12:
            fprintf( stderr, "Erase Displayed Memory\n");
            break;
        case 13:
            fprintf( stderr, "Carriage Return\n");
            lastcc = incc;
            incc=0;
            if ( *outbuf ) {
                fprintf(stderr, "%s\n", outbuf);
                if( vbi->con ) {
                    console_printf( vbi->con, "%s\n", outbuf );
                }
                *outbuf = 0;
                mode = 0;
            }

            break;
        case 14:
            fprintf( stderr, "Erase Non-Displayed Memory\n");
            break;
        case 15:
            fprintf( stderr, "End Of Caption\n");
            lastcc = incc;
            incc=0;
            if ( *outbuf ) {
                fprintf(stderr, "%s\n", outbuf);
                if( vbi->con ) {
                    console_printf( vbi->con, "%s\n", outbuf );
                }

                *outbuf = 0;
                mode = 0;
            }

            break;
        default:
            break;
        }

        return 0;
    }

    if( bottom && xds_decode( b1, b2 ) ) return 0;

/*
    fprintf( stderr, "cc: 0x%0x 0x%0x, %d%d%d%d%d%d%d%d %d%d%d%d%d%d%d%d, '%c' '%c'\n",
            b1, b2,
            b1 >> 7 & 1,
            b1 >> 6 & 1,
            b1 >> 5 & 1,
            b1 >> 4 & 1,
            b1 >> 3 & 1,
            b1 >> 2 & 1,
            b1 >> 1 & 1,
            b1 >> 0 & 1,
            b2 >> 7 & 1,
            b2 >> 6 & 1,
            b2 >> 5 & 1,
            b2 >> 4 & 1,
            b2 >> 3 & 1,
            b2 >> 2 & 1,
            b2 >> 1 & 1,
            b2 >> 0 & 1, b1, b2);
*/
    fprintf( stderr, "b1 = 0x%02x  b2 = 0x%02x\n", b1, b2 );
    if( b1 == 0x11 || b1 == 0x19 || 
        b1 == 0x12 || b1 == 0x13 || 
        b1 == 0x1A || b1 == 0x1B ) {
        switch( b1 ) {
        case 0x1A:
        case 0x12:
            /* use extcode1 */
            if( b1 > 31 && b2 > 31 && b1 <= 0x3F && b2 <= 0x3F )
                fprintf( stderr, "char %d (%c),  char %d (%c)\n", b1, extcode1[b1-32] , b2, extcode1[b2-32] );

            break;
        case 0x13:
        case 0x1B:
            /* use extcode2 */
            if( b1 > 31 && b2 > 31 && b1 <= 0x3F && b2 <= 0x3F )
                fprintf( stderr, "char %d (%c),  char %d (%c)\n", b1, extcode2[b1-32] , b2, extcode2[b2-32] );

            break;
        case 0x11:
        case 0x19:
            /* use wcode */
            if( b1 > 31 && b2 > 31 && b1 <= 0x3F && b2 <= 0x3F )
                fprintf( stderr, "char %d (%c),  char %d (%c)\n", b1, wccode[b1-32] , b2, wccode[b2-32] );

            break;
        default:
            
            break;
        }
    } else if( b1  ) {
        /* use ccode */
        if( lastchar == 1 ) {
            /* backspace */
        }
        if( b1 > 31 && incc ) {
            char blah = b1;
            //fprintf( stderr, "char %d [%c] (%c)\n",  b1, b1, ccode[b1-32] );
            strncat(outbuf, &ccode[blah-32], 1 );
        }
        if( b2 > 31 && incc ) {
            char blah = b2;
//            fprintf( stderr, "char %d [%c] (%c)\n",  b2, b2, ccode[b2-32] );
            strncat(outbuf, &ccode[blah-32], 1 );
        }

        if ( 0 && *outbuf && incc )
            if (outbuf[strlen(outbuf) - 1] != ' ')
                strncat(outbuf, ccode, 1);
        n = strlen(outbuf);

        if( !incc )
            fprintf( stderr, "Not in CC\n");

//        fprintf(stderr, "CC: %s\n", outbuf);
    }



#if 0
    if ((b1 & 96)) {
        if (b1 > 31) {
            strncat(outbuf, &ccode[b1 - 32], 1);
            if (lastchar == '.' || lastchar == '['
                || lastchar == '>' || lastchar == ']'
                || lastchar == '!' || lastchar == '?')
                outbuf[strlen(outbuf) - 1] =
                    toupper(ccode[b1 - 32]);
            if (b1 > 32)
                lastchar = ccode[b1 - 32];
        }
        if (b2 > 31) {
            strncat(outbuf, &ccode[b2 - 32], 1);
            if (lastchar == '.' || lastchar == '['
                || lastchar == '>' || lastchar == ']'
                || lastchar == '!' || lastchar == '?')
                outbuf[strlen(outbuf) - 1] =
                    toupper(ccode[b2 - 32]);
            if (b2 > 32)
                lastchar = ccode[b2 - 32];
        }
    }
    if (!(b1 & 96) && b1 && *outbuf)
        if (outbuf[strlen(outbuf) - 1] != ' ')
            strncat(outbuf, ccode, 1);
    n = strlen(outbuf);

#endif

    return n - m;
}				/* ProcessLine */



vbidata_t *vbidata_new( const char *filename, console_t *con  )
{
    vbidata_t *vbi = (vbidata_t *) malloc( sizeof( vbidata_t ) );
    if( !vbi ) {
        return 0;
    }

    vbi->fd = open( filename, O_RDONLY );
    if( vbi->fd < 0 ) {
        fprintf( stderr, "vbidata: Can't open %s: %s\n",
                 filename, strerror( errno ) );
        free( vbi );
        return 0;
    }

    vbi->con = con;

    return vbi;
}

void vbidata_delete( vbidata_t *vbi )
{
    free( vbi );
}

void vbidata_process_frame( vbidata_t *vbi, int printdebug )
{
    int i, j;

    if( read( vbi->fd, vbi->buf, 65536 ) < 65536 ) {
        fprintf( stderr, "error, can't read vbi data.\n" );
        return;
    }

    ProcessLine( vbi, &vbi->buf[ DO_LINE*2048 ], 0 );
    ProcessLine( vbi, &vbi->buf[ (16+DO_LINE)*2048 ], 1 );


/*
    if( printdebug ) {
        for( i = 0; i < 32; i++ ) {
            for( j = 0; j < 2048; j++ ) {
                fprintf( stdout, "%d %d\n", (i*2048) + j, vbi->buf[ (i*2048) + j ] );
            }
        }
    }
*/
}

