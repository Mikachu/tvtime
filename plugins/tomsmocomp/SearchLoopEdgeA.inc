// -*- c++ -*-

// Searches 2 pixel to the left and right, in both the old
//  and new fields, but takes averages. These are even
// pixel addresses. Chroma match will be used. (YUY2)
        MERGE4PIXavg("-4(%%edi)", "4(%%esi, %%ecx, 2)")  // up left, down right
        MERGE4PIXavg("4(%%edi)", "-4(%%esi, %%ecx, 2)")  // up right, down left
        MERGE4PIXavg("-4(%%edi, %%ecx)", "4(%%esi, %%ecx)") // left, right
        MERGE4PIXavg("4(%%edi, %%ecx)", "-4(%%esi, %%ecx)") // right, left
        MERGE4PIXavg("-4(%%edi, %%ecx, 2)", "4(%%esi)")   // down left, up right
        MERGE4PIXavg("4(%%edi, %%ecx, 2)", "-4(%%esi)")   // down right, up left
