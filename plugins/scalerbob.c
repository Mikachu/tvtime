/**
 * Dummy plugin for 'scalerbob' support.
 *
 * Copyright (C) 2002 Billy Biggs <vektor@dumbterm.net>.
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

#include <stdio.h>
#include "speedy.h"
#include "deinterlace.h"

static deinterlace_method_t scalerbobmethod =
{
    DEINTERLACE_PLUGIN_API_VERSION,
    "Television: Half Resolution",
    "TelevisionHalf",
    1,
    0,
    1,
    0,
    0,
    1,
    0,
    0,
    0,
    { "Expands each field independently without",
      "blurring or copying in time.  Use this if you",
      "want TV-quality with low CPU, and you have",
      "configured your monitor to run at the refresh",
      "rate of the video signal.",
      "",
      "Half resolution is poor quality but low CPU",
      "requirements for watching in a small window.",
      "",
      "" }
};

deinterlace_method_t *scalerbob_get_method( void )
{
    return &scalerbobmethod;
}

