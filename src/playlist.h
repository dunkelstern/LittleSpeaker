#ifndef LITTLESPEAKER_PLAYLIST_H
#define LITTLESPEAKER_PLAYLIST_H

#include "AudioFileSource.h"
#include "AudioGenerator.h"

typedef enum _PlaybackState {
    PlaybackStateStopped = 0,
    PlaybackStatePlaying = 1,
    PlaybackStatePaused = 2,
    PlaybackStateSkipping = 3
} PlaybackState;

class Playlist {
    public:
        Playlist(AudioOutput *output, int maxEntries = 10);
        ~Playlist();

        bool addFilename(const char *filename);
        PlaybackState getState();
        void play();
        void pause();
        void skip();
        void stopAndClear();
        void freeAllBuffers();

        // Internal
        char *consumeItem();
        AudioGenerator *getDecoderForFile(const char *filename);
        AudioFileSource *getAudioFileSourceForFile(const char *filename);
        AudioOutput *getOutput();

    private:
        AudioOutput *output;
        char **itemRingbuffer;
        int ringbufferSize;
        int readMarker;
        int writeMarker;
        SemaphoreHandle_t ringBufferMutex;
        PlaybackState state;
        TaskHandle_t playbackTask;
        char *preallocateBuffer;
        char *preallocateCodec;
};

#endif
