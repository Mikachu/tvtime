
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

#define DO_LINE 11
static char outbuf[2048];
static int pll = 0;
static char *ccode = " !\"#$%&'()a+,-./0123456789:;<=>?@abcdefghijklmnopqrstuvwxyz[e]iouabcdefghijklmnopqrstuvwxyzc/Nn.";	/* this is NOT exactly right */

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

static void parse_xds_packet( const char *packet, int length )
{
    int i;

    fprintf( stderr, "XDS packet, class " );
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

    case 0xf: fprintf( stderr, "END\n" ); break;
    }
    for( i = 0; packet[ i ] != 0xf; i++ ) {
        fprintf( stderr, "0x%02x ", packet[ i ] );
    }
    fprintf( stderr, "\n" );
}

static void xds_decode( int b1, int b2 )
{
    if( xds_cursor > 2046 ) {
        xds_cursor = 0;
    }

    xds_packet[ xds_cursor ] = b1;
    xds_packet[ xds_cursor + 1 ] = b1;
    xds_cursor += 2;

    if( b1 == 0xf ) {
        parse_xds_packet( xds_packet, xds_cursor );
        xds_cursor = 0;
    }
}

int ProcessLine( unsigned char *s, int bottom )
{
    int w1, b1, b2;
    static int lastchar = 0, mode = 0;
    static int nocc = 0;
    int m, n;

    m = strlen(outbuf);
    w1 = ccdecode(s);
    if (!w1)
	nocc++;

    b1 = w1 & 0x7f;
    b2 = (w1 >> 8) & 0x7f;

    if( !b1 && !b2 ) return 0;

    if( bottom ) {
        xds_decode( b1, b2 );
    }

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
    if (!(b1 & 96) && b1 && *outbuf) {
	if (++mode > 4) {
	    fprintf(stderr, "%s\n", outbuf);
	    *outbuf = 0;
	    mode = 0;
	}
    }
    return n - m;
}				/* ProcessLine */


struct vbidata_s
{
    int fd;
    unsigned char buf[ 65536 ];
};

vbidata_t *vbidata_new( const char *filename )
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
    ProcessLine( &vbi->buf[ DO_LINE*2048 ], 0 );
    ProcessLine( &vbi->buf[ (16+DO_LINE)*2048 ], 1 );

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

