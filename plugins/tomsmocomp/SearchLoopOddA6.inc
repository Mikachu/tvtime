// -*- c++ -*-

// Searches 3 pixels to the left and right, in both the old
//  and new fields, but takes averages. These are odd
// pixel addresses. Any chroma match will not be used. (YUY2)
        MERGE4PIXavg("-6(%%edi)", "6(%%esi, %%ecx, 2)")  // up left, down right
        MERGE4PIXavg("6(%%edi)", "-6(%%esi, %%ecx, 2)")  // up right, down left
        MERGE4PIXavg("-6(%%edi, %%ecx)", "6(%%esi, %%ecx)") // left, right
        MERGE4PIXavg("6(%%edi, %%ecx)", "-6(%%esi, %%ecx)") // right, left
        MERGE4PIXavg("-6(%%edi, %%ecx, 2)", "6(%%esi)")   // down left, up right
        MERGE4PIXavg("6(%%edi, %%ecx, 2)", "-6(%%esi)")   // down right, up left
