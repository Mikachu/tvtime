// -*- c++ -*-

// Searches 1 pixel to the left and right, in both the old
//  and new fields, but takes averages. These are odd
// pixel addresses. Any chroma match will not be used. (YUY2)
        MERGE4PIXavg("-2(%%edi)", "2(%%esi, %%ecx, 2)")  // up left, down right
        MERGE4PIXavg("2(%%edi)", "-2(%%esi, %%ecx, 2)")  // up right, down left
        MERGE4PIXavg("-2(%%edi, %%ecx, 2)", "2(%%esi)")   // down left, up right
        MERGE4PIXavg("2(%%edi, %%ecx, 2)", "-2(%%esi)")   // down right, up left   
#include "SearchLoopOddA2.inc"
