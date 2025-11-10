// SauerWUI - geometry templates
#include "engine.h"
#include "edittemplates.h"

extern selinfo sel;
extern void boxs3D(const vec &o, vec s, int g);
extern block3 *blockcopy(const block3 &s, int rgrid);
extern void freeblock(block3 *b, bool alloced = true);
extern void pasteblock(block3 &b, selinfo &sel, bool local);
extern void changed(const block3 &sel, bool commit);

namespace edittemplates
{
    struct region
    {
        ivec o, s;
        int grid, orient;
        region() : o(0, 0, 0), s(0, 0, 0), grid(0), orient(0) {}
        bool valid() const { return s.x > 0 && s.y > 0 && s.z > 0; }
    };

    struct templategroup
    {
        int id;
        vector<region> regions;
        templategroup() : id(-1) {}
    };

    static vector<templategroup> groups;
    static block3 *clipboard = NULL;
    static region clipregion;
    static int activegroup = -1;
    static int nextgroupid = 1;
    static bool debugdraw = false;
    static bool propagating = false;
    static void resetstate();
    static void persistgroups();
    static void loadgroupsfromstring(const char *data);
    static void cleartemplateclipboard();

    static inline region makeregion(const selinfo &s)
    {
        region r;
        r.o = s.o;
        r.s = s.s;
        r.grid = s.grid;
        r.orient = s.orient;
        return r;
    }

    static inline region makeregion(const block3 &b)
    {
        region r;
        r.o = b.o;
        r.s = b.s;
        r.grid = b.grid;
        r.orient = b.orient;
        return r;
    }

    static inline bool sameregion(const region &a, const region &b)
    {
        return a.o == b.o && a.s == b.s && a.grid == b.grid;
    }

    static inline ivec regionextent(const region &r)
    {
        return ivec(r.s).mul(r.grid);
    }

    static inline ivec blockextent(const block3 &b)
    {
        return ivec(b.s).mul(b.grid);
    }

    static inline void regionbounds(const region &r, ivec &bbmin, ivec &bbmax)
    {
        bbmin = r.o;
        bbmax = ivec(r.o).add(regionextent(r));
    }

    static inline void blockbounds(const block3 &b, ivec &bbmin, ivec &bbmax)
    {
        bbmin = b.o;
        bbmax = ivec(b.o).add(blockextent(b));
    }

    static templategroup *findgroup(const region &r, int &groupindex, int &instanceindex)
    {
        loopv(groups)
        {
            templategroup &group = groups[i];
            loopvj(group.regions) if(sameregion(group.regions[j], r))
            {
                groupindex = i;
                instanceindex = j;
                return &group;
            }
        }
        groupindex = -1;
        instanceindex = -1;
        return NULL;
    }

    static bool findcoveringregion(const region &r, int &groupindex, int &instanceindex)
    {
        loopv(groups)
        {
            templategroup &group = groups[i];
            loopvj(group.regions)
            {
                ivec omin, omax, imin, imax;
                regionbounds(group.regions[j], omin, omax);
                regionbounds(r, imin, imax);
                if(group.regions[j].grid == r.grid && group.regions[j].orient == r.orient &&
                   imin.x >= omin.x && imin.y >= omin.y && imin.z >= omin.z &&
                   imax.x <= omax.x && imax.y <= omax.y && imax.z <= omax.z)
                {
                    groupindex = i;
                    instanceindex = j;
                    return true;
                }
            }
        }
        groupindex = -1;
        instanceindex = -1;
        return false;
    }

    static templategroup *findmembership(const region &r, int &groupindex, int &regionindex)
    {
        templategroup *group = findgroup(r, groupindex, regionindex);
        if(group) return group;
        if(findcoveringregion(r, groupindex, regionindex)) return &groups[groupindex];
        return NULL;
    }

    static templategroup *findgroupbyid(int id)
    {
        loopv(groups) if(groups[i].id == id) return &groups[i];
        return NULL;
    }

    static void removegroup(int index)
    {
        if(index < 0 || index >= groups.length()) return;
        int removedid = groups[index].id;
        groups.remove(index);
        if(activegroup == removedid) activegroup = -1;
    }

    static void cleartemplateclipboard()
    {
        if(clipboard)
        {
            freeblock(clipboard);
            clipboard = NULL;
        }
        clipregion = region();
    }

    static void setregionbounds(region &r, const ivec &bbmin, const ivec &bbmax)
    {
        r.o = bbmin;
        ivec extent = ivec(bbmax).sub(bbmin);
        loopk(3) r.s[k] = max(extent[k] / r.grid, 1);
    }

    static bool regionoverlapsblock(const region &r, const block3 &b)
    {
        if(r.grid != b.grid || r.orient != b.orient) return false;
        ivec rmin, rmax, bmin, bmax;
        regionbounds(r, rmin, rmax);
        blockbounds(b, bmin, bmax);
        loopk(3)
        {
            if(rmax[k] <= bmin[k] || bmax[k] <= rmin[k]) return false;
        }
        return true;
    }

    static void expandregionforblock(templategroup &group, int regionindex, const block3 &sel)
    {
        region &base = group.regions[regionindex];
        ivec rmin, rmax, bmin, bmax;
        regionbounds(base, rmin, rmax);
        blockbounds(sel, bmin, bmax);
        ivec newmin(rmin), newmax(rmax);
        loopk(3)
        {
            newmin[k] = min(rmin[k], bmin[k]);
            newmax[k] = max(rmax[k], bmax[k]);
        }
        if(newmin == rmin && newmax == rmax) return;
        ivec lowerdelta = ivec(newmin).sub(rmin);
        ivec upperdelta = ivec(newmax).sub(rmax);
        loopv(group.regions)
        {
            region &r = group.regions[i];
            ivec ormin, ormax;
            regionbounds(r, ormin, ormax);
            ormin.add(lowerdelta);
            ormax.add(upperdelta);
            setregionbounds(r, ormin, ormax);
        }
        persistgroups();
    }

    static bool removeregionfromgroups(const region &reg, templategroup *skipgroup = NULL)
    {
        int groupindex = -1, instanceindex = -1;
        templategroup *group = findmembership(reg, groupindex, instanceindex);
        if(!group || group == skipgroup) return false;
        group->regions.remove(instanceindex);
        if(group->regions.empty()) removegroup(groupindex);
        persistgroups();
        return true;
    }

    static void addregion(templategroup &group, const region &reg)
    {
        if(!reg.valid()) return;
        loopvj(group.regions) if(sameregion(group.regions[j], reg)) return;
        group.regions.add(reg);
    }

    static selinfo makeselection(const region &r)
    {
        selinfo out;
        out.o = r.o;
        out.s = r.s;
        out.grid = r.grid;
        out.orient = r.orient;
        out.corner = out.cx = out.cy = 0;
        out.cxs = out.cys = 2;
        return out;
    }

    static selinfo makeblocksel(const block3 &b)
    {
        selinfo out;
        out.o = b.o;
        out.s = b.s;
        out.grid = b.grid;
        out.orient = b.orient;
        out.corner = out.cx = out.cy = 0;
        out.cxs = out.cys = 2;
        return out;
    }

    static bvec makecolor(int id)
    {
        uint seed = uint(id) * 0x9E3779B9u + 0x7F4A7C15u;
        bvec color;
        color.x = (uchar)(64 + ((seed      ) & 0xFF) * 98 / 255);
        color.y = (uchar)(64 + ((seed >>  8) & 0xFF) * 98 / 255);
        color.z = (uchar)(64 + ((seed >> 16) & 0xFF) * 98 / 255);
        return color;
    }

    static cube &templateblockcube(const block3 &b, int x, int y, int z)
    {
        int dim = dimension(b.orient), dc = dimcoord(b.orient);
        ivec pos(dim, x*b.grid, y*b.grid, dc*(b.s[dim]-1)*b.grid);
        pos.add(b.o);
        if(dc) pos[dim] -= z*b.grid;
        else pos[dim] += z*b.grid;
        return lookupcube(pos, b.grid);
    }

    static inline void copycube(const cube &src, cube &dst)
    {
        dst = src;
        dst.visible = 0;
        dst.merged = 0;
        dst.ext = NULL;
        if(src.children)
        {
            dst.children = newcubes(F_EMPTY);
            loopi(8) copycube(src.children[i], dst.children[i]);
        }
    }

    static inline void pastecube(const cube &src, cube &dst)
    {
        discardchildren(dst);
        copycube(src, dst);
    }

    static void pasteblockto(const block3 &src, const selinfo &dstsel)
    {
        block3 dst(dstsel);
        const cube *s = src.c();
        int dim = dimension(dst.orient);
        int rx = dst.s[R[dim]], ry = dst.s[C[dim]], rz = dst.s[D[dim]];
        loop(z, rz) loop(y, ry) loop(x, rx)
        {
            cube &c = templateblockcube(dst, x, y, z);
            pastecube(*s, c);
            ++s;
        }
    }

    static bool regioncontains(const region &r, const block3 &b)
    {
        ivec rmax = ivec(r.o).add(regionextent(r));
        ivec bmax = ivec(b.o).add(blockextent(b));
        return b.o.x >= r.o.x && b.o.y >= r.o.y && b.o.z >= r.o.z &&
               bmax.x <= rmax.x && bmax.y <= rmax.y && bmax.z <= rmax.z;
    }

    static bool targetcontains(const region &target, const block3 &src, const ivec &offset)
    {
        ivec to = ivec(target.o).add(offset);
        ivec tmax = ivec(to).add(blockextent(src));
        ivec regionmax = ivec(target.o).add(regionextent(target));
        return to.x >= target.o.x && to.y >= target.o.y && to.z >= target.o.z &&
               tmax.x <= regionmax.x && tmax.y <= regionmax.y && tmax.z <= regionmax.z;
    }

    static bool findregionforchange(const block3 &sel, int &groupindex, int &regionindex)
    {
        loopv(groups)
        {
            templategroup &group = groups[i];
            loopvj(group.regions) if(regioncontains(group.regions[j], sel))
            {
                groupindex = i;
                regionindex = j;
                return true;
            }
        }

        loopv(groups)
        {
            templategroup &group = groups[i];
            loopvj(group.regions) if(regionoverlapsblock(group.regions[j], sel))
            {
                expandregionforblock(group, j, sel);
                groupindex = i;
                regionindex = j;
                return true;
            }
        }
        groupindex = regionindex = -1;
        return false;
    }

    static void clearclipboard()
    {
        if(clipboard)
        {
            freeblock(clipboard);
            clipboard = NULL;
        }
        clipregion = region();
        activegroup = -1;
    }

    static void templatecopycmd()
    {
        if(noedit(true)) return;
        if(sel.s.iszero()) { conoutf(CON_WARN, "no selection to template copy"); return; }

        region current = makeregion(sel);
        int copygroupid = -1;
        {
            int groupindex = -1, instanceindex = -1;
            templategroup *group = findgroup(current, groupindex, instanceindex);
            if(group) copygroupid = group->id;
        }

        clearclipboard();
        clipboard = blockcopy(block3(sel), sel.grid);
        if(!clipboard)
        {
            conoutf(CON_ERROR, "unable to copy template selection");
            return;
        }

        clipregion = current;
        if(copygroupid >= 0) activegroup = copygroupid;
    }

    static void templatepastecmd()
    {
        if(noedit(true)) return;
        if(!clipboard)
        {
            conoutf(CON_WARN, "no template copied");
            return;
        }

        pasteblock(*clipboard, sel, true);

        templategroup *group = activegroup >= 0 ? findgroupbyid(activegroup) : NULL;
        if(!group)
        {
            templategroup &created = groups.add();
            created.id = nextgroupid++;
            created.regions.setsize(0);
            activegroup = created.id;
            group = &created;
            if(clipregion.valid()) addregion(*group, clipregion);
        }

        addregion(*group, makeregion(sel));
        persistgroups();
    }

    static void templateclearcmd(int *id, int *haveid)
    {
        if(noedit(true)) return;
        if(*haveid)
        {
            templategroup *group = findgroupbyid(*id);
            if(!group)
            {
                conoutf(CON_WARN, "no template group with id %d", *id);
                return;
            }
            loopv(groups) if(groups[i].id == *id)
            {
                removegroup(i);
                persistgroups();
                break;
            }
            return;
        }

        if(sel.s.iszero()) { conoutf(CON_WARN, "no selection to clear from template"); return; }

        region current = makeregion(sel);
        if(!removeregionfromgroups(current))
        {
            conoutf(CON_WARN, "selection is not part of a template group");
            return;
        }
    }

    static void templategetcmd()
    {
        if(noedit(true) || sel.s.iszero()) { intret(-1); return; }
        region current = makeregion(sel);
        int groupindex = -1, instanceindex = -1;
        templategroup *group = findgroup(current, groupindex, instanceindex);
        if(!group && !findcoveringregion(current, groupindex, instanceindex))
        {
            intret(-1);
            return;
        }
        intret(groups[groupindex].id);
    }

    static void templatesetcmd(int *id)
    {
        if(noedit(true)) return;
        if(sel.s.iszero()) { conoutf(CON_WARN, "no selection to set in template"); return; }
        if(!id || *id <= 0) { conoutf(CON_WARN, "invalid template id"); return; }

        templategroup *group = findgroupbyid(*id);
        if(!group)
        {
            templategroup &created = groups.add();
            created.id = *id;
            created.regions.setsize(0);
            group = &created;
            nextgroupid = max(nextgroupid, *id + 1);
        }

        region current = makeregion(sel);
        block3 blocksel(sel);

        int existingIndex = -1, existingRegion = -1;
        templategroup *existing = findmembership(current, existingIndex, existingRegion);

        bool updated = false;
        if(existing && existing == group)
        {
            expandregionforblock(*group, existingRegion, blocksel);
            updated = true;
        }
        else
        {
            loopv(group->regions)
            {
                region &r = group->regions[i];
                if(r.grid != current.grid || r.orient != current.orient) continue;
                if(regioncontains(r, blocksel) || regionoverlapsblock(r, blocksel))
                {
                    expandregionforblock(*group, i, blocksel);
                    updated = true;
                    break;
                }
            }
            if(!updated) addregion(*group, current);
        }

        if(existing && existing != group)
        {
            if(existingIndex >= 0 && existingIndex < groups.length())
            {
                templategroup &g = groups[existingIndex];
                if(existingRegion >= 0 && existingRegion < g.regions.length())
                    g.regions.remove(existingRegion);
                if(g.regions.empty()) removegroup(existingIndex);
            }
        }

        activegroup = group->id;
        persistgroups();
    }

    static void templatedebugcmd()
    {
        debugdraw = !debugdraw;
        conoutf("template debug %s", debugdraw ? "enabled" : "disabled");
    }

    static void resetstate()
    {
        cleartemplateclipboard();
        clipregion = region();
        groups.shrink(0);
        activegroup = -1;
        nextgroupid = 1;
    }

    static void persistgroups()
    {
        if(groups.empty())
        {
            setmapvar("templategroups", "");
            return;
        }

        vector<char> buf;
        bool wrote = false;
        buf.put("v1", 2);
        loopv(groups)
        {
            templategroup &group = groups[i];
            if(group.regions.empty()) continue;
            if(!wrote) wrote = true;
            buf.add(';');
            string tmp;
            formatstring(tmp, "%d:%d:", group.id, group.regions.length());
            buf.put(tmp, strlen(tmp));
            loopvj(group.regions)
            {
                if(j) buf.add('|');
                const region &r = group.regions[j];
                formatstring(tmp, "%d,%d,%d,%d,%d,%d,%d,%d", r.o.x, r.o.y, r.o.z, r.s.x, r.s.y, r.s.z, r.grid, r.orient);
                buf.put(tmp, strlen(tmp));
            }
        }

        if(!wrote)
        {
            setmapvar("templategroups", "");
            return;
        }

        buf.add('\0');
        setmapvar("templategroups", buf.getbuf());
    }

    static void loadgroupsfromstring(const char *data)
    {
        if(!data || !*data) return;

        if(strncmp(data, "v1", 2)) return;
        data += 2;
        if(*data == ';') ++data;
        if(!*data) return;

        char *copy = newstring(data);
        char *cursor = copy;
        int maxid = 0;

        while(cursor && *cursor)
        {
            char *groupstr = cursor;
            char *next = strchr(cursor, ';');
            if(next) { *next = '\0'; cursor = next + 1; }
            else cursor = NULL;
            if(!*groupstr) continue;

            char *idsep = strchr(groupstr, ':');
            if(!idsep) continue;
            *idsep = '\0';
            int id = atoi(groupstr);

            char *countstr = idsep + 1;
            char *datasep = strchr(countstr, ':');
            if(!datasep) continue;
            *datasep = '\0';
            int rcount = atoi(countstr);

            char *regions = datasep + 1;
            templategroup &group = groups.add();
            group.id = id;

            loopi(rcount)
            {
                if(!regions || !*regions) break;
                char *entry = regions;
                char *nextentry = strchr(regions, '|');
                if(nextentry) { *nextentry = '\0'; regions = nextentry + 1; }
                else regions = NULL;

                region reg;
                if(sscanf(entry, "%d,%d,%d,%d,%d,%d,%d,%d", &reg.o.x, &reg.o.y, &reg.o.z, &reg.s.x, &reg.s.y, &reg.s.z, &reg.grid, &reg.orient) == 8 && reg.valid())
                    group.regions.add(reg);
            }

            if(group.regions.empty())
            {
                groups.pop();
                continue;
            }
            maxid = max(maxid, group.id);
        }

        delete[] copy;
        if(groups.empty()) return;
        nextgroupid = max(maxid+1, 1);
        activegroup = groups.last().id;
    }

    static void drawregionoverlay(const region &region, const bvec &color)
    {
        vec mins(float(region.o.x), float(region.o.y), float(region.o.z));
        vec extent(float(region.s.x), float(region.s.y), float(region.s.z));
        extent.mul(region.grid);
        vec maxs = vec(mins).add(extent);

        vec p000(mins.x, mins.y, mins.z);
        vec p100(maxs.x, mins.y, mins.z);
        vec p110(maxs.x, maxs.y, mins.z);
        vec p010(mins.x, maxs.y, mins.z);
        vec p001(mins.x, mins.y, maxs.z);
        vec p101(maxs.x, mins.y, maxs.z);
        vec p111(maxs.x, maxs.y, maxs.z);
        vec p011(mins.x, maxs.y, maxs.z);

        auto quad = [](const vec &a, const vec &b, const vec &c, const vec &d)
        {
            gle::attrib(a);
            gle::attrib(b);
            gle::attrib(c);
            gle::attrib(d);
        };

        bvec overlaycolor;
        overlaycolor.x = (uchar)(color.x * 0.3f);
        overlaycolor.y = (uchar)(color.y * 0.3f);
        overlaycolor.z = (uchar)(color.z * 0.3f);

        gle::colorub(overlaycolor.x, overlaycolor.y, overlaycolor.z, 32);
        gle::defvertex();
        gle::begin(GL_QUADS);
        quad(p000, p100, p110, p010); // bottom
        quad(p101, p001, p011, p111); // top
        quad(p001, p000, p010, p011); // left
        quad(p100, p101, p111, p110); // right
        quad(p010, p110, p111, p011); // front
        quad(p001, p101, p100, p000); // back
        gle::end();
    }

    void renderdebug()
    {
        if(!debugdraw || groups.empty()) return;
        loopv(groups)
        {
            templategroup &group = groups[i];
            bvec color = makecolor(group.id);
            loopvj(group.regions)
            {
                const region &region = group.regions[j];
                drawregionoverlay(region, color);
                gle::colorub(color.x, color.y, color.z);
                boxs3D(vec(region.o), vec(region.s), region.grid);
            }
        }
    }

    void onchanged(const block3 &sel)
    {
        if(propagating) return;

        int groupindex = -1, regionindex = -1;
        if(!findregionforchange(sel, groupindex, regionindex)) return;

        templategroup &group = groups[groupindex];
        if(group.regions.length() <= 1) return;

        block3 *copy = blockcopy(sel, sel.grid);
        if(!copy) return;

        ivec offset = ivec(sel.o).sub(group.regions[regionindex].o);

        propagating = true;
        loopvj(group.regions)
        {
            if(j == regionindex) continue;
            const region &target = group.regions[j];
            if(!targetcontains(target, sel, offset)) continue;

            selinfo targetsel = makeblocksel(sel);
            targetsel.o = ivec(target.o).add(offset);
            pasteblockto(*copy, targetsel);

            block3 dstblock(targetsel);
            changed(dstblock, false);
        }
        propagating = false;

        freeblock(copy);
    }
} // namespace edittemplates

void edittemplates_renderdebug()
{
    edittemplates::renderdebug();
}

void edittemplates_onchanged(const block3 &sel)
{
    edittemplates::onchanged(sel);
}

void edittemplates_reset()
{
    edittemplates::resetstate();
    edittemplates::persistgroups();
}

void edittemplates_loadpersisted()
{
    const char *data = getmapvar("templategroups");
    edittemplates::resetstate();
    edittemplates::loadgroupsfromstring(data);
}

using edittemplates::templatecopycmd;
using edittemplates::templatepastecmd;
using edittemplates::templateclearcmd;
using edittemplates::templatedebugcmd;
using edittemplates::templategetcmd;
using edittemplates::templatesetcmd;

COMMANDN(templatecopy, templatecopycmd, "");
COMMANDN(templatepaste, templatepastecmd, "");
ICOMMAND(templateclear, "iN", (int *id, int *haveid), templateclearcmd(id, haveid));
COMMANDN(templatedebug, templatedebugcmd, "");
ICOMMAND(templateget, "", (), templategetcmd());
COMMANDN(templateset, templatesetcmd, "i");
