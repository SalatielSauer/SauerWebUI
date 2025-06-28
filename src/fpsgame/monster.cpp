// monster.h: implements AI for single player monsters, currently client only
#include "game.h"

extern int physsteps;

namespace game
{
    static vector<int> teleports;

    // SauerWUI - custom monsters
    /*static const int TOTMFREQ = 14;
    static const int NUMMONSTERTYPES = 9;*/

    struct monstertype
    {
        // SauerWUI - custom monsters
        /*short gun, speed, health, freq, lag, rate, pain, loyalty, bscale, weight;
        short painsound, diesound;
        const char* name, * mdlname, * vwepname;*/
        int gun, speed, health, freq, lag, rate, pain, loyalty, bscale, weight;
        int painsound, diesound;
        int puppet;
        string name, mdlname, vwepname;
    };

    // SauerWUI - custom monsters
    /*static const monstertype monstertypes[NUMMONSTERTYPES] =
    {
        { GUN_FIREBALL,  15, 100, 3, 0,   100, 800, 1, 10,  90, S_PAINO, S_DIE1,   "an ogro",     "ogro",       "ogro/vwep"},
        { GUN_CG,        18,  70, 2, 70,   10, 400, 2, 10,  50, S_PAINR, S_DEATHR, "a rhino",     "monster/rhino",      NULL},
        { GUN_SG,        13, 120, 1, 100, 300, 400, 4, 14, 115, S_PAINE, S_DEATHE, "ratamahatta", "monster/rat",        "monster/rat/vwep"},
        { GUN_RIFLE,     14, 200, 1, 80,  400, 300, 4, 18, 145, S_PAINS, S_DEATHS, "a slith",     "monster/slith",      "monster/slith/vwep"},
        { GUN_RL,        12, 500, 1, 0,   200, 200, 6, 24, 210, S_PAINB, S_DEATHB, "bauul",       "monster/bauul",      "monster/bauul/vwep"},
        { GUN_BITE,      24,  50, 3, 0,   100, 400, 1, 15,  75, S_PAINP, S_PIGGR2, "a hellpig",   "monster/hellpig",    NULL},
        { GUN_ICEBALL,   11, 250, 1, 0,    10, 400, 6, 18, 160, S_PAINH, S_DEATHH, "a knight",    "monster/knight",     "monster/knight/vwep"},
        { GUN_SLIMEBALL, 15, 100, 1, 0,   200, 400, 2, 10,  60, S_PAIND, S_DEATHD, "a goblin",    "monster/goblin",     "monster/goblin/vwep"},
        { GUN_GL,        22,  50, 1, 0,   200, 400, 1, 10,  40, S_PAIND, S_DEATHD, "a spider",    "monster/spider",      NULL },
    };*/
    static vector<monstertype> monstertypes;

    VAR(skill, 1, 3, 10);
    VAR(killsendsp, 0, 1, 1);
    VAR(monsteraiinterval, 0, 50, 60000); // SauerWUI - custom monsters AI

    bool monsterhurt;
    vec monsterhurtpos;
    
    struct monster : fpsent
    {
        int monsterstate;                   // one of M_*, M_NONE means human
    
        int mtype, tag;                     // see monstertypes table
        fpsent *enemy;                      // monster wants to kill this entity
        float targetyaw;                    // monster wants to look in this direction
        int trigger;                        // millis at which transition to another monsterstate takes place
        vec attacktarget;                   // delayed attacks
        int anger;                          // how many times already hit by fellow monster
        physent *stacked;
        vec stackpos;

        // SauerWUI - custom monsters AI
        int lastai;
        int defaultai = 0;
    
        monster(int _type, int _yaw, int _tag, int _state, int _trigger, int _move) :
            monsterstate(_state), tag(_tag),
            stacked(NULL),
            stackpos(0, 0, 0),
            lastai(0)
        {
            type = ENT_AI;
            respawn();

            // SauerWUI - custom monsters
            //if(_type>=NUMMONSTERTYPES || _type < 0)
            if(_type<0 || _type>=monstertypes.length())
            {
                conoutf(CON_WARN, "warning: unknown monster in spawn: %d", _type);
                _type = 0;
            }

            mtype = _type;
            const monstertype &t = monstertypes[mtype];
            eyeheight = 8.0f;
            aboveeye = 7.0f;
            radius *= t.bscale/10.0f;
            xradius = yradius = radius;
            eyeheight *= t.bscale/10.0f;
            aboveeye *= t.bscale/10.0f;
            weight = t.weight;
            if(_state!=M_SLEEP) spawnplayer(this);
            trigger = lastmillis+_trigger;
            targetyaw = yaw = (float)_yaw;
            move = _move;
            enemy = player1;
            gunselect = t.gun;
            maxspeed = (float)t.speed*4;
            health = t.health;
            armour = 0;
            loopi(NUMGUNS) ammo[i] = 10000;
            pitch = 0;
            roll = 0;
            state = CS_ALIVE;
            anger = 0;
            copystring(name, t.name);
        }
       
        void normalize_yaw(float angle)
        {
            while(yaw<angle-180.0f) yaw += 360.0f;
            while(yaw>angle+180.0f) yaw -= 360.0f;
        }
 
        // monster AI is sequenced using transitions: they are in a particular state where
        // they execute a particular behaviour until the trigger time is hit, and then they
        // reevaluate their situation based on the current state, the environment etc., and
        // transition to the next state. Transition timeframes are parametrized by difficulty
        // level (skill), faster transitions means quicker decision making means tougher AI.

        void transition(int _state, int _moving, int n, int r) // n = at skill 0, n/2 = at skill 10, r = added random factor
        {
            if (monstertypes[mtype].puppet == 0 || defaultai == 1)
            {
                monsterstate = _state;
                move = _moving;
            }

            n = n*130/100;
            trigger = lastmillis+n-skill*(n/16)+rnd(r+1);
        }


        void monsteraction(int curtime)           // main AI thinking routine, called every frame for every monster
        {
            defaultai = 0; // SauerWUI - custom monsters AI

            if (enemy->state == CS_DEAD) { enemy = player1; anger = 0; }
            normalize_yaw(targetyaw);
            if (targetyaw > yaw)             // slowly turn monster towards his target
            {
                yaw += curtime * 0.5f;
                if (targetyaw < yaw) yaw = targetyaw;
            }
            else
            {
                yaw -= curtime * 0.5f;
                if (targetyaw > yaw) yaw = targetyaw;
            }
            float dist = enemy->o.dist(o);
            if (monsterstate != M_SLEEP) pitch = dist > 0 ? asin((enemy->o.z - o.z) / dist) / RAD : 0;

            // SauerWUI - custom monsters AI (WIP)
            if (monstertypes[mtype].puppet == 1)
            {
                if (identexists("level_monsterai") && (lastmillis - lastai >= monsteraiinterval))
                {
                    tagval args[18];
                    args[0].setint(tag); // monster tag
                    args[1].setint(enemy->type); // monster type
                    args[2].setfloat(dist); // enemy distance
                    float enemyyaw = -atan2(enemy->o.x - o.x, enemy->o.y - o.y) / RAD;
                    float enemypitch = dist > 0 ? asin((enemy->o.z - o.z) / dist) / RAD : 0;
                    vec t;
                    bool inview = false;
                    if (raycubelos(o, enemy->o, t))
                    {
                        float yd = fabs(yaw - enemyyaw); if (yd > 180.f) yd = 360.f - yd;
                        float pd = fabs(pitch - enemypitch); if (pd > 180.f) pd = 360.f - pd;
                        if (yd <= 90.f && pd <= 60.f) inview = true;
                    }
                    args[3].setint(inview ? 1 : 0);         // enemy visible
                    args[4].setint(enemy->gunselect);       // enemy gun
                    args[5].setint(enemy->health);          // enemy health
                    args[6].setint(enemy->armour);          // enemy armour
                    args[7].setint(enemy->state);           // enemy state
                    defformatstring(eyawpitch, "%f %f", enemyyaw, enemypitch);
                    args[8].setstr(newstring(eyawpitch));   // enemy yaw pitch

                    defformatstring(myawpitch, "%f %f", yaw, pitch);
                    args[9].setstr(newstring(myawpitch));   // monster yaw pitch
                    defformatstring(posbuf, "%f %f %f", o.x, o.y, o.z);
                    args[10].setstr(newstring(posbuf));     // monster position
                    args[11].setint(gunselect);             // monster weapon
                    args[12].setint(health);                // monster health
                    args[13].setint(monsterstate);          // monster state
                    args[14].setint(lastmillis);            // millis
                    defformatstring(movbuf, "%d %d", move, strafe);
                    args[15].setstr(newstring(movbuf));     // monster movement
                    args[16].setint(anger);                 // monster anger
                    args[17].setint(blocked ? 1 : 0);       // monster is blocked

                    ident* ai = idents.access("level_monsterai");
                    char* ret = ai ? executestr(ai, args, 18, true) : NULL;
                    if (ret)
                    {
                        int action = -1; char arg1[256] = ""; float b = 0.0f, c = 0.0f;
                        sscanf(ret, "%d %255[^\n] %f %f", &action, arg1, &b, &c);
                        float a = strtod(arg1, NULL);
                        //conoutf(CON_DEBUG, arg1);
                        delete[] ret;
                        switch (action)
                        {
                        case 0:
                        {
                            if (defaultai == 0)
                            {
                                lastaction = 0;
                                defaultai = 1;
                                transition(M_HOME, 1, 800, 500);
                            }
                            break;
                        }

                        case 1: // attack: arg1 = "x y z" (optional attack position), fallback to enemy->o if not provided, b & c unused
                            if (enemy->state == CS_ALIVE)
                            {
                                vec attackpos;
                                float px, py, pz;
                                int n = sscanf(arg1, "%f %f %f", &px, &py, &pz);

                                if (n == 3) attackpos = vec(px, py, pz);
                                else attackpos = enemy->o;

                                attacktarget = attackpos;
                                transition(M_AIMING, 0, monstertypes[mtype].lag, 10);
                                lastaction = 0;
                                attacking = true;
                                shoot(this, attacktarget);
                            }
                            break;
                        case 2: // move: a = move, b = strafe, c unused
                        {
                            move = (int)a; strafe = (int)b;
                            moveplayer(this, 1, true);
                            break;
                        }

                        case 3: // jump: jump forever
                            jumping = true; break;
                        case 4: // set aim: a = yaw, b = pitch, c unused
                            targetyaw = a; pitch = b; break;
                        case 5: // play sound: a = sound id, rest unused
                            playsound(int(a), &o); break;
                        default:
                            break;
                        }
                    }
                    lastai = lastmillis;
                }

                //return;
            }

            if (monstertypes[mtype].puppet == 0 || defaultai == 1) { // SauerWUI - custom monsters AI
                if (blocked)                                                              // special case: if we run into scenery
                {
                    blocked = false;
                    if (!rnd(20000 / monstertypes[mtype].speed))                            // try to jump over obstackle (rare)
                    {
                        jumping = true;
                    }
                    else if (trigger < lastmillis && (monsterstate != M_HOME || !rnd(5)))  // search for a way around (common)
                    {
                        targetyaw += 90 + rnd(180);                                          // patented "random walk" AI pathfinding (tm) ;)
                        transition(M_SEARCH, 1, 100, 1000);
                    }
                }

                float enemyyaw = -atan2(enemy->o.x - o.x, enemy->o.y - o.y) / RAD;

                switch (monsterstate)
                {
                case M_PAIN:
                case M_ATTACKING:
                case M_SEARCH:
                    if (trigger < lastmillis) transition(M_HOME, 1, 100, 200);
                    break;

                case M_SLEEP:                       // state classic sp monster start in, wait for visual contact
                {
                    if (editmode) break;
                    normalize_yaw(enemyyaw);
                    float angle = (float)fabs(enemyyaw - yaw);
                    if (dist < 32                   // the better the angle to the player, the further the monster can see/hear
                        || (dist < 64 && angle < 135)
                        || (dist < 128 && angle < 90)
                        || (dist < 256 && angle < 45)
                        || angle < 10
                        || (monsterhurt && o.dist(monsterhurtpos) < 128))
                    {
                        vec target;
                        if (raycubelos(o, enemy->o, target))
                        {
                            transition(M_HOME, 1, 500, 200);
                            playsound(S_GRUNT1 + rnd(2), &o);
                        }
                    }
                    break;
                }

                case M_AIMING:                      // this state is the delay between wanting to shoot and actually firing
                    if (trigger < lastmillis)
                    {
                        lastaction = 0;
                        attacking = true;
                        shoot(this, attacktarget);
                        transition(M_ATTACKING, 0, 600, 0);
                    }
                    break;

                case M_HOME:                        // monster has visual contact, heads straight for player and may want to shoot at any time
                    targetyaw = enemyyaw;
                    if (trigger < lastmillis)
                    {
                        vec target;
                        if (!raycubelos(o, enemy->o, target))    // no visual contact anymore, let monster get as close as possible then search for player
                        {
                            transition(M_HOME, 1, 800, 500);
                        }
                        else
                        {
                            bool melee = false, longrange = false;
                            switch (monstertypes[mtype].gun)
                            {
                            case GUN_BITE:
                            case GUN_FIST: melee = true; break;
                            case GUN_RIFLE: longrange = true; break;
                            }
                            // the closer the monster is the more likely he wants to shoot, 
                            if ((!melee || dist < 20) && !rnd(longrange ? (int)dist / 12 + 1 : min((int)dist / 12 + 1, 6)) && enemy->state == CS_ALIVE)      // get ready to fire
                            {
                                attacktarget = target;
                                transition(M_AIMING, 0, monstertypes[mtype].lag, 10);
                            }
                            else                                                        // track player some more
                            {
                                transition(M_HOME, 1, monstertypes[mtype].rate, 0);
                            }
                        }
                    }
                    break;

                }
            }
            if (move || maymove() || (stacked && (stacked->state != CS_ALIVE || stackpos != stacked->o)))
            {
                vec pos = feetpos();
                loopv(teleports) // equivalent of player entity touch, but only teleports are used
                {
                    entity& e = *entities::ents[teleports[i]];
                    float dist = e.o.dist(pos);
                    if (dist < 16) entities::teleport(teleports[i], this);
                }

                if (physsteps > 0) stacked = NULL;
                moveplayer(this, 1, true);        // use physics to move monster
            }
        }

        void monsterpain(int damage, fpsent *d)
        {
            if(d->type==ENT_AI)     // a monster hit us
            {
                if(this!=d)            // guard for RL guys shooting themselves :)
                {
                    anger++;     // don't attack straight away, first get angry
                    int _anger = d->type==ENT_AI && mtype==((monster *)d)->mtype ? anger/2 : anger;
                    if(_anger>=monstertypes[mtype].loyalty) enemy = d;     // monster infight if very angry
                }
            }
            else if(d->type==ENT_PLAYER) // player hit us
            {
                anger = 0;
                enemy = d;
                monsterhurt = true;
                monsterhurtpos = o;
            }
            damageeffect(damage, this);
            if((health -= damage)<=0)
            {
                state = CS_DEAD;
                lastpain = lastmillis;
                playsound(monstertypes[mtype].diesound, &o);
                monsterkilled();
                gibeffect(max(-health, 0), vel, this);

                defformatstring(id, "monster_dead_%d", tag);
                execident(id);
            }
            else
            {
                transition(M_PAIN, 0, monstertypes[mtype].pain, 200);      // in this state monster won't attack
                playsound(monstertypes[mtype].painsound, &o);
            }
        }
    };

    void stackmonster(monster *d, physent *o)
    {
        d->stacked = o;
        d->stackpos = o->o;
    }

    int nummonsters(int tag, int state)
    {
        int n = 0;
        loopv(monsters) if(monsters[i]->tag==tag && (monsters[i]->state==CS_ALIVE ? state!=1 : state>=1)) n++;
        return n;
    }
    ICOMMAND(nummonsters, "ii", (int *tag, int *state), intret(nummonsters(*tag, *state)));

    void preloadmonsters()
    {
        // SauerWUI - custom monsters
        //loopi(NUMMONSTERTYPES) preloadmodel(monstertypes[i].mdlname);
        loopv(monstertypes) preloadmodel(monstertypes[i].mdlname);

        for(int i = S_GRUNT1; i <= S_SLIMEBALL; i++) preloadsound(i);
        if(m_dmsp) preloadsound(S_V_FIGHT);
        if(m_classicsp) preloadsound(S_V_RESPAWNPOINT);
    }

    static monstertype *loadingmonster = NULL;

    // SauerWUI - custom monsters
    ICOMMAND(monsterweapon, "i", (int *gun), if(loadingmonster) loadingmonster->gun = *gun;);
    ICOMMAND(monsterspeed, "i", (int *spd), if(loadingmonster) loadingmonster->speed = *spd;);
    ICOMMAND(monsterhealth, "i", (int *hp), if(loadingmonster) loadingmonster->health = *hp;);
    ICOMMAND(monsterfreq, "i", (int *fq), if(loadingmonster) loadingmonster->freq = *fq;);
    ICOMMAND(monsterlag, "i", (int *lg), if(loadingmonster) loadingmonster->lag = *lg;);
    ICOMMAND(monsterrate, "i", (int *rt), if(loadingmonster) loadingmonster->rate = *rt;);
    ICOMMAND(monsterpain, "i", (int *pn), if(loadingmonster) loadingmonster->pain = *pn;);
    ICOMMAND(monsterloyalty, "i", (int *ly), if(loadingmonster) loadingmonster->loyalty = *ly;);
    ICOMMAND(monsterbscale, "i", (int *bs), if(loadingmonster) loadingmonster->bscale = *bs;);
    ICOMMAND(monsterweight, "i", (int *wt), if(loadingmonster) loadingmonster->weight = *wt;);
    ICOMMAND(monsterpainsound, "i", (int *ps), if(loadingmonster) loadingmonster->painsound = *ps;);
    ICOMMAND(monsterdiesound, "i", (int *ds), if(loadingmonster) loadingmonster->diesound = *ds;);
    ICOMMAND(monsterpuppet, "i", (int* pp), if (loadingmonster) loadingmonster->puppet = *pp;);
    ICOMMAND(monstername, "s", (char *n), if(loadingmonster) copystring(loadingmonster->name, n ? n : ""));
    ICOMMAND(monstermodel, "s", (char *n), if(loadingmonster) copystring(loadingmonster->mdlname, n ? n : ""));
    ICOMMAND(monstervwep, "s", (char *n), if(loadingmonster) copystring(loadingmonster->vwepname, n ? n : ""));

    // SauerWUI - custom monsters
    void loadmonster(char* cfg)
    {
        monstertype mt;
        copystring(mt.name, "an ogro");
        copystring(mt.mdlname, "ogro");
        copystring(mt.vwepname, "ogro/vwep");
        mt.gun = GUN_FIREBALL;
        mt.speed = 15;
        mt.health = 100;
        mt.freq = 3;
        mt.lag = 0;
        mt.rate = 100;
        mt.pain = 800;
        mt.loyalty = 1;
        mt.bscale = 10;
        mt.weight = 90;
        mt.painsound = S_PAINO;
        mt.diesound = S_DIE1;
        mt.puppet = 0;
        loadingmonster = &mt;
        execfile(cfg, false);
        loadingmonster = NULL;
        monstertypes.add(mt);
    }
    COMMAND(loadmonster, "s");

    // SauerWUI - custom monsters
    static void clearmonsterdefs()
    {
        clearmonsters();
        monstertypes.shrink(0);
    }
    COMMANDN(clearmonsters, clearmonsterdefs, "");

    vector<monster *> monsters;
    
    int nextmonster, spawnremain, numkilled, monstertotal, mtimestart, remain;
    
    void spawnmonster()     // spawn a random monster according to freq distribution in DMSP
    {
        // SauerWUI - custom monsters
        /*int n = rnd(TOTMFREQ), type;
        for(int i = 0; ; i++) if((n -= monstertypes[i].freq)<0) { type = i; break; }*/
        int tot = 0;
        loopv(monstertypes) tot += monstertypes[i].freq;
        if (!tot) return;
        int n = rnd(tot), type = 0;
        for (int i = 0; i < monstertypes.length(); i++) if ((n -= monstertypes[i].freq) < 0) { type = i; break; }

        monsters.add(new monster(type, rnd(360), 0, M_SEARCH, 1000, 1));
    }

    void clearmonsters()     // called after map start or when toggling edit mode to reset/spawn all monsters to initial state
    {
        if (monstertypes.empty()) return; // SauerWUI - custom monsters

        removetrackedparticles();
        removetrackeddynlights();
        loopv(monsters) delete monsters[i]; 
        cleardynentcache();
        monsters.shrink(0);
        numkilled = 0;
        monstertotal = 0;
        spawnremain = 0;
        remain = 0;
        monsterhurt = false;
        if(m_dmsp)
        {
            nextmonster = mtimestart = lastmillis+10000;
            monstertotal = spawnremain = skill*10;
        }
        else if(m_classicsp)
        {
            mtimestart = lastmillis;
            loopv(entities::ents)
            {
                extentity &e = *entities::ents[i];
                if(e.type!=MONSTER) continue;
                monster *m = new monster(e.attr2, e.attr1, e.attr3, M_SLEEP, 100, 0);  
                monsters.add(m);
                m->o = e.o;
                entinmap(m);
                updatedynentcache(m);
                monstertotal++;
            }
        }
        teleports.setsize(0);
        if(m_dmsp || m_classicsp)
        {
            loopv(entities::ents) if(entities::ents[i]->type==TELEPORT) teleports.add(i);
        }
    }

    void endsp(bool allkilled)
    {
        conoutf(CON_GAMEINFO, allkilled ? "\f2you have cleared the map!" : "\f2you reached the exit!");
        monstertotal = 0;
        forceintermission();
    }
    ICOMMAND(endsp, "", (), endsp(false));

    
    void monsterkilled()
    {
        numkilled++;
        player1->frags = numkilled;
        remain = monstertotal-numkilled;
        if(remain>0 && remain<=5) conoutf(CON_GAMEINFO, "\f2only %d monster(s) remaining", remain);
    }

    void updatemonsters(int curtime)
    {
        if (monstertypes.empty()) return; // SauerWUI - custom monsters
        if(m_dmsp && spawnremain && lastmillis>nextmonster)
        {
            if(spawnremain--==monstertotal) { conoutf(CON_GAMEINFO, "\f2The invasion has begun!"); playsound(S_V_FIGHT); }
            nextmonster = lastmillis+1000;
            spawnmonster();
        }
        
        if(killsendsp && monstertotal && !spawnremain && numkilled==monstertotal) endsp(true);
        
        bool monsterwashurt = monsterhurt;
        
        loopv(monsters)
        {
            if(monsters[i]->state==CS_ALIVE) monsters[i]->monsteraction(curtime);
            else if(monsters[i]->state==CS_DEAD)
            {
                if(lastmillis-monsters[i]->lastpain<2000)
                {
                    //monsters[i]->move = 0;
                    monsters[i]->move = monsters[i]->strafe = 0;
                    moveplayer(monsters[i], 1, true);
                }
            }
        }
        
        if(monsterwashurt) monsterhurt = false;
    }

    void rendermonsters()
    {
        if (monstertypes.empty()) return; // SauerWUI - custom monsters
        loopv(monsters)
        {
            monster &m = *monsters[i];
            if(m.state!=CS_DEAD || lastmillis-m.lastpain<10000)
            {
                modelattach vwep[2];
                vwep[0] = modelattach("tag_weapon", monstertypes[m.mtype].vwepname, ANIM_VWEP_IDLE|ANIM_LOOP, 0);
                float fade = 1;
                if(m.state==CS_DEAD) fade -= clamp(float(lastmillis - (m.lastpain + 9000))/1000, 0.0f, 1.0f);
                renderclient(&m, monstertypes[m.mtype].mdlname, vwep, 0, m.monsterstate==M_ATTACKING ? -ANIM_ATTACK1 : 0, 300, m.lastaction, m.lastpain, fade);
            }
        }
    }

    void suicidemonster(monster *m)
    {
        m->monsterpain(400, player1);
    }

    void hitmonster(int damage, monster *m, fpsent *at, const vec &vel, int gun)
    {
        m->monsterpain(damage, at);
    }

    void spsummary(int accuracy)
    {
        conoutf(CON_GAMEINFO, "\f2--- single player time score: ---");
        int pen, score = 0;
        pen = ((lastmillis-maptime)*100)/game::scaletime(1000); score += pen; if(pen) conoutf(CON_GAMEINFO, "\f2time taken: %d seconds (%d simulated seconds)", pen, (lastmillis-maptime)/1000);
        pen = player1->deaths*60; score += pen; if(pen) conoutf(CON_GAMEINFO, "\f2time penalty for %d deaths (1 minute each): %d seconds", player1->deaths, pen);
        pen = remain*10;          score += pen; if(pen) conoutf(CON_GAMEINFO, "\f2time penalty for %d monsters remaining (10 seconds each): %d seconds", remain, pen);
        pen = (10-skill)*20;      score += pen; if(pen) conoutf(CON_GAMEINFO, "\f2time penalty for lower skill level (20 seconds each): %d seconds", pen);
        pen = 100-accuracy;       score += pen; if(pen) conoutf(CON_GAMEINFO, "\f2time penalty for missed shots (1 second each %%): %d seconds", pen);
        defformatstring(aname, "bestscore_%s", getclientmap());
        const char *bestsc = getalias(aname);
        int bestscore = *bestsc ? parseint(bestsc) : score;
        if(score<bestscore) bestscore = score;
        defformatstring(nscore, "%d", bestscore);
        alias(aname, nscore);
        conoutf(CON_GAMEINFO, "\f2TOTAL SCORE (time + time penalties): %d seconds (best so far: %d seconds)", score, bestscore);
    }
}

