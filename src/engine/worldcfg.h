#pragma once

#include "cube.h"

namespace worldcfg
{
    struct mapsound_alt
    {
        string name;
        int volume;
    };

    struct mapsound_entry
    {
        string name;
        int volume;
        int maxuses;
        vector<mapsound_alt> alts;
    };

    int nummapsoundentries();
    bool collect_mapsound_entry(int idx, mapsound_entry &entry);

    enum sectionflags
    {
        SECTION_MAPVARS   = 1 << 0,
        SECTION_TEXTURES  = 1 << 1,
        SECTION_MAPMODELS = 1 << 2,
        SECTION_SOUNDS    = 1 << 3,
        SECTION_ALL       = SECTION_MAPVARS | SECTION_TEXTURES | SECTION_MAPMODELS | SECTION_SOUNDS
    };

    bool writemapcfg_sections(int sections = SECTION_ALL, const char *filename = NULL);
}
