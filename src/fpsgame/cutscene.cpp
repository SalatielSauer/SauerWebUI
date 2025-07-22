#include "game.h"
#include "cutscene.h"
#include "engine.h"
#include "SDL_mixer.h"

extern int maxchannels;

extern bool detachedcamera;

extern float aspect, fovy, curfov;
extern float nearplane;
extern int farplane, screenw, screenh;
extern matrix4 projmatrix;
extern vec curfogcolor;
extern void setcamprojmatrix(bool init = true, bool flush = false);
extern void setcammatrix();
extern int unescapestring(char* dst, const char* src, const char* end);

namespace game { fpsent* spawnstate(::fpsent* d); void updatepos(fpsent* d); }

namespace cutscene
{
    using namespace game;
        struct frame
        {
            int actor;
            int time;
            vec pos;
            float yaw, pitch, roll;
            int gun;
            int attack;
        };

    struct subtitle
    {
        int frame;
        int start;
        int duration;
        int x, y;
        float size;
        string script;
    };

    struct image
    {
        int frame;
        int start;
        int duration;
        int x, y;
        float scale;
        string path;
        Texture* tex;
    };

   
    struct audio
    {
        int frame;
        int start;
        int duration;
        int from, to;
        string path;
        string cond;
        Mix_Chunk *chunk;
        Mix_Chunk* clip;
        int channel;
    };

    struct postfx
    {
        int frame;
        int start;
        int duration;
        vec4 params;
        string script;
    };

    struct csmapmodel
    {
        int frame;
        int start;
        int duration;
        string path;
        string script;
        vec pos;
        float yaw, pitch, scale;
        string anim;
        int collide;
    };

    struct runcommand
    {
        int frame;
        int start;
        int duration;
        string script;
        bool executed;
    };

    static void applyframe(fpsent* d, const frame& fr, const frame* prev)
    {
        d->o = fr.pos;
        d->yaw = fr.yaw;
        d->pitch = fr.pitch;
        d->roll = fr.roll;
        d->lastupdate = totalmillis;
        d->gunselect = fr.gun;
        d->attacking = fr.attack!=0;

        if (prev)
        {
            vec delta = vec(fr.pos).sub(prev->pos);
            int dt = fr.time - prev->time;
            if (dt <= 0) dt = 1;
            d->vel = vec(delta).mul(1000.0f / dt);
            vec f, r;
            vecfromyawpitch(fr.yaw, 0, 1, 0, f);
            vecfromyawpitch(fr.yaw + 90, 0, 1, 0, r);
            float fd = f.dot(delta), sd = r.dot(delta);
            if (fabsf(fd) >= fabsf(sd))
            {
                d->move = fd > 0.1f ? 1 : fd < -0.1f ? -1 : 0;
                d->strafe = 0;
            }
            else
            {
                d->strafe = sd > 0.1f ? 1 : sd < -0.1f ? -1 : 0;
                d->move = 0;
            }
            d->physstate = fabsf(delta.z) > 0.1f ? PHYS_FALL : PHYS_FLOOR;
        }
        else
        {
            d->vel = vec(0, 0, 0);
            d->move = d->strafe = 0;
            d->physstate = PHYS_FLOOR;
        }
        updatephysstate(d);
        game::updatepos(d);
        if(fr.attack)
        {
            vec dir;
            vecfromyawpitch(d->yaw, d->pitch, 1, 0, dir);
            vec target = vec(dir).mul(1024).add(d->o);
            game::shoot(d, target);
        }
    }

    static vector<frame> cameraframes;
    static vector< vector<frame> > actorframes;
    static vector<int> actormodels;
    static vector<frame> pauseframes;

    static vector<subtitle> subtitles;
    static int subtitleindex = 0;

    static vector<image> images;
    static int imageindex = 0;

    static vector<audio> audios;
    static int audioindex = 0;
    static int extrachannels = 0;

    static vector<postfx> postfxs;
    static int postfxindex = 0;

    static vector<csmapmodel> csmapmodels;
    static int mapmodelindex = 0;

    static vector<runcommand> runcommands;
    static int runcommandindex = 0;

    static void updatemapmodels();
    static void updateaudios(int playtime);
    static void updatepostfxs(int playtime);
    static void updateruncommands(int playtime);
    static void seekaudios(int playtime);

    static vector<fpsent*> actors;
    static vector<int> actorindex;

    static int starttime = 0, endtime = 0;
    static int camindex = 0;
    static int numactors = 0, curactor = -1;
    static bool playing = false, recording = false, paused = false, usecamera = true;
    static bool showcameramodel = false;
    static bool appendframes = false;
    static frame cameraprev;
    static bool havecamera = false;
    static frame lerpcamstart;
    static bool havelerpcamstart = false;
    static int oldplayerstate = CS_ALIVE;
    static bool changedstate = false;
    VARP(cutscenecamdebug, 0, 1, 1);
    FVARP(cutscenecamdebugsize, 0.25f, 2.0f, 4.0f);
    VARP(cutscenecamdebugpath, 0, 0, 1);
    VARP(cutscenecamdebugpathstep, 1, 1, 100);
    _SVAR(cutscenecurrentfile, cutscenecurrentfile, "", IDF_READONLY);
    _VAR(cutsceneframeslen, cutsceneframeslen, 0, 0, INT_MAX, IDF_READONLY);
    static int pausestart = 0;
    static stream* outfile = NULL;
    static string filename;

    static void updateframeslen()
    {
        int len = cameraframes.length();
        loopv(actorframes) len += actorframes[i].length();
        cutsceneframeslen = len;
    }

    static char* formatfile(const char* name)
    {
        static string fname;
        if (name && !strchr(name, '.')) formatstring(fname, "%s.ctscn", name);
        else copystring(fname, name);
        return fname;
    }

    static void writeframe(stream* f, const frame& fr)
    {
        if(fr.actor < 0)
            f->printf("%d %d %f %f %f %f %f %f\n", fr.actor, fr.time, fr.pos.x, fr.pos.y, fr.pos.z, fr.yaw, fr.pitch, fr.roll);
        else
            f->printf("%d %d %f %f %f %f %f %f %d %d\n", fr.actor, fr.time, fr.pos.x, fr.pos.y, fr.pos.z, fr.yaw, fr.pitch, fr.roll, fr.gun, fr.attack);
    }

    static inline int countbrackets(const char* s)
    {
        int n = 0;
        for (; *s; ++s) if (*s == '[') ++n; else if (*s == ']') --n;
        return n;
    }

    static void joinmultiline(char* line, char*& next)
    {
        int depth = countbrackets(line);
        while (depth > 0 && next)
        {
            char* nl = strchr(next, '\n');
            if (nl) *nl++ = '\0';
            size_t len = strlen(line);
            size_t add = strlen(next);
            line[len] = '\n';
            memmove(line + len + 1, next, add + 1);
            depth += countbrackets(line + len + 1);
            next = nl;
        }
    }

    static void savecurrent()
    {
        if (!cutscenecurrentfile[0]) return;
        stream* f = openfile(path(cutscenecurrentfile, true), "w");
        if (!f)
        {
            conoutf(CON_ERROR, "cannot write %s", cutscenecurrentfile);
            return;
        }
        loopv(subtitles) f->printf("subtitle %d [%s] %d %d %d %f\n", subtitles[i].frame, subtitles[i].script, subtitles[i].x, subtitles[i].y, subtitles[i].duration, subtitles[i].size);
        loopv(images) f->printf("image %d \"%s\" %d %d %d %f\n", images[i].frame, images[i].path, images[i].x, images[i].y, images[i].duration, images[i].scale);
        loopv(audios) f->printf("audio %d \"%s\" %d %d %d [%s]\n", audios[i].frame, audios[i].path, audios[i].from, audios[i].to, audios[i].duration, audios[i].cond);
        loopv(actormodels) f->printf("actormodel %d %d\n", i, actormodels[i]);
        loopv(csmapmodels) f->printf("mapmodel %d \"%s\" [%s] %d\n", csmapmodels[i].frame, csmapmodels[i].path, csmapmodels[i].script, csmapmodels[i].duration);
        loopv(runcommands) f->printf("runcommand %d [%s] %d\n", runcommands[i].frame, runcommands[i].script, runcommands[i].duration);
        loopv(cameraframes) writeframe(f, cameraframes[i]);
        loopv(actorframes) loopvj(actorframes[i]) writeframe(f, actorframes[i][j]);
        delete f;
    }

    template<class T>
    static inline void removeafter(vector<T>& v, int startms)
    {
        loopvrev(v) if (v[i].start >= startms) v.remove(i);
    }

    static inline void truncateactorframes(vector< vector<frame> >& acts, int startms)
    {
        loopv(acts)
        {
            int n = acts[i].length();
            while (n > 0 && acts[i][n - 1].time > startms) n--;
            acts[i].setsize(n);
        }
    }

    template<class T>
    static inline void extendtime(int& t, const vector<T>& v)
    {
        loopv(v) t = max(t, v[i].start + v[i].duration);
    }

    static inline void extendtime(int& t, vector< vector<frame> >& acts)
    {
        loopv(acts) if (acts[i].length()) t = max(t, acts[i].last().time);
    }

    static void writeexisting(stream* f, bool spec)
    {
        loopv(subtitles) f->printf("subtitle %d [%s] %d %d %d %f\n", subtitles[i].frame, subtitles[i].script, subtitles[i].x, subtitles[i].y, subtitles[i].duration, subtitles[i].size);
        loopv(images) f->printf("image %d \"%s\" %d %d %d %f\n", images[i].frame, images[i].path, images[i].x, images[i].y, images[i].duration, images[i].scale);
        loopv(audios) f->printf("audio %d \"%s\" %d %d %d [%s]\n", audios[i].frame, audios[i].path, audios[i].from, audios[i].to, audios[i].duration, audios[i].cond);
        loopv(actormodels) f->printf("actormodel %d %d\n", i, actormodels[i]);
        loopv(csmapmodels) f->printf("mapmodel %d \"%s\" [%s] %d\n", csmapmodels[i].frame, csmapmodels[i].path, csmapmodels[i].script, csmapmodels[i].duration);
        loopv(postfxs) f->printf("postfx %d [%s] %d %f %f %f %f\n", postfxs[i].frame, postfxs[i].script, postfxs[i].duration, postfxs[i].params.x, postfxs[i].params.y, postfxs[i].params.z, postfxs[i].params.w);
        loopv(runcommands) f->printf("runcommand %d [%s] %d\n", runcommands[i].frame, runcommands[i].script, runcommands[i].duration);
        if (!spec) loopv(cameraframes) writeframe(f, cameraframes[i]);
        loopv(actorframes) loopvj(actorframes[i]) writeframe(f, actorframes[i][j]);
    }

    static bool readframes(const char* fn, vector<frame>& cam, vector< vector<frame> >& actors, vector<int>& models, int& maxactor, vector<subtitle>* subs = NULL, vector<image>* imgs = NULL, vector<audio>* aus = NULL, vector<postfx>* fx = NULL, vector<csmapmodel>* mms = NULL, vector<runcommand>* rcs = NULL)
    {
        size_t len = 0;
        char* buf = loadfile(path(formatfile(fn), true), &len);
        if (!buf) return false;
        char* line = buf;
        while (line)
        {
            char* next = strchr(line, '\n');
            if (next) *next++ = '\0';
            joinmultiline(line, next);
            int id, mdl;
            if (sscanf(line, "actormodel %d %d", &id, &mdl) == 2)
            {
                while (models.length() <= id) models.add(-1);
                models[id] = mdl;
                maxactor = max(maxactor, id);
                line = next;
                if (!line) break;
                continue;
            }
            frame fr;
            int a = -1;
            int num = sscanf(line, "%d %d %f %f %f %f %f %f %d %d", &a, &fr.time, &fr.pos.x, &fr.pos.y, &fr.pos.z, &fr.yaw, &fr.pitch, &fr.roll, &fr.gun, &fr.attack);
            if (num >= 8)
            {
                fr.actor = a;
                if(num < 10) { fr.gun = 0; fr.attack = 0; }
            }
            else if (sscanf(line, "%d %f %f %f %f %f %f", &fr.time, &fr.pos.x, &fr.pos.y, &fr.pos.z, &fr.yaw, &fr.pitch, &fr.roll) == 7)
            {
                fr.actor = -1;
                fr.gun = 0;
                fr.attack = 0;
            }
            else if(!strncmp(line, "subtitle", 8))
            {
                if(subs)
                {
                    subtitle s;
                    s.start = 0;
                    s.frame = 0;
                    s.duration = 0;
                    s.x = s.y = 0;
                    s.size = 1.0f;
                    int n = sscanf(line, "subtitle %d [%255[^]]] %d %d %d %f", &s.frame, s.script, &s.x, &s.y, &s.duration, &s.size);
                    if (n >= 5)
                    {
                        if (n < 6) s.size = 1.0f;
                        int len = unescapestring(s.script, s.script, s.script + strlen(s.script));
                        s.script[len] = '\0';
                        subs->add(s);
                    }
                }
                line = next;
                if(!line) break;
                continue;
            }
            else if (!strncmp(line, "image", 5))
            {
                if (imgs)
                {
                    image im;
                    im.start = 0;
                    im.frame = 0;
                    im.duration = 0;
                    im.x = im.y = 0;
                    im.scale = 1.0f;
                    im.tex = NULL;
                    int n = sscanf(line, "image %d \"%255[^\"]\" %d %d %d %f", &im.frame, im.path, &im.x, &im.y, &im.duration, &im.scale);
                    if (n >= 5)
                    {
                        if (n < 6) im.scale = 1.0f;
                        imgs->add(im);
                    }
                }
                line = next;
                if (!line) break;
                continue;
            }
            else if (!strncmp(line, "audio", 5))
            {
                if (aus)
                {
                    audio au;
                    au.start = 0;
                    au.frame = 0;
                    au.duration = 0;
                    au.from = au.to = 0;
                    au.chunk = NULL;
                    au.channel = -1;
                    au.cond[0] = '\0';
                    au.clip = NULL;
                    int n = sscanf(line, "audio %d \"%255[^\"]\" %d %d %d [%255[^]]]", &au.frame, au.path, &au.from, &au.to, &au.duration, au.cond);
                    if(n >= 5)
                    {
                        if(n < 6) au.cond[0] = '\0';
                        aus->add(au);
                    }
                }
                line = next;
                if (!line) break;
                continue;
            }
            else if (!strncmp(line, "postfx", 6))
            {
                if (fx)
                {
                    postfx px;
                    px.start = 0;
                    px.frame = 0;
                    px.duration = 0;
                    px.params = vec4(0, 0, 0, 0);
                    int n = sscanf(line, "postfx %d [%255[^]]] %d %f %f %f %f", &px.frame, px.script, &px.duration, &px.params.x, &px.params.y, &px.params.z, &px.params.w);
                    if (n >= 3)
                    {
                        fx->add(px);
                    }
                }
                line = next;
                if (!line) break;
                continue;
            }
            else if (!strncmp(line, "mapmodel", 8))
            {
                if (mms)
                {
                    csmapmodel mm;
                    mm.start = 0;
                    mm.frame = 0;
                    mm.duration = 0;
                    mm.script[0] = '\0';
                    int n = sscanf(line, "mapmodel %d \"%255[^\"]\" [%255[^]]] %d", &mm.frame, mm.path, mm.script, &mm.duration);
                    if (n < 4)
                    {
                        mm.script[0] = '\0';
                        if (sscanf(line, "mapmodel %d \"%255[^\"]\" %d", &mm.frame, mm.path, &mm.duration) < 3)
                        {
                            line = next;
                            if (!line) break;
                            continue;
                        }
                    }
                    mms->add(mm);
                }
                line = next;
                if(!line) break;
                continue;
            }
            else if (!strncmp(line, "runcommand", 10))
            {
                if (rcs)
                {
                    runcommand rc;
                    rc.start = 0;
                    rc.frame = 0;
                    rc.duration = 0;
                    rc.executed = false;
                    int n = sscanf(line, "runcommand %d [%255[^]]] %d", &rc.frame, rc.script, &rc.duration);
                    if (n >= 2)
                    {
                        if (n < 3) rc.duration = 0;
                        int len = unescapestring(rc.script, rc.script, rc.script + strlen(rc.script));
                        rc.script[len] = '\0';
                        rcs->add(rc);
                    }
                }
                line = next;
                if(!line) break;
                continue;
            }
            else { line = next; if (!line) break; continue; }

            if (fr.actor < 0) cam.add(fr);
            else
            {
                while (actors.length() <= fr.actor) actors.add();
                actors[fr.actor].add(fr);
                maxactor = max(maxactor, fr.actor);
            }
            line = next;
            if (!line) break;
        }

        if (subs)
        {
            loopv((*subs))
            {
                subtitle &s = (*subs)[i];
                if(cam.inrange(s.frame)) s.start = cam[s.frame].time;
                else s.start = 0;
            }
        }

        if (imgs)
        {
            loopv((*imgs))
            {
                image& im = (*imgs)[i];
                if (cam.inrange(im.frame)) im.start = cam[im.frame].time;
                else im.start = 0;
            }
        }

       
        if (aus)
        {
            loopv((*aus))
            {
                audio &au = (*aus)[i];
                if (cam.inrange(au.frame)) au.start = cam[au.frame].time;
                else au.start = 0;
            }
        }

        if (fx)
        {
            loopv((*fx))
            {
                postfx& px = (*fx)[i];
                if (cam.inrange(px.frame)) px.start = cam[px.frame].time;
                else px.start = 0;
            }
        }

        if (mms)
        {
            loopv((*mms))
            {
                csmapmodel &mm = (*mms)[i];
                if (cam.inrange(mm.frame)) mm.start = cam[mm.frame].time;
                else mm.start = 0;
            }
        }

        if (rcs)
        {
            loopv((*rcs))
            {
                runcommand &rc = (*rcs)[i];
                if (cam.inrange(rc.frame)) rc.start = cam[rc.frame].time;
                else rc.start = 0;
            }
        }

        delete[] buf;
        return true;
    }

    bool isactive() { return playing || recording; }

    bool isrecording() { return recording; }

    bool isrecordingactor(physent* d)
    {
        return recording && curactor >= 0 && curactor < actors.length() && actors[curactor] == d;
    }

    int currenttime()
    {
        if (!isactive()) return 0;
        return paused ? pausestart - starttime : lastmillis - starttime;
    }

    int currentframe()
    {
        if (!isactive()) return 0;
        int curms = currenttime();

        int best = -1;

        if (!cameraframes.empty())
        {
            int idx = 0;
            while (idx + 1 < cameraframes.length() && cameraframes[idx + 1].time <= curms) idx++;
            best = idx;
        }

        loopi(numactors) if (actorframes[i].length())
        {
            int aidx = 0;
            while (aidx + 1 < actorframes[i].length() && actorframes[i][aidx + 1].time <= curms) aidx++;
            best = max(best, aidx);
        }

        return max(best, 0);
    }

    static void playinternal(const char* file, int startms, int endms, bool camera)
    {
        if (!m_edit)
        {
            conoutf(CON_ERROR, "cutscene playback only allowed in edit mode");
            return;
        }

        stop();
        vector<frame> cam;
        vector< vector<frame> > acts;
        vector<int> models;
        vector<subtitle> subs;
        vector<image> imgs;
        vector<audio> aus;
        vector<postfx> fx;
        vector<csmapmodel> mms;
        vector<runcommand> rcs;

        int maxactor = -1;

        if (!readframes(file, cam, acts, models, maxactor, &subs, &imgs, &aus, &fx, &mms, &rcs))
        {
            conoutf(CON_ERROR, "could not load cutscene %s", file);
            return;
        }

        cameraframes.move(cam);
        actorframes.move(acts);
        actormodels.move(models);
        subtitles.move(subs);
        images.move(imgs);

       
        audios.move(aus);
        extrachannels = audios.length();
        if (extrachannels > 0)
        {
            Mix_AllocateChannels(maxchannels + extrachannels);
            Mix_ReserveChannels(maxchannels);
        }

        postfxs.move(fx);
        csmapmodels.move(mms);
        runcommands.move(rcs);
        mapmodelindex = 0;
        subtitleindex = 0;
        imageindex = 0;
        audioindex = 0;
        postfxindex = 0;
        runcommandindex = 0;
        numactors = maxactor + 1;
        while (actorframes.length() < numactors) actorframes.add();
        actors.shrink(0);
        actorindex.shrink(0);
        loopi(numactors)
        {
            fpsent* d = game::spawnstate(new fpsent);
            d->clientnum = -1;
            d->state = CS_ALIVE;
            d->lastupdate = totalmillis;
            if (i < actormodels.length()) d->playermodel = actormodels[i];
            game::players.add(d);
            actors.add(d);
            actorindex.add(0);
        }
        updateframeslen();
        copystring(filename, formatfile(file));
        DELETEA(cutscenecurrentfile);
        cutscenecurrentfile = newstring(filename);
        startms = max(startms, 0);
        int lasttime = cameraframes.empty() ? 0 : cameraframes.last().time;
        extendtime(lasttime, actorframes);
        extendtime(lasttime, subtitles);
        extendtime(lasttime, images);
        extendtime(lasttime, audios);
        extendtime(lasttime, postfxs);
        extendtime(lasttime, csmapmodels);
        extendtime(lasttime, runcommands);

        endtime = endms > 0 ? min(endms, lasttime) : lasttime;
        starttime = lastmillis;
        camindex = 0;
        subtitleindex = imageindex = audioindex = postfxindex = 0;
        runcommandindex = 0;
        playing = true;
        recording = false;
        paused = false;
        usecamera = camera;
        detachedcamera = camera;

        if(camera)
        {
            oldplayerstate = game::player1->state;
            if(game::player1->state != CS_SPECTATOR)
            {
                game::player1->state = CS_SPECTATOR;
                changedstate = true;
            }
        }
        settime(startms);
        conoutf(CON_DEBUG, "play cutscene %s from %d to %d", file, startms, endtime);
    }

    void play(const char* file, int startms, int endms)
    {
        playinternal(file, startms, endms, true);
    }

    void playbackstart(const char* file, int startms, int endms)
    {
        playinternal(file, startms, endms, false);
    }

    void recordstart(const char* file)
    {
        stop();
        cameraframes.shrink(0);
        actorframes.shrink(0);
        actormodels.shrink(0);
        numactors = 0;
        curactor = (game::player1->state == CS_SPECTATOR ? -1 : numactors++);
        if (curactor >= 0) actormodels.add(game::player1->playermodel);
        outfile = openfile(path(formatfile(file), true), "w");
        if (!outfile)
        {
            conoutf(CON_ERROR, "cannot open %s for recording", file);
            return;
        }
        loopv(actormodels) outfile->printf("actormodel %d %d\n", i, actormodels[i]);
        copystring(filename, formatfile(file));
        DELETEA(cutscenecurrentfile);
        cutscenecurrentfile = newstring(filename);
        updateframeslen();
        starttime = lastmillis;
        camindex = 0;
        playing = false;
        recording = true;
        paused = false;
        appendframes = false;
        detachedcamera = true;
        conoutf(CON_DEBUG, "recording cutscene to %s", file);
    }

    void recordover(const char* file)
    {
        stop();
        vector<frame> cam;
        vector< vector<frame> > acts;
        vector<int> models;
        vector<subtitle> subs;
        vector<image> imgs;
        vector<audio> aus;
        vector<postfx> fx;
        vector<csmapmodel> mms;

        int maxactor = -1;

        if (!readframes(file, cam, acts, models, maxactor, &subs, &imgs, &aus, &fx, &mms))
        {
            conoutf(CON_ERROR, "could not load cutscene %s", file);
            return;
        }

        bool spec = (game::player1->state == CS_SPECTATOR);

        if (spec)
        {
            cameraframes.shrink(0);
            showcameramodel = false;
        }
        else
        {
            cameraframes.move(cam);
        }

        actorframes.move(acts);
        actormodels.move(models);
        subtitles.move(subs);
        images.move(imgs);
        audios.move(aus);
        postfxs.move(fx);
        csmapmodels.move(mms);

        mapmodelindex = 0;
        subtitleindex = 0;
        imageindex = 0;
        audioindex = 0;
        postfxindex = 0;

        extrachannels = audios.length();
        if (extrachannels > 0)
        {
            Mix_AllocateChannels(maxchannels + extrachannels);
            Mix_ReserveChannels(maxchannels);
        }

        actors.shrink(0);
        actorindex.shrink(0);

        numactors = maxactor + 1;
        loopi(numactors)
        {
            fpsent* d = game::spawnstate(new fpsent);
            d->clientnum = -1;
            d->state = CS_ALIVE;
            d->lastupdate = totalmillis;
            if (i < actormodels.length()) d->playermodel = actormodels[i];
            game::players.add(d);
            actors.add(d);
            actorindex.add(0);
        }

        copystring(filename, formatfile(file));
        DELETEA(cutscenecurrentfile);
        cutscenecurrentfile = newstring(filename);

        int lasttime = cameraframes.empty() ? 0 : cameraframes.last().time;
       
        extendtime(lasttime, actorframes);
        extendtime(lasttime, subtitles);
        extendtime(lasttime, images);
        extendtime(lasttime, audios);
        extendtime(lasttime, postfxs);
        extendtime(lasttime, csmapmodels);
        extendtime(lasttime, runcommands);

        endtime = lasttime;
        starttime = lastmillis;
        camindex = 0;
        loopi(numactors) actorindex[i] = 0;

        curactor = spec ? -1 : numactors++;
        if (curactor >= 0)
        {
            while (actorframes.length() <= curactor) actorframes.add();
            actorindex.add(0);
            fpsent* d = game::spawnstate(new fpsent);
            d->clientnum = -1;
            d->state = CS_ALIVE;
            d->lastupdate = totalmillis;
            d->playermodel = game::player1->playermodel;
            //game::players.add(d);
            actors.add(d);
            actormodels.add(game::player1->playermodel);
        }

        updateframeslen();

        outfile = openfile(path(formatfile(file), true), "w");
        if (!outfile)
        {
            conoutf(CON_ERROR, "cannot open %s for recording", file);
            stop();
            return;
        }

        writeexisting(outfile, spec);

        playing = true;
        recording = true;
        paused = false;
        appendframes = false;
        usecamera = false;
        detachedcamera = false;
        conoutf(CON_DEBUG, "recording over cutscene %s", file);
    }

    void recordcontinue()
    {
        if (!cutscenecurrentfile[0])
        {
            conoutf(CON_ERROR, "no cutscene loaded");
            return;
        }

        if (outfile) { delete outfile; outfile = NULL; }

        bool spec = (game::player1->state == CS_SPECTATOR);
        int curms = paused ? pausestart - starttime : lastmillis - starttime;

        if (spec)
        {
            loopvrev(cameraframes) if (cameraframes[i].time >= curms) cameraframes.remove(i);
        }

        int lasttime = cameraframes.empty() ? 0 : cameraframes.last().time;
        extendtime(lasttime, actorframes);
        extendtime(lasttime, subtitles);
        extendtime(lasttime, images);
        extendtime(lasttime, audios);
        extendtime(lasttime, postfxs);
        extendtime(lasttime, csmapmodels);
        extendtime(lasttime, runcommands);
        endtime = lasttime;

        updateframeslen();

        outfile = openfile(path(filename, true), "w");
        if (!outfile)
        {
            conoutf(CON_ERROR, "cannot open %s for recording", filename);
            return;
        }

        writeexisting(outfile, false);

        settime(curms);

        playing = true;
        recording = true;
        paused = false;
        appendframes = true;
        usecamera = !spec;
        detachedcamera = false;

        conoutf(CON_DEBUG, "continuing cutscene recording %s", filename);
    }

    static void updatemapmodels()
    {
        if (csmapmodels.empty() || !playing) return;
        int curms = paused ? pausestart - starttime : lastmillis - starttime;
        while (mapmodelindex < csmapmodels.length() && curms >= csmapmodels[mapmodelindex].start + csmapmodels[mapmodelindex].duration) mapmodelindex++;
        for (int i = mapmodelindex; i < csmapmodels.length() && curms >= csmapmodels[i].start; ++i)
        {
            if (curms > csmapmodels[i].start + csmapmodels[i].duration) continue;
            csmapmodel& mm = csmapmodels[i];
            tagval args[1];
            args[0].setint(curms - mm.start);
            char* ret = mm.script[0] ? executestr(mm.script, args, 1) : NULL;
            const char* vals = ret && *ret ? ret : "512 512 512 0 0 20 mapmodel 1";
            char anim[64] = "mapmodel";
            sscanf(vals, "%f %f %f %f %f %f %63s %d", &mm.pos.x, &mm.pos.y, &mm.pos.z, &mm.yaw, &mm.pitch, &mm.scale, anim, &mm.collide);
            copystring(mm.anim, anim);
            delete[] ret;
        }
    }

    void update(int curtime)
    {
        int shaderstate = 0;
        if (paused) shaderstate = 2;
        else if (recording) shaderstate = 1;
        else if (playing) shaderstate = 3;
        GLOBALPARAMI(cutscenestate, shaderstate);
        if (recording && !paused)
        {
            showcameramodel = false;
            int now = lastmillis - starttime;
            frame fr;
            fr.actor = -1;
            fr.time = now;
            fr.pos = camera1->o;
            fr.yaw = camera1->yaw;
            fr.pitch = camera1->pitch;
            fr.roll = camera1->roll;
            fr.gun = 0;
            fr.attack = 0;
            cameraframes.add(fr);
            if (outfile) writeframe(outfile, fr);

            if (curactor >= 0)
            {
                fr.actor = curactor;
                fr.pos = game::player1->o;
                fr.yaw = game::player1->yaw;
                fr.pitch = game::player1->pitch;
                fr.roll = game::player1->roll;
                fr.gun = game::player1->gunselect;
                fr.attack = game::player1->attacking ? 1 : 0;
                while (actorframes.length() <= curactor) actorframes.add();
                actorframes[curactor].add(fr);
                if (outfile) writeframe(outfile, fr);
            }
            updateframeslen();
            if (appendframes && now > endtime) endtime = now;
        }
        //else showcameramodel = false;
        if (playing && !paused)
        {
            int playtime = lastmillis - starttime;
            if (playtime > endtime) { stop(); return; }
            while (camindex + 1 < cameraframes.length() && cameraframes[camindex + 1].time <= playtime) camindex++;
            if (camindex < cameraframes.length())
            {
                frame& fr = cameraframes[camindex];
                cameraprev = fr;
                havecamera = true;
                if (usecamera)
                {
                    camera1->o = fr.pos;
                    camera1->yaw = fr.yaw;
                    camera1->pitch = fr.pitch;
                    camera1->roll = fr.roll;
                }
            }
            loopi(numactors)
            {
                while (actorindex[i] + 1 < actorframes[i].length() && actorframes[i][actorindex[i] + 1].time <= playtime) actorindex[i]++;
                if (actorindex[i] < actorframes[i].length())
                {
                    frame& fr = actorframes[i][actorindex[i]];
                    frame* prev = actorindex[i] > 0 ? &actorframes[i][actorindex[i] - 1] : NULL;
                    fpsent* d = actors[i];
                    //applyframe(d, fr, prev);
                    if (!(recording && i == curactor))
                        applyframe(d, fr, prev);
                }
            }

            updateaudios(playtime);
            updatepostfxs(playtime);
            updateruncommands(playtime);

            updatemapmodels();

            //showcameramodel = !usecamera && !recording;
            showcameramodel = !usecamera && !(recording && curactor < 0);
        }
        else if (paused)
        {
            loopi(numactors)
            {
                fpsent* d = actors[i];
                if (i < pauseframes.length()) applyframe(d, pauseframes[i], NULL);
                else d->lastupdate = totalmillis;
            }
        }
        else showcameramodel = false;
    }

    void pause()
    {
        if (!isactive()) return;
        if (!paused)
        {
            pausestart = lastmillis;
            pauseframes.shrink(0);
            loopi(numactors)
            {
                frame fr;
                fr.actor = i;
                fr.time = 0;
                fr.pos = actors[i]->o;
                fr.yaw = actors[i]->yaw;
                fr.pitch = actors[i]->pitch;
                fr.roll = actors[i]->roll;
                fr.gun = actors[i]->gunselect;
                fr.attack = actors[i]->attacking ? 1 : 0;
                pauseframes.add(fr);
            }

            loopv(audios)
            {
                if (audios[i].channel >= 0 && Mix_Playing(audios[i].channel))
                    Mix_Pause(audios[i].channel);
            }
            paused = true;
            conoutf(CON_DEBUG, "cutscene paused");
        }
        else
        {
            starttime += lastmillis - pausestart;
            paused = false;
            pauseframes.shrink(0);
           
            loopv(audios)
            {
                if (audios[i].channel >= 0 && Mix_Paused(audios[i].channel))
                    Mix_Resume(audios[i].channel);
            }
            conoutf(CON_DEBUG, "cutscene resumed");
        }
    }

    void stop()
    {
        if (outfile) { delete outfile; outfile = NULL; }
        if (!isactive() && actors.empty()) return;
        playing = recording = false;
        paused = false;
        appendframes = false;
        pauseframes.shrink(0);
        cameraframes.shrink(0);
        actorframes.shrink(0);
        actormodels.shrink(0);
        subtitles.shrink(0);
        images.shrink(0);

        loopv(audios)
        {
            if (audios[i].channel >= 0) Mix_HaltChannel(audios[i].channel);
            if (audios[i].clip) { Mix_FreeChunk(audios[i].clip); audios[i].clip = NULL; }
            if (audios[i].chunk && audios[i].chunk->allocated)
                Mix_FreeChunk(audios[i].chunk);
        }
        audios.shrink(0);
        if (extrachannels > 0)
        {
            Mix_ReserveChannels(0);
            Mix_AllocateChannels(maxchannels);
            extrachannels = 0;
        }

        postfxs.shrink(0);
        csmapmodels.shrink(0);
        runcommands.shrink(0);

        subtitleindex = 0;
        imageindex = 0;
        audioindex = 0;
        runcommandindex = 0;
        camindex = 0;
        numactors = 0;
        curactor = -1;
        loopv(actors)
        {
            game::removeweapons(actors[i]);
            game::players.removeobj(actors[i]);
            delete actors[i];
        }
        actors.shrink(0);
        actorindex.shrink(0);
        detachedcamera = false;
        if(changedstate)
        {
            game::player1->state = oldplayerstate;
            changedstate = false;
        }

        execute("clearpostfx");

        conoutf(CON_DEBUG, "cutscene stopped");
        filename[0] = '\0';
        DELETEA(cutscenecurrentfile);
        cutscenecurrentfile = newstring("");
        updateframeslen();
    }

    void load(const char* file)
    {
        vector<frame> cam;
        vector< vector<frame> > acts;
        vector<int> models;
        vector<subtitle> subs;
        vector<image> imgs;
        vector<audio> aus; 
        vector<postfx> fx;
        vector<csmapmodel> mms;
        vector<runcommand> rcs;

        int maxa = -1;
        int offset = 0;

        if (!readframes(formatfile(file), cam, acts, models, maxa, &subs, &imgs, &aus, &fx, &mms, &rcs)) return;
        if (!cameraframes.empty()) offset = max(offset, cameraframes.last().time);

        extendtime(offset, actorframes);
        extendtime(offset, subtitles);
        extendtime(offset, images);
        extendtime(offset, audios);
        extendtime(offset, postfxs);
        extendtime(offset, csmapmodels);
        extendtime(offset, runcommands);
        loopv(cam)
        {
            frame fr = cam[i];
            fr.time += offset;
            cameraframes.add(fr);
            if (outfile) writeframe(outfile, fr);
        }

        loopvj(acts)
        {
            int id = j;
            while (id >= numactors)
            {
                actorframes.add();
                actorindex.add(0);
                if (models.inrange(id)) actormodels.add(models[id]);
                else actormodels.add(game::player1->playermodel);
                numactors++;
            }
            loopvk(acts[j])
            {
                frame fr = acts[j][k];
                fr.time += offset;
                fr.actor = id;
                actorframes[id].add(fr);
                if (outfile) writeframe(outfile, fr);
            }
        }

        loopv(subs)
        {
            subtitle s = subs[i];
            s.start += offset;
            subtitles.add(s);
        }

        loopv(imgs)
        {
            image im = imgs[i];
            im.start += offset;
            images.add(im);
        }

        loopv(aus)
        {
            audio au = aus[i];
            au.start += offset;
            au.channel = -1;
            au.chunk = NULL;
            au.clip = NULL;
            audios.add(au);
        }

       loopv(fx)
       {
           postfx px = fx[i];
           px.start += offset;
           postfxs.add(px);
       }

       loopv(mms)
       {
           csmapmodel mm = mms[i];
           mm.start += offset;
           csmapmodels.add(mm);
       }

        loopv(rcs)
        {
            runcommand rc = rcs[i];
            rc.start += offset;
            rc.executed = false;
            runcommands.add(rc);
        }

        loopi(models.length()) if (i < actormodels.length()) actormodels[i] = models[i];
        updateframeslen();
        conoutf(CON_DEBUG, "loaded cutscene %s", file);
    }

    void restart()
    {
        if (!isactive()) return;
        starttime = lastmillis;
        camindex = 0;
        loopi(numactors) actorindex[i] = 0;
        subtitleindex = 0;
        imageindex = 0;
        audioindex = 0;
        postfxindex = 0;
        mapmodelindex = 0;
        runcommandindex = 0;
        loopv(runcommands) runcommands[i].executed = false;
        conoutf(CON_DEBUG, "cutscene restarted");
    }

    void settime(int millis)
    {
        if (!isactive()) return;
        // reset any running audio and post effects when seeking
        loopv(audios)
        {
            if (audios[i].channel >= 0)
            {
                Mix_HaltChannel(audios[i].channel);
                if (audios[i].clip) { Mix_FreeChunk(audios[i].clip); audios[i].clip = NULL; }
                audios[i].channel = -1;
            }
        }
        execute("clearpostfx");
        starttime = lastmillis - millis;
        camindex = 0;
        loopi(numactors) actorindex[i] = 0;
        while (camindex < cameraframes.length() && cameraframes[camindex].time < millis) camindex++;
        loopi(numactors) while (actorindex[i] < actorframes[i].length() && actorframes[i][actorindex[i]].time < millis) actorindex[i]++;
       
        subtitleindex = 0;
        imageindex = 0;
        postfxindex = 0;
        mapmodelindex = 0;
        audioindex = 0;
        runcommandindex = 0;
        loopv(runcommands) runcommands[i].executed = false;

        while (audioindex < audios.length() && audios[audioindex].start + audios[audioindex].duration <= millis) audioindex++;
        while (subtitleindex < subtitles.length() && subtitles[subtitleindex].start + subtitles[subtitleindex].duration <= millis) subtitleindex++;
        while (imageindex < images.length() && images[imageindex].start + images[imageindex].duration <= millis) imageindex++;
        while (postfxindex < postfxs.length() && postfxs[postfxindex].start + postfxs[postfxindex].duration <= millis) postfxindex++;
        while (mapmodelindex < csmapmodels.length() && csmapmodels[mapmodelindex].start + csmapmodels[mapmodelindex].duration <= millis) mapmodelindex++;
        while (runcommandindex < runcommands.length() && runcommands[runcommandindex].start + runcommands[runcommandindex].duration <= millis) runcommandindex++;

        int playtime = millis;

        // start any audio that should be active at this time
        seekaudios(playtime);

        // apply any post effects for the current time
        updatepostfxs(playtime);
        updateruncommands(playtime);
        conoutf(CON_DEBUG, "cutscene time set to %d", millis);
    }

    void setframe(int index)
    {
        if (!isactive()) return;
        if (index < 0) index = 0;
        int millis = 0;
        if (!cameraframes.empty())
        {
            index = min(index, cameraframes.length() - 1);
            millis = cameraframes[index].time;
        }
        else
        {
            loopi(numactors) if (actorframes[i].length())
            {
                int idx = clamp(index, 0, actorframes[i].length() - 1);
                millis = max(millis, actorframes[i][idx].time);
            }
        }
        settime(millis);
        conoutf(CON_DEBUG, "cutscene frame set to %d", index);
    }

    void clearcam(int dir)
    {
        if (cameraframes.empty()) return;

        int curms = paused ? pausestart - starttime : lastmillis - starttime;

        if (!dir)
        {
            cameraframes.shrink(0);
        }
        else
        {
            loopvrev(cameraframes)
            {
                if ((dir < 0 && cameraframes[i].time <= curms) ||
                    (dir > 0 && cameraframes[i].time >= curms))
                    cameraframes.remove(i);
            }
        }

        camindex = 0;
        while (camindex < cameraframes.length() && cameraframes[camindex].time < curms) camindex++;
        updateframeslen();

        conoutf(CON_DEBUG, "camera frames cleared");
    }

    void clearactor(int id)
    {
        if (!actorframes.inrange(id) || !actors.inrange(id)) return;
        actorframes[id].shrink(0);
        if (actorindex.inrange(id)) actorindex[id] = 0;
        int idx = game::players.find(actors[id]);
        if (idx >= 0) game::players.remove(idx);
        game::removeweapons(actors[id]);
        updateframeslen();
        savecurrent();
        conoutf(CON_DEBUG, "actor %d frames cleared", id);
    }

    void rendercamerapath()
    {
        if(playing && usecamera) return;
        if (!cutscenecamdebugpath || cameraframes.length() < 2) return;
        int step = max(cutscenecamdebugpathstep, 1);
        int cur = playing || recording ? lastmillis - starttime : INT_MIN;
        for (int i = 0; i < cameraframes.length() - 1; i += step)
        {
            const frame& a = cameraframes[i];
            const frame& b = cameraframes[min(i + step, cameraframes.length() - 1)];
            int color = a.time >= cur ? 0xFFFF00 : 0x222222;
            particle_flare(a.pos, b.pos, 1, PART_STREAK, color);
        }
    }

    void rendercamera()
    {
        if (!showcameramodel || !havecamera) return;
        rendermodel(NULL, "camera", ANIM_MAPMODEL | ANIM_LOOP, cameraprev.pos, cameraprev.yaw+90, cameraprev.pitch,
            MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED);
    }

    static physent* oldcam = NULL;
    static physent debugcam;
    static int olddrawtex = 0;

    static float oldaspect = 0, oldfovy = 0, oldfov = 0;
    static int oldfarplane = 0;
    static matrix4 oldproj;

    static void startdebug(int x, int y, int w, int h)
    {
        glViewport(x, y, w, h);
        glScissor(x, y, w, h);
        glEnable(GL_SCISSOR_TEST);

        olddrawtex = ::drawtex;
        ::drawtex = DRAWTEX_MODELPREVIEW;

        oldcam = camera1;
        debugcam = *camera1;
        debugcam.type = ENT_CAMERA;
        camera1 = &debugcam;

        oldaspect = aspect;
        oldfovy = fovy;
        oldfov = curfov;
        oldfarplane = farplane;
        oldproj = projmatrix;

        aspect = w / float(h);
        curfov = 2 * atan2(tan(fovy / 2 * RAD), 1 / aspect) / RAD;

        clearfogdist();
        zerofogcolor();
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        projmatrix.perspective(fovy, aspect, nearplane+6, farplane);
        setcamprojmatrix();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
    }

    static void enddebug()
    {
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);

        resetfogdist();
        resetfogcolor();
        glClearColor(curfogcolor.r, curfogcolor.g, curfogcolor.b, 1);

        aspect = oldaspect;
        fovy = oldfovy;
        curfov = oldfov;
        farplane = oldfarplane;

        camera1 = oldcam;

        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, screenw, screenh);

        ::drawtex = olddrawtex;

        projmatrix = oldproj;
        setcamprojmatrix();
    }

    void rendercamerafeed()
    {
        if(playing && usecamera) return;
        if (!cutscenecamdebug || !havecamera || !isactive()) return;
        int size = int(min(screenw, screenh) / 4 * cutscenecamdebugsize);
        size = clamp(size, 1, min(screenw, screenh));
        int x = screenw - size - FONTH;
        int y = screenh - size - FONTH;

        bool oldmodel = showcameramodel;
        showcameramodel = false;

        pushhudmatrix();

        startdebug(x, y, size, size);
        camera1->o = cameraprev.pos;
        camera1->yaw = cameraprev.yaw;
        camera1->pitch = cameraprev.pitch;
        camera1->roll = cameraprev.roll;

        setcammatrix();
        setcamprojmatrix(true, true);

        visiblecubes();

        glViewport(x, y, size, size);
        glScissor(x, y, size, size);
        glEnable(GL_SCISSOR_TEST);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (limitsky()) drawskybox(farplane, true);
        rendergeom();

        rendermapmodels();
        ::rendergame(true);
        if (!limitsky()) drawskybox(farplane, false);

        enddebug();

        pophudmatrix(true);
        hudshader->set();

        showcameramodel = oldmodel;
    }

    void rendersubtitles()
    {
        if (subtitles.empty() || !playing) return;
        int curms = paused ? pausestart - starttime : lastmillis - starttime;
        while (subtitleindex < subtitles.length() && curms >= subtitles[subtitleindex].start + subtitles[subtitleindex].duration) subtitleindex++;
        for (int i = subtitleindex; i < subtitles.length() && curms >= subtitles[i].start; ++i)
        {
            if (curms > subtitles[i].start + subtitles[i].duration) continue;
            tagval __args[1];
            __args[0].setint(curms - subtitles[i].start);
            char* text = executestr(subtitles[i].script, __args, 1);
            if (text && *text)
            {
                pushhudmatrix();
                float scale = (screenh / 1800.0f) * subtitles[i].size;
                hudmatrix.scale(scale, scale, 1);
                flushhudmatrix();
                glEnable(GL_BLEND);
                draw_text(text, subtitles[i].x, subtitles[i].y);
                pophudmatrix(true);
                hudshader->set();
            }
            delete[] text;
        }
    }

    void renderimages()
    {
        if (images.empty() || !playing) return;
        int curms = paused ? pausestart - starttime : lastmillis - starttime;
        while (imageindex < images.length() && curms >= images[imageindex].start + images[imageindex].duration) imageindex++;
        for (int i = imageindex; i < images.length() && curms >= images[i].start; ++i)
        {
            if (curms > images[i].start + images[i].duration) continue;
            if (!images[i].tex) images[i].tex = textureload(images[i].path, 3, true);
            Texture* tex = images[i].tex;
            if (!tex) continue;
            glBindTexture(GL_TEXTURE_2D, tex->id);
            hudshader->set();
            pushhudmatrix();
            float scale = (screenh / 1800.0f) * images[i].scale;
            hudmatrix.scale(scale, scale, 1);
            flushhudmatrix();
            hudquad(images[i].x, images[i].y, tex->w, tex->h);
            pophudmatrix();
            hudshader->set();
        }
    }

    void rendermapmodels()
    {
        if(csmapmodels.empty() || !playing) return;
        int curms = paused ? pausestart - starttime : lastmillis - starttime;
        for(int i = mapmodelindex; i < csmapmodels.length() && curms >= csmapmodels[i].start; ++i)
        {
            if(curms > csmapmodels[i].start + csmapmodels[i].duration) continue;
            csmapmodel &mm = csmapmodels[i];
            vector<int> anims; findanims(mm.anim, anims);
            int anim = anims.empty() ? ANIM_MAPMODEL|ANIM_LOOP : (anims[0]|ANIM_LOOP);
            model* mdl = loadmodel(mm.path);
            if (mdl)
            {
                mdl->scale = mm.scale;
                mdl->collide = mm.collide != 0;
            }
            rendermodel(NULL, mm.path, anim, mm.pos, mm.yaw, mm.pitch, MDL_LIGHT | MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED);
        }
    }

    void lerpcamfrom()
    {
        lerpcamstart.actor = -1;
        lerpcamstart.time = cameraframes.empty() ? 0 : cameraframes.last().time;
        lerpcamstart.pos = camera1->o;
        lerpcamstart.yaw = camera1->yaw;
        lerpcamstart.pitch = camera1->pitch;
        lerpcamstart.roll = camera1->roll;
        lerpcamstart.gun = 0;
        lerpcamstart.attack = 0;
        havelerpcamstart = true;
    }

    void lerpcamto(int millis)
    {
        if (!havelerpcamstart) return;
        int start = lerpcamstart.time;
        frame target;
        target.actor = -1;
        target.time = start + max(millis, 0);
        target.pos = camera1->o;
        target.yaw = camera1->yaw;
        target.pitch = camera1->pitch;
        target.roll = camera1->roll;
        target.gun = 0;
        target.attack = 0;

        const int step = 1;
        cameraframes.add(lerpcamstart);
        if (outfile) writeframe(outfile, lerpcamstart);
        for (int t = step; t < millis; t += step)
        {
            float k = float(t) / float(millis);
            frame fr;
            fr.actor = -1;
            fr.time = start + t;
            fr.pos = vec(lerpcamstart.pos).lerp(target.pos, k);
            fr.yaw = lerpcamstart.yaw + (target.yaw - lerpcamstart.yaw) * k;
            fr.pitch = lerpcamstart.pitch + (target.pitch - lerpcamstart.pitch) * k;
            fr.roll = lerpcamstart.roll + (target.roll - lerpcamstart.roll) * k;
            fr.gun = 0;
            fr.attack = 0;
            cameraframes.add(fr);
            if (outfile) writeframe(outfile, fr);
        }
        cameraframes.add(target);
        if (outfile) writeframe(outfile, target);
        havelerpcamstart = false;
        updateframeslen();
    }

    int actorid(physent* d)
    {
        return actors.find((fpsent*)d);
    }

    bool playingcamera()
    {
        return playing && usecamera;
    }

    static void updateaudios(int playtime)
    {
        while (audioindex < audios.length() && playtime >= audios[audioindex].start + audios[audioindex].duration) audioindex++;
        for (int i = audioindex; i < audios.length() && playtime >= audios[i].start; ++i)
        {
            if (playtime > audios[i].start + audios[i].duration) continue;
            if (audios[i].cond[0])
            {
                tagval __args[1];
                __args[0].setint(playtime - audios[i].start);
                char *ret = executestr(audios[i].cond, __args, 1);
                bool ok = ret && atoi(ret) != 0;
                delete[] ret;
                if (!ok) continue;
            }
            if (audios[i].channel >= 0 && Mix_Playing(audios[i].channel)) continue;
            if (audios[i].channel >= 0 && !Mix_Playing(audios[i].channel))
            {
                if (audios[i].clip) { Mix_FreeChunk(audios[i].clip); audios[i].clip = NULL; }
                audios[i].channel = -1;
            }
            if (!audios[i].chunk) audios[i].chunk = Mix_LoadWAV(findfile(audios[i].path, "rb"));
            if (!audios[i].chunk) continue;
            int freq = 0, chans = 0; Uint16 fmt = 0; Mix_QuerySpec(&freq, &fmt, &chans);
            int bps = freq * ((fmt & 0xFF) / 8) * chans;
            Uint32 startb = Uint32((double)audios[i].from * bps / 1000.0);
            Uint32 endb = audios[i].to > audios[i].from ? Uint32((double)audios[i].to * bps / 1000.0) : audios[i].chunk->alen;
            if (endb > audios[i].chunk->alen) endb = audios[i].chunk->alen;
            if (startb >= endb) continue;
            if (audios[i].clip) { Mix_FreeChunk(audios[i].clip); audios[i].clip = NULL; }
            audios[i].clip = Mix_QuickLoad_RAW(audios[i].chunk->abuf + startb, endb - startb);
            if (!audios[i].clip) continue;
            audios[i].clip->allocated = 0;
            audios[i].channel = Mix_PlayChannelTimed(-1, audios[i].clip, 0, (audios[i].to > audios[i].from) ? audios[i].to - audios[i].from : -1);
        }
    }

    static void seekaudios(int playtime)
    {
        loopi(audios.length())
        {
            if (playtime < audios[i].start || playtime >= audios[i].start + audios[i].duration) continue;
            if (audios[i].cond[0])
            {
                tagval __args[1];
                __args[0].setint(playtime - audios[i].start);
                char *ret = executestr(audios[i].cond, __args, 1);
                bool ok = ret && atoi(ret) != 0;
                delete[] ret;
                if (!ok) continue;
            }
            if (!audios[i].chunk) audios[i].chunk = Mix_LoadWAV(findfile(audios[i].path, "rb"));
            if (!audios[i].chunk) continue;
            int freq = 0, chans = 0; Uint16 fmt = 0; Mix_QuerySpec(&freq, &fmt, &chans);
            int samplesize = ((fmt & 0xFF) / 8) * chans;
            int bps = freq * samplesize;
            int offsetms = clamp(playtime - audios[i].start, 0, audios[i].duration);
            Uint32 startb = Uint32((double)(audios[i].from + offsetms) * bps / 1000.0);
            Uint32 endb = audios[i].to > audios[i].from ? Uint32((double)audios[i].to * bps / 1000.0) : audios[i].chunk->alen;
            if (endb > audios[i].chunk->alen) endb = audios[i].chunk->alen;
            startb -= startb % samplesize;
            endb -= endb % samplesize;
            if (startb >= endb) continue;
            if (audios[i].clip) { Mix_FreeChunk(audios[i].clip); audios[i].clip = NULL; }
            audios[i].clip = Mix_QuickLoad_RAW(audios[i].chunk->abuf + startb, endb - startb);
            if (!audios[i].clip) continue;
            audios[i].clip->allocated = 0;
            int ticks = audios[i].duration - offsetms;
            if (audios[i].to > audios[i].from) ticks = min(ticks, audios[i].to - audios[i].from - offsetms);
            if (ticks <= 0) continue;
            audios[i].channel = Mix_PlayChannelTimed(-1, audios[i].clip, 0, ticks);
        }
    }

    static void updatepostfxs(int playtime)
    {
        while (postfxindex < postfxs.length() && playtime >= postfxs[postfxindex].start + postfxs[postfxindex].duration) postfxindex++;
        bool addedfx = false;
        for (int i = postfxindex; i < postfxs.length() && playtime >= postfxs[i].start; ++i)
        {
            if (playtime > postfxs[i].start + postfxs[i].duration) continue;
            tagval __args[1];
            __args[0].setint(playtime - postfxs[i].start);
            char *sh = executestr(postfxs[i].script, __args, 1);
            if (sh && *sh)
            {
                defformatstring(cmd, "setpostfx %s 0 0 1 %f %f %f %f", sh, postfxs[i].params.x, postfxs[i].params.y, postfxs[i].params.z, postfxs[i].params.w);
                execute(cmd);
                addedfx = true;
            }
            delete[] sh;
        }
        if (!addedfx) execute("clearpostfx");
    }

    static void updateruncommands(int playtime)
    {
        for (int i = runcommandindex; i < runcommands.length() && playtime >= runcommands[i].start; ++i)
        {
            runcommand &rc = runcommands[i];
            if (rc.duration)
            {
                if (playtime < rc.start + rc.duration)
                {
                    tagval args[1];
                    args[0].setint(playtime - rc.start);
                    executestr(rc.script, args, 1);
                }
            }
            else if (!rc.executed)
            {
                tagval args[1];
                args[0].setint(playtime - rc.start);
                executestr(rc.script, args, 1);
                rc.executed = true;
            }
        }
        while (runcommandindex < runcommands.length() && playtime >= runcommands[runcommandindex].start + (runcommands[runcommandindex].duration ? runcommands[runcommandindex].duration : 1))
            runcommandindex++;
    }
}
