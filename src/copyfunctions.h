/**
 * Copyright (C) 2005 Billy Biggs <vektor@dumbterm.net>.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef COPYFUNCTIONS_H_INCLUDED
#define COPYFUNCTIONS_H_INCLUDED

#if defined (__SVR4) && defined (__sun)
# include <sys/int_types.h>
#else
# include <stdint.h>
#endif

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Interpolates a packed 4:2:2 scanline using linear interpolation.
 */
extern void (*interpolate_packed422_scanline)( uint8_t *output, uint8_t *top,
                                               uint8_t *bot, int width );

/**
 * Blit from and to packed 4:2:2 scanline.
 */
extern void (*blit_packed422_scanline)( uint8_t *dest, const uint8_t *src, int width );


/**
 * Fast memcpy function, used by all of the blit functions.  Won't blit
 * anything if dest == src.
 */
extern void (*fast_memcpy)( void *output, const void *input, size_t size );

/**
 * Sets up the function pointers to point at the fastest function
 * available.  Requires accelleration settings (see mm_accel.h).
 */
void setup_copyfunctions( uint32_t accel );

/**
 * Returns a bitfield of what accellerations were used when speedy was
 * initialized.  See mm_accel.h.
 */
uint32_t copyfunctions_get_accel( void );


#ifdef __cplusplus
};
#endif
#endif /* COPYFUNCTIONS_H_INCLUDED */
