// -*- c++ -*-

// Searches the center vertical line above center and below, in both the old 
// and new fields, but takes averages.  These are even pixel addresses.
        MERGE4PIXavgH("(%%edi, %%ecx, 2)", "(%%edi, %%ecx)", "(%%esi, %%ecx)", "(%%esi)")	// down, up
        MERGE4PIXavgH("(%%edi)", "(%%edi, %%ecx)", "(%%esi, %%ecx)", "(%%esi, %%ecx, 2)")	// up, down
