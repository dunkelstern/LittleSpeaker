#ifndef LITTLESPEAKER_SDCARD_H
#define LITTLESPEAKER_SDCARD_H

#include <Arduino.h>
#include "menu.h"
#include "playlist.h"

#define MAX_ALBUMS 99
#define MAX_TRACKS 999

typedef enum _SDState {
    SDStateAlbumMenu = 0,
    SDStateAlbumPlayback = 1
} SDState;

class SDPlayer {
    public:
        SDPlayer(Playlist *playlist);
        ~SDPlayer();

        Menu *makeMenu();
        Playlist *playlist;
    
        // Internal for menu handling
        void play(int8_t albumIndex, int16_t trackIndex, bool reset);
        bool previous(bool announce = true, bool loop = true);
        bool next(bool announce = true, bool loop = true);
        void pause();

        void reset();
        SDState getState();
        void setState(SDState state);
    
    private:
        void announce(int8_t albumIndex, int16_t trackIndex);
        char *pathOfAlbumAtIndex(int8_t albumIndex);
        char *nameOfTrackAtIndex(char *path, int16_t trackIndex);
        int16_t indexForFilenameAtPath(char *filename, char *path);
        char *switchAlbum(int8_t albumIndex);

        int8_t currentAlbum;
        int16_t currentTrack;

        int8_t maxAlbum;
        int16_t maxTrack;
        uint16_t *shuffle;

        SDState state;
};

#endif