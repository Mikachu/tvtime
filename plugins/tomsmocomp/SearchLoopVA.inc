// -*- c++ -*-

// Searches the center vertical line above center and below, in both the old 
// and new fields, but takes averages.  These are even pixel addresses.
        MERGE4PIXavg("(%%edi, %%ecx, 2)", "(%%esi)")	// down, up
        MERGE4PIXavg("(%%edi)", "(%%esi, %%ecx, 2)")	// up, down
