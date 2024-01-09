#ifndef LITTLESPEAKER_BLUETOOTH_H
#define LITTLESPEAKER_BLUETOOTH_H

#include <Arduino.h>
#include "BluetoothA2DPSink.h"
#include "AudioOutputFilter3BandEQ.h"
#include "menu.h"
#include "playlist.h"

typedef enum _BTState {
    BTStateStopped = 0,
    BTStatePlaying = 1,
    BTStatePaused = 2
} BTState;

class BluetoothPlayer {
    public:
        BluetoothPlayer(Playlist *playlist, AudioOutputFilter3BandEQ *eq = NULL);
        ~BluetoothPlayer();

        Menu *makeMenu();
        Playlist *playlist;
    
        // Internal for menu handling
        BluetoothA2DPSink *makeSink();
        void destroySink();

        void previous();
        void next();
        void pause();

        BluetoothA2DPSink *a2dp;
        AudioOutputFilter3BandEQ *eq;
        void setState(BTState state);

        private:
            BTState state;
};

#endif