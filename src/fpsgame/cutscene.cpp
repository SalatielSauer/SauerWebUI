#include "game.h"
#include "cutscene.h"
#include "engine.h"

extern bool detachedcamera;

extern float aspect, fovy, curfov;
extern float nearplane;
extern int farplane, screenw, screenh;
extern matrix4 projmatrix;
extern vec curfogcolor;
extern void setcamprojmatrix(bool init = true, bool flush = false);
extern void setcammatrix();

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

    static vector<fpsent*> actors;
    static vector<int> actorindex;

    static int starttime = 0, endtime = 0;
    static int camindex = 0;
    static int numactors = 0, curactor = -1;
    static bool playing = false, recording = false, paused = false, usecamera = true;
    static bool showcameramodel = false;
    static frame cameraprev;
    static bool havecamera = false;
    static frame lerpcamstart;
    static bool havelerpcamstart = false;
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

    static bool readframes(const char* fn, vector<frame>& cam, vector< vector<frame> >& actors, vector<int>& models, int& maxactor)
    {
        size_t len = 0;
        char* buf = loadfile(path(formatfile(fn), true), &len);
        if (!buf) return false;
        char* line = buf;
        while (line)
        {
            char* next = strchr(line, '\n');
            if (next) *next++ = '\0';
            int id, mdl;
            if (sscanf(line, "model %d %d", &id, &mdl) == 2)
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
        int maxactor = -1;
        if (!readframes(file, cam, acts, models, maxactor) || (cam.empty() && acts.empty()))
        {
            conoutf(CON_ERROR, "could not load cutscene %s", file);
            return;
        }
        cameraframes.move(cam);
        actorframes.move(acts);
        actormodels.move(models);
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
        loopv(actorframes) if (actorframes[i].length()) lasttime = max(lasttime, actorframes[i].last().time);
        endtime = endms > 0 ? min(endms, lasttime) : lasttime;
        starttime = lastmillis - startms;
        camindex = 0;
        loopi(numactors) actorindex[i] = 0;
        while (camindex < cameraframes.length() && cameraframes[camindex].time < startms) camindex++;
        loopi(numactors) while (actorindex[i] < actorframes[i].length() && actorframes[i][actorindex[i]].time < startms) actorindex[i]++;
        playing = true;
        recording = false;
        paused = false;
        usecamera = camera;
        detachedcamera = camera;
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
        loopv(actormodels) outfile->printf("model %d %d\n", i, actormodels[i]);
        copystring(filename, formatfile(file));
        DELETEA(cutscenecurrentfile);
        cutscenecurrentfile = newstring(filename);
        updateframeslen();
        starttime = lastmillis;
        camindex = 0;
        playing = false;
        recording = true;
        paused = false;
        detachedcamera = true;
        conoutf(CON_DEBUG, "recording cutscene to %s", file);
    }

    void recordover(const char* file)
    {
        stop();
        vector<frame> cam;
        vector< vector<frame> > acts;
        vector<int> models;
        int maxactor = -1;
        if (!readframes(file, cam, acts, models, maxactor))
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
        numactors = maxactor + 1;
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

        copystring(filename, formatfile(file));
        DELETEA(cutscenecurrentfile);
        cutscenecurrentfile = newstring(filename);
        int lasttime = cameraframes.empty() ? 0 : cameraframes.last().time;
        loopv(actorframes) if (actorframes[i].length()) lasttime = max(lasttime, actorframes[i].last().time);
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
        loopv(actormodels) outfile->printf("model %d %d\n", i, actormodels[i]);
        if (!spec) loopv(cameraframes) writeframe(outfile, cameraframes[i]);
        loopv(actorframes) loopvj(actorframes[i]) writeframe(outfile, actorframes[i][j]);

        playing = true;
        recording = true;
        paused = false;
        usecamera = false;
        detachedcamera = false;
        conoutf(CON_DEBUG, "recording over cutscene %s", file);
    }

    void update(int curtime)
    {
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
            paused = true;
            conoutf(CON_DEBUG, "cutscene paused");
        }
        else
        {
            starttime += lastmillis - pausestart;
            paused = false;
            pauseframes.shrink(0);
            conoutf(CON_DEBUG, "cutscene resumed");
        }
    }

    void stop()
    {
        if (outfile) { delete outfile; outfile = NULL; }
        if (!isactive() && actors.empty()) return;
        playing = recording = false;
        paused = false;
        pauseframes.shrink(0);
        cameraframes.shrink(0);
        actorframes.shrink(0);
        actormodels.shrink(0);
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
        int maxa = -1;
        if (!readframes(formatfile(file), cam, acts, models, maxa) || (cam.empty() && acts.empty())) return;
        int offset = 0;
        if (!cameraframes.empty()) offset = max(offset, cameraframes.last().time);
        loopv(actorframes) if (actorframes[i].length()) offset = max(offset, actorframes[i].last().time);
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
        conoutf(CON_DEBUG, "cutscene restarted");
    }

    void settime(int millis)
    {
        if (!isactive()) return;
        starttime = lastmillis - millis;
        camindex = 0;
        loopi(numactors) actorindex[i] = 0;
        while (camindex < cameraframes.length() && cameraframes[camindex].time < millis) camindex++;
        loopi(numactors) while (actorindex[i] < actorframes[i].length() && actorframes[i][actorindex[i]].time < millis) actorindex[i]++;
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

    void rendercamerapath()
    {
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

        const int step = 50;
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

}
