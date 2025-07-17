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

    // draw the recorded camera path
    void rendercamerapath();

    // render camera model if enabled
    void rendercamera();

    // render camera feed overlay when debugging
    void rendercamerafeed();

    // returns true if playing or recording
    bool isactive();

    // returns true if currently recording
    bool isrecording();

    // true when the entity is the actor being recorded
    bool isrecordingactor(physent* d);
}

#endif // CUTSCENE_H