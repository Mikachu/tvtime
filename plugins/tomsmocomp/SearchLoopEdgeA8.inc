// -*- c++ -*-

// Searches 4 pixel to the left and right, in both the old
//  and new fields, but takes averages. These are even
// pixel addresses. Chroma match will be used. (YUY2)
        MERGE4PIXavg("-8(%%edi)", "8(%%esi, %%ecx, 2)")  // up left, down right
        MERGE4PIXavg("8(%%edi)", "-8(%%esi, %%ecx, 2)")  // up right, down left
        MERGE4PIXavg("-8(%%edi, %%ecx)", "8(%%esi, %%ecx)") // left, right
        MERGE4PIXavg("8(%%edi, %%ecx)", "-8(%%esi, %%ecx)") // right, left
        MERGE4PIXavg("-8(%%edi, %%ecx, 2)", "8(%%esi)")   // down left, up right
        MERGE4PIXavg("8(%%edi, %%ecx, 2)", "-8(%%esi)")   // down right, up left

