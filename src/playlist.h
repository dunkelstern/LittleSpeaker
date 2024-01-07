#ifndef LITTLESPEAKER_PLAYLIST_H
#define LITTLESPEAKER_PLAYLIST_H

#include "AudioFileSource.h"
#include "AudioGenerator.h"

typedef enum _PlaybackState {
    PlaybackStateStopped = 0,
    PlaybackStatePlaying = 1,
    PlaybackStatePaused = 2,
    PlaybackStateSkipping = 3,
    PlaybackStateReset = 4
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

        void loop();

        void registerPlaylistEndCallback(void (*callback)(void *context), void *context);

    private:
        char *consumeItem();
        bool setupDecoderForFile(const char *filename);
        bool setupAudioSourceForFile(const char *filename);
        void destroyAudioChain();

        AudioFileSource *base;
        AudioFileSource *source;
        AudioGenerator *decoder;
        AudioOutput *output;
        char **itemRingbuffer;
        int ringbufferSize;
        int readMarker;
        int writeMarker;
        SemaphoreHandle_t ringBufferMutex;
        PlaybackState state;
        TaskHandle_t playbackTask;
        char *preallocateBuffer;
        void (*endCallback)(void *);
        void *endContext;
};

#endif
