#include "sdcard.h"

#include <SD.h>

void sdPrev(Menu *item);
void sdNext(Menu *item);
void sdPlayPause(Menu *item);
bool sdLeave(Menu *item);
void sdEnter(Menu *item);
void sdPlaylistEnd(void *context);


SDPlayer::SDPlayer(Playlist *playlist) {
    this->playlist = playlist;

    this->currentAlbum = 0;
    this->currentTrack = 0;
    this->shuffle = reinterpret_cast<uint16_t *>(malloc(sizeof(uint16_t) * MAX_TRACKS));
    this->state = SDStateAlbumMenu;

    // scan SD card
    this->maxAlbum = 0;
    this->maxTrack = 0;

    File dir = SD.open("/");
    File file;
    bool finished = false;
    do {
        file = dir.openNextFile("r");
        if (!file) {
            finished = true;
            continue;
        }
        if (file.isDirectory()) {
            const char *filename = file.name();

            if (filename[0] == '.') continue;
            if (strcasecmp("system", filename) == 0) continue;
            if (strcasecmp("webradio", filename) == 0) continue;
            Serial.printf("%d, Found directory: %s\n", this->maxAlbum, filename);
            this->maxAlbum++;
        }
        file.close();
    } while (!finished);
    dir.close();

    this->switchAlbum(0);
}

SDPlayer::~SDPlayer() {
}

Menu* SDPlayer::makeMenu() {
    ButtonMenu *sdMenu = new ButtonMenu(sdPrev, sdNext, sdPlayPause, sdLeave, this);
    sdMenu->setEnterCallback(sdEnter);

    return sdMenu;
}

char* SDPlayer::pathOfAlbumAtIndex(int8_t albumIndex) {
    File dir = SD.open("/");
    int8_t index = 0;
    char *result = NULL;

    File file;
    bool finished = false;
    do {
        file = dir.openNextFile("r");
        if (!file) {
            finished = true;
            continue;
        }
        if (file.isDirectory()) {
            const char *filename = file.name();

            if (filename[0] == '.') continue;
            if (strcasecmp("system", filename) == 0) continue;
            if (strcasecmp("webradio", filename) == 0) continue;
            if (index == albumIndex) {
                Serial.printf("Found directory: %s at index %d\n", filename, index);
                result = strdup(file.path());
                finished = true;
            } else {
                index++;
            }
        }
        file.close();
    } while (!finished);
    dir.close();

    return result;
}

char* SDPlayer::nameOfTrackAtIndex(char *path, int16_t trackIndex) {
    File dir = SD.open(path);
    int16_t index = 0;
    char *result = NULL;

    File file;
    bool finished = false;
    do {
        file = dir.openNextFile("r");
        if (!file) {
            finished = true;
            continue;
        }
        if (!file.isDirectory()) {
            const char *filename = file.name();
            int len = strlen(filename);

            if (filename[0] == '.') continue;
            if (strcasecmp("album.mp3",filename) == 0) continue;
            if (strcasecmp(".jpg", filename + len - 4) == 0) continue;
            if (strcasecmp(".jpeg", filename + len - 5) == 0) continue;
            if (strcasecmp(".png", filename + len - 4) == 0) continue;
            if (strcasecmp(".nfo", filename + len - 4) == 0) continue;
            if (strcasecmp(".m3u", filename + len - 4) == 0) continue;
            if (strcasecmp(".m3u8", filename + len - 5) == 0) continue;

            if (index == trackIndex) {
                Serial.printf("Found file: %s at index %d\n", filename, index);
                result = strdup(filename);
                finished = true;
            } else {
                index++;
            }
        }
        file.close();
    } while (!finished);
    dir.close();

    return result;
}

int16_t SDPlayer::indexForFilenameAtPath(char *searchFilename, char *path) {
    File dir = SD.open(path);
    File file;
    bool finished = false;
    int16_t index = -1;
    do {
        file = dir.openNextFile("r");
        if (!file) {
            finished = true;
            index = -1;
            continue;
        }
        if (!file.isDirectory()) {
            const char *filename = file.name();
            int len = strlen(filename);

            if (filename[0] == '.') continue;
            if (strcasecmp("album.mp3",filename) == 0) continue;
            if (strcasecmp(".jpg", filename + len - 4) == 0) continue;
            if (strcasecmp(".jpeg", filename + len - 5) == 0) continue;
            if (strcasecmp(".png", filename + len - 4) == 0) continue;
            if (strcasecmp(".nfo", filename + len - 4) == 0) continue;
            if (strcasecmp(".m3u", filename + len - 4) == 0) continue;
            if (strcasecmp(".m3u8", filename + len - 5) == 0) continue;

            if (strcasecmp(searchFilename, filename) == 0) {
                Serial.printf("Found file: %s at index %d\n", filename, index);
                finished = true;
            } else {
                index++;
            }
        }
        file.close();
    } while (!finished);
    dir.close();

    return index;
}

char* SDPlayer::switchAlbum(int8_t albumIndex) {
    char *path = this->pathOfAlbumAtIndex(albumIndex);
    if (!path) return NULL;

    this->currentAlbum = albumIndex;
    this->maxTrack = 0;

    // check for playlist file
    char buffer[256];
    size_t bytes;
    bool playlist = true;
    snprintf(buffer, 256, "%s/album.m3u", path);
    if (!SD.exists(buffer)) {
        snprintf(buffer, 256, "%s/album.m3u8", path);
        if (!SD.exists(buffer)) {
            playlist = false;
        }
    }
    if (playlist) {
        File file = SD.open(buffer);
        while (file.available()) {
            bytes = file.readBytesUntil('\n', buffer, 256);
            while (bytes == 256) {
                bytes = file.readBytesUntil('\n', buffer, 256);
            }
            if (buffer[0] != '#') {
                // found a filename, determine our index for it
                int16_t fileIndex = this->indexForFilenameAtPath(buffer, path);
                if (fileIndex > 0) {
                    this->shuffle[this->maxTrack] = fileIndex;
                    this->maxTrack++;
                }
            }
        }
        file.close();
    } else {
        // calculate maxTrack
        File dir = SD.open(path);
        File file;
        bool finished = false;
        do {
            file = dir.openNextFile("r");
            if (!file) {
                finished = true;
                continue;
            }
            if (!file.isDirectory()) {
                const char *filename = file.name();
                int len = strlen(filename);

                if (filename[0] == '.') continue;
                if (strcasecmp("album.mp3",filename) == 0) continue;
                if (strcasecmp(".jpg", filename + len - 4) == 0) continue;
                if (strcasecmp(".jpeg", filename + len - 5) == 0) continue;
                if (strcasecmp(".png", filename + len - 4) == 0) continue;
                if (strcasecmp(".nfo", filename + len - 4) == 0) continue;
                if (strcasecmp(".m3u", filename + len - 4) == 0) continue;
                if (strcasecmp(".m3u8", filename + len - 5) == 0) continue;

                Serial.printf("%d, Found file: %s\n", this->maxTrack, filename);
                this->shuffle[maxTrack] = maxTrack;
                this->maxTrack++;
            }
            file.close();
        } while (!finished);
        dir.close();

        // TODO: shuffle
    }

    // debug output
    Serial.printf("Number of tracks = %d\n", this->maxTrack);
    for(int16_t i = 0; i < this->maxTrack; i++) {
        Serial.printf(" - Track %d at [%d]\n", this->shuffle[i], i);
    }
    return path;
}

void SDPlayer::play(int8_t albumIndex, int16_t trackIndex, bool reset) {
    if ((albumIndex < 0) || (albumIndex >= this->maxAlbum)) return;

    Serial.printf("Play in state %d, album: %d, track: %d/%d\n", this->state, this->currentAlbum, this->currentTrack, this->maxTrack);

    char *path;
    if (albumIndex != this->currentAlbum) {
        // switch album
        Serial.printf("Switching album to %d: %s\n", albumIndex, path);
        path = this->switchAlbum(albumIndex);
        this->currentTrack = trackIndex >= 0 ? trackIndex : 0;
    } else {
        path = this->pathOfAlbumAtIndex(albumIndex);
    }

    if (!path) return;

    if ((trackIndex < 0) || (trackIndex >= this->maxTrack)) {
        return;
    }

    // play track from this album
    char *filename = this->nameOfTrackAtIndex(path, shuffle[trackIndex]);
    if (filename) {
        Serial.printf("Play track index %d: %s\n", trackIndex, filename);
        this->currentTrack = trackIndex;
    }

    // Stop playback
    if (reset) {
        if (this->playlist->getState() != PlaybackStateStopped) {
            this->playlist->stopAndClear();
        }
    }

    if (filename) {
        // Add file to playlist and start playback
        char fullPath[256];
        snprintf(fullPath, 256, "%s/%s", path, filename);
        free(filename);
        this->playlist->registerPlaylistEndCallback(sdPlaylistEnd, reinterpret_cast<void *>(this));
        this->playlist->addFilename(fullPath);
        this->playlist->play();

        this->state = SDStateAlbumPlayback;
    }

    if (path) {
        free(path);
    }
}


bool SDPlayer::previous(bool announce, bool loop) {
    if (this->playlist->getState() == PlaybackStatePaused) {
        this->playlist->stopAndClear();
    }

    Serial.printf("Prev in state %d, album: %d, track: %d\n", this->state, this->currentAlbum, this->currentTrack);

    if (this->state == SDStateAlbumMenu) {
        this->currentAlbum--;
        if (this->currentAlbum < 0) {
            if (loop) {
                this->currentAlbum = this->maxAlbum - 1;
            } else {
                this->currentAlbum = 0;
            }
        }
        this->playlist->stopAndClear();
        this->announce(this->currentAlbum, -1);
    } else {
        this->currentTrack--;
        if (this->currentTrack < 0) {
            if (loop) {
                this->currentTrack = this->maxTrack - 1;
            } else {
                this->currentTrack = 0;
                return false;
            }
        }
        if (announce) {
            this->announce(this->currentAlbum, this->currentTrack);
        }
        this->play(this->currentAlbum, this->currentTrack, false);
    }
    return true;
}

bool SDPlayer::next(bool announce, bool loop) {
    if (this->playlist->getState() == PlaybackStatePaused) {
        this->playlist->stopAndClear();
    }
    Serial.printf("Next in state %d, album: %d, track: %d\n", this->state, this->currentAlbum, this->currentTrack);

    if (this->state == SDStateAlbumMenu) {
        this->currentAlbum++;
        if (this->currentAlbum >= this->maxAlbum) {
            if (loop) {
                this->currentAlbum = 0;
            } else {
                this->currentAlbum = this->maxAlbum - 1;
            }
        }
        this->announce(this->currentAlbum, -1);
    } else {
        this->currentTrack++;
        if (loop) {
            this->currentTrack = 0;
        } else {
            this->currentTrack = this->maxTrack - 1;
            return false;
        }

        if (announce) {
            this->announce(this->currentAlbum, this->currentTrack);
        }
        this->play(this->currentAlbum, this->currentTrack, false);
    }

    return true;
}

void SDPlayer::pause() {
    if (this->state == SDStateAlbumMenu) {
        this->currentTrack = 0;
        this->state = SDStateAlbumPlayback;
        this->playlist->stopAndClear();
        this->announce(this->currentAlbum, this->currentTrack);
        this->play(this->currentAlbum, this->currentTrack, false);
    } else {
        if ((this->playlist->getState() == PlaybackStatePlaying) || (this->playlist->getState() == PlaybackStatePaused)) {
            this->playlist->pause();
        }
    }
}

void SDPlayer::announce(int8_t albumIndex, int16_t trackIndex) {
    char buffer[256] = { 0 };

    this->playlist->stopAndClear();
    if (trackIndex < 0) {
        char *path = this->pathOfAlbumAtIndex(albumIndex);
        snprintf(buffer, 256, "%s/album.mp3", path);
        free(path);
        if (!SD.exists(buffer)) {
            // FIXME: number generator
            snprintf(buffer, 128, "/system/%d.mp3", albumIndex + 1);
            this->playlist->addFilename("/system/album.mp3");
            Serial.printf("Album %d does not have an announcer, using %s\n", albumIndex, buffer);
        } else {
            Serial.printf("Album %d has an announcer, using %s\n", albumIndex, buffer);
        }
        this->playlist->addFilename(buffer);
    } else {
        // FIXME: number generator
        snprintf(buffer, 128, "/system/%d.mp3", trackIndex + 1);
        this->playlist->addFilename("/system/track.mp3");
        this->playlist->addFilename(buffer);        
    }
    this->playlist->play();
}

void SDPlayer::reset() {
    this->currentAlbum = 0;
    this->currentTrack = 0;
    this->announce(this->currentAlbum, -1);
}

SDState SDPlayer::getState() {
    return this->state;
}

void SDPlayer::setState(SDState newState) {
    if ((this->state == SDStateAlbumPlayback) && (newState == SDStateAlbumMenu)) {
        this->announce(this->currentAlbum, -1);
    }
    Serial.printf("State is now %d\n", newState);
    this->state = newState;
}

void sdPrev(Menu *menu) {
    SDPlayer *player = reinterpret_cast<SDPlayer *>(menu->getContext());
    player->previous();
}

void sdNext(Menu *menu) {
    SDPlayer *player = reinterpret_cast<SDPlayer *>(menu->getContext());
    player->next();
}

void sdPlayPause(Menu *menu) {
    SDPlayer *player = reinterpret_cast<SDPlayer *>(menu->getContext());
    player->pause();
}

bool sdLeave(Menu *menu) {
    SDPlayer *player = reinterpret_cast<SDPlayer *>(menu->getContext());
    Serial.printf("Leave command in state %d\n", player->getState());
    if (player->getState() == SDStateAlbumMenu) {
        return true;
    } else {
        player->setState(SDStateAlbumMenu);
        return false;
    }
}

void sdEnter(Menu *menu) {
    SDPlayer *player = reinterpret_cast<SDPlayer *>(menu->getContext());
    player->reset();
}

void sdPlaylistEnd(void *context) {
    SDPlayer *player = reinterpret_cast<SDPlayer *>(context);
    bool success = player->next(false, false);
    if (!success) {
        player->playlist->addFilename("/system/stopped.mp3");
        player->playlist->play();
    }
}