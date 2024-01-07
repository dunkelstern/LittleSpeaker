#ifndef LITTLESPEAKER_WEBRADIO_H
#define LITTLESPEAKER_WEBRADIO_H

#include <Arduino.h>
#include "menu.h"
#include "playlist.h"

#define MAX_WEBRADIO_STATIONS 25

class WebradioPlayer {
    public:
        WebradioPlayer(Playlist *playlist);
        ~WebradioPlayer();

        Menu *makeMenu();
        Playlist *playlist;
    
        // Internal for menu handling
        void play(int index);
        void previous();
        void next();
        void pause();

        void reset();
    private:
        void announce(int index);

        int urloffset[MAX_WEBRADIO_STATIONS];
        int numRadioStations;
        int currentItem;

};

#endif
