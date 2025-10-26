#include "engine.h"
#include "worldcfg.h"

extern vector<int> smoothgroups;

namespace worldcfg
{
    struct soundwriteresult
    {
        int written;
        int failed;
        bool hadcontent;

        soundwriteresult() : written(0), failed(0), hadcontent(false) {}
    };

    static bool write_mapvars_section(stream *f, bool prependnewline)
    {
        bool wrote = false;
        enumerate(idents, ident, id,
        {
            if(!(id.flags & IDF_OVERRIDE) || id.flags & IDF_READONLY || !(id.flags & IDF_OVERRIDDEN)) continue;

            if(id.type == ID_VAR || id.type == ID_FVAR || id.type == ID_SVAR)
            {
                if(!wrote)
                {
                    if(prependnewline) f->printf("\n");
                    f->printf("// mapvars\n");
                    wrote = true;
                }
                switch(id.type)
                {
                    case ID_VAR:
                        f->printf("%s %d\n", escapeid(id), *id.storage.i);
                        break;
                    case ID_FVAR:
                        f->printf("%s %s\n", escapeid(id), floatstr(*id.storage.f));
                        break;
                    case ID_SVAR:
                        f->printf("%s %s\n", escapeid(id), escapestring(*id.storage.s));
                        break;
                    default:
                        break;
                }
            }
        });
        return wrote;
    }

    static inline const char *strippackagesprefix(const char *name)
    {
        if(!name) return "";

        const char *path = name;
        while(*path == '<')
        {
            const char *end = strchr(path, '>');
            if(!end) break;
            path = end + 1;
        }

        if(!strncmp(path, "packages/", 9))
        {
            static string tmp;
            copystring(tmp, name);

            size_t prefixlen = path - name;
            size_t tmplen = strlen(tmp);
            if(tmplen >= prefixlen + 9)
                memmove(&tmp[prefixlen], &tmp[prefixlen + 9], tmplen - prefixlen - 9 + 1);

            return tmp;
        }

        return name;
    }

    static bool write_mapmodel_section(stream *f, bool prependnewline)
    {
        bool wrote = false;
        loopv(mapmodels)
        {
            mapmodelinfo &mmi = mapmodels[i];
            if(!mmi.name[0]) continue;
            if(!wrote)
            {
                if(prependnewline) f->printf("\n");
                f->printf("// mapmodels\n");
                f->printf("mapmodelreset\n\n");
                wrote = true;
            }
            f->printf("// model %d\n", i);
            f->printf("mmodel %s\n", escapestring(mmi.name));
        }
        return wrote;
    }

    static inline bool fequal(float a, float b, float eps = 1e-6f)
    {
        return fabs(a - b) <= eps;
    }

    static void write_texture_section(stream *f)
    {
        loopi(slots.length())
        {
            Slot *slotptr = slots[i];
            if(!slotptr || slotptr->sts.empty() || !slotptr->variants) continue;
            Slot &slot = *slotptr;
            VSlot &vs = *slot.variants;

            f->printf("// slot %d\n", i);

            const char *shadername = (slot.shader && slot.shader->name) ? slot.shader->name :
                                     (stdworldshader && stdworldshader->name ? stdworldshader->name : "stdworld");
            f->printf("setshader %s\n", escapestring(shadername));

            loopvj(slot.params)
            {
                SlotShaderParam &param = slot.params[j];
                if(!param.name) continue;
                f->printf("setshaderparam %s %.6f %.6f %.6f %.6f\n",
                    escapestring(param.name),
                    param.val[0], param.val[1], param.val[2], param.val[3]);
            }

            loopvj(slot.sts)
            {
                Slot::Tex &tex = slot.sts[j];
                const char *texname = strippackagesprefix(tex.name);
                f->printf("texture %d %s %d %d %d %.6f\n",
                    tex.type,
                    escapestring(texname),
                    vs.rotation & 7,
                    vs.offset.x,
                    vs.offset.y,
                    vs.scale);
            }

            if(slot.autograss && *slot.autograss)
                f->printf("autograss %s\n", escapestring(strippackagesprefix(slot.autograss)));

            if(slot.smooth >= 0)
            {
                int angle = smoothgroups.inrange(slot.smooth) ? smoothgroups[slot.smooth] : -1;
                if(angle >= 0) f->printf("texsmooth %d %d\n", slot.smooth, angle);
            }

            if(!fequal(vs.scroll.x, 0.0f) || !fequal(vs.scroll.y, 0.0f))
                f->printf("texscroll %.6f %.6f\n", vs.scroll.x*1000.0f, vs.scroll.y*1000.0f);

            if(vs.layer || (slot.layermaskname && *slot.layermaskname) || slot.layermaskmode || !fequal(slot.layermaskscale, 1.0f))
            {
                const char *maskname = strippackagesprefix(slot.layermaskname ? slot.layermaskname : "");
                f->printf("texlayer %d %s %d %.6f\n", vs.layer, escapestring(maskname), slot.layermaskmode, slot.layermaskscale);
            }

            if(!fequal(vs.alphafront, 0.5f) || !fequal(vs.alphaback, 0.0f))
                f->printf("texalpha %.6f %.6f\n", vs.alphafront, vs.alphaback);

            if(!fequal(vs.colorscale.x, 1.0f) || !fequal(vs.colorscale.y, 1.0f) || !fequal(vs.colorscale.z, 1.0f))
                f->printf("texcolor %.6f %.6f %.6f\n", vs.colorscale.x, vs.colorscale.y, vs.colorscale.z);

            f->printf("\n");
        }
    }

    static soundwriteresult write_sound_section(stream *f, bool prependnewline)
    {
        soundwriteresult result;
        int total = nummapsoundentries();
        loopi(total)
        {
            mapsound_entry entry;
            entry.alts.setsize(0);
            if(!collect_mapsound_entry(i, entry))
            {
                ++result.failed;
                continue;
            }

            if(!result.hadcontent)
            {
                if(prependnewline) f->printf("\n");
                f->printf("// sounds\n");
                f->printf("mapsoundreset\n\n");
                result.hadcontent = true;
            }

            ++result.written;
            f->printf("// sound %d\n", i);
            f->printf("mapsound %s %d %d\n", escapestring(entry.name), entry.volume, entry.maxuses);
            loopvj(entry.alts)
            {
                mapsound_alt &alt = entry.alts[j];
                f->printf("altmapsound %s %d\n", escapestring(alt.name), alt.volume);
            }
            //f->printf("\n");
        }
        return result;
    }

    static void choose_default_basename(int sections, const char *cfgname, string &out)
    {
        if(sections == SECTION_MAPVARS) formatstring(out, "%s_mapvars", cfgname);
        else if(sections == SECTION_TEXTURES) formatstring(out, "%s_textures", cfgname);
        else if(sections == SECTION_MAPMODELS) formatstring(out, "%s_mmodels", cfgname);
        else if(sections == SECTION_SOUNDS) formatstring(out, "%s_sounds", cfgname);
        else formatstring(out, "%s", cfgname);
    }

    static void build_output_path(string &out, const char *filename, const char *defaultbase)
    {
        string chosen;
        if(filename && *filename) copystring(chosen, filename);
        else copystring(chosen, defaultbase);

        const char *ext = strrchr(chosen, '.');
        if(!ext || strcasecmp(ext, ".cfg")) concatstring(chosen, ".cfg");

        string rel;
        formatstring(rel, "packages/base/%s", chosen);
        copystring(out, path(rel));
    }

    static bool writemapcfg_internal(int sections, const char *filename, bool logsoundinfo)
    {
        const char *map = game::getclientmap();
        if (!map || !*map) map = "untitled";

        string pakname, mapname, cfgname;
        getmapfilenames(map, NULL, pakname, mapname, cfgname);

        string defaultbase, fullpath;
        choose_default_basename(sections, cfgname, defaultbase);
        build_output_path(fullpath, filename, defaultbase);

        stream *f = openutf8file(fullpath, "w");
        if(!f)
        {
            conoutf(CON_ERROR, "could not write %s", map);
            return false;
        }

        f->printf("// autogenerated map config for map %s\n\n", map);

        bool wrote_any = false;
        if(sections & SECTION_MAPVARS)
        {
            if(write_mapvars_section(f, wrote_any)) wrote_any = true;
        }

        if (sections & SECTION_MAPMODELS)
        {
            if (write_mapmodel_section(f, wrote_any)) wrote_any = true;
        }

        if (sections & SECTION_SOUNDS)
        {
            soundwriteresult result = write_sound_section(f, wrote_any);
            if (result.hadcontent) wrote_any = true;

            if (result.failed > 0)
                // we'll probably never get here, but if we do...
                conoutf(CON_WARN, "skipped %d map sound%s when writing %s", result.failed, result.failed == 1 ? "" : "s", fullpath);
            //if (logsoundinfo && result.hadcontent)
            //    conoutf(CON_INFO, "wrote %d map sound%s to %s", result.written, result.written == 1 ? "" : "s", fullpath);
        }

        if(sections & SECTION_TEXTURES)
        {
            if(wrote_any) f->printf("\n");
            f->printf("// textures\n");
            f->printf("texturereset\n\n");
            write_texture_section(f);
            wrote_any = true;
        }

        delete f;

        const char *label = "map config";
        if(sections == SECTION_MAPVARS) label = "map variables config";
        else if(sections == SECTION_MAPMODELS) label = "mapmodel config";
        else if(sections == SECTION_SOUNDS) label = "sound config";
        else if(sections == SECTION_TEXTURES) label = "texture config";

        conoutf(CON_INFO, "wrote %s to %s", label, fullpath);
        return true;
    }

    bool writemapcfg_sections(int sections, const char *filename)
    {
        if(!sections) sections = SECTION_ALL;
        sections &= SECTION_ALL;
        if(!sections) sections = SECTION_ALL;

        bool logsoundinfo = (sections & SECTION_SOUNDS) && !(sections & (SECTION_MAPVARS | SECTION_TEXTURES | SECTION_MAPMODELS));
        const char *name = (filename && *filename) ? filename : NULL;

        return writemapcfg_internal(sections, name, logsoundinfo);
    }
}

static void writemapcfg_cmd(char *filename)
{
    worldcfg::writemapcfg_sections(worldcfg::SECTION_ALL, filename && *filename ? filename : NULL);
}

static void writetexturescfg_cmd(char *filename)
{
    worldcfg::writemapcfg_sections(worldcfg::SECTION_TEXTURES, filename && *filename ? filename : NULL);
}

static void writemapvarscfg_cmd(char *filename)
{
    worldcfg::writemapcfg_sections(worldcfg::SECTION_MAPVARS, filename && *filename ? filename : NULL);
}

static void writemmodelscfg_cmd(char *filename)
{
    worldcfg::writemapcfg_sections(worldcfg::SECTION_MAPMODELS, filename && *filename ? filename : NULL);
}

static void writesoundscfg_cmd(char *filename)
{
    worldcfg::writemapcfg_sections(worldcfg::SECTION_SOUNDS, filename && *filename ? filename : NULL);
}

ICOMMAND(writemapcfg, "s", (char *filename), writemapcfg_cmd(filename));
ICOMMAND(writetexturescfg, "s", (char *filename), writetexturescfg_cmd(filename));
ICOMMAND(writemapvarscfg, "s", (char *filename), writemapvarscfg_cmd(filename));
ICOMMAND(writemmodelscfg, "s", (char *filename), writemmodelscfg_cmd(filename));
ICOMMAND(writesoundscfg, "s", (char *filename), writesoundscfg_cmd(filename));
