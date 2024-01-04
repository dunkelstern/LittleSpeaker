#ifndef LITTLESPEAKER_BLUETOOTH_H
#define LITTLESPEAKER_BLUETOOTH_H

#include <Arduino.h>
#include "BluetoothA2DPSink.h"
#include "menu.h"
#include "playlist.h"


class BluetoothPlayer {
    public:
        BluetoothPlayer(Playlist *playlist);
        ~BluetoothPlayer();

        Menu *makeMenu();
        Playlist *playlist;
    
        // Internal for menu handling
        BluetoothA2DPSink *makeSink();
        void destroySink();

        void previous();
        void next();
        void pause();

    private:
        BluetoothA2DPSink *a2dp;

};

#endif