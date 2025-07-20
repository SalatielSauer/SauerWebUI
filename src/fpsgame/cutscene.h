#ifndef CUTSCENE_H
#define CUTSCENE_H

namespace cutscene
{
    // update playback or recording each frame
    void update(int curtime);

    // play back a cutscene between the given times (in milliseconds)
    void play(const char* file, int startms, int endms);

    // play back actors only, without moving the camera
    void playbackstart(const char* file, int startms, int endms);

    // begin recording camera frames to a file
    void recordstart(const char* file);

    // record on top of an existing cutscene
    void recordover(const char* file);

    // toggle pause state for playback or recording
    void pause();

    // stop playback or recording
    void stop();

    // append frames from another file
    void load(const char* file);

    // restart from the beginning
    void restart();

    // set the current time position
    void settime(int millis);

    // set the current frame index
    void setframe(int index);

    // remove camera frames (all=0, past=-1, future=1)
    void clearcam(int dir = 0);

    // remove actor frames for a specific actor
    void clearactor(int id);

    // draw the recorded camera path
    void rendercamerapath();

    // render camera model if enabled
    void rendercamera();

    // render camera feed overlay when debugging
    void rendercamerafeed();

    // store current camera settings for later interpolation
    void lerpcamfrom();

    // interpolate from the stored camera settings to the current ones
    // over the specified time in milliseconds
    void lerpcamto(int millis);

    // returns true if playing or recording
    bool isactive();

    // returns true if currently recording
    bool isrecording();

    // true when the entity is the actor being recorded
    bool isrecordingactor(physent* d);

    // return the id for the given actor (-1 if not an actor)
    int actorid(physent* d);

    // return the number of frames currently loaded
    int frameslen();

    // return the current time position in milliseconds
    int currenttime();

    // return the current frame index
    int currentframe();
}

#endif // CUTSCENE_H