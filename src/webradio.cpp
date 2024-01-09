#include "webradio.h"
#include <SD.h>
#include <WiFi.h>

static void activateWifi(Menu *menu);
static void deactivateWifi(Menu *menu);

static void webPrev(Menu *item);
static void webNext(Menu *item);
static void webPlayPause(Menu *item);

#define MAX_URL_LEN 255

WebradioPlayer::WebradioPlayer(Playlist *playlist) {
    this->playlist = playlist;
    this->numRadioStations = 0;
    this->currentItem = 0;

    // load playlist file
    File file = SD.open("/webradio.txt");
    if (!file) {
        Serial.println("No webradio.txt found");
    }

    // parse playlist file
    char buffer[MAX_URL_LEN + 1] = { 0 };
    while (file.available()) {
        size_t bytes;
        
        bytes = file.readBytesUntil(':', buffer, 6);
        if ((bytes != 4) || (strncmp("http", buffer, 4) != 0)) {
            // can not parse line, skip to end of line
            buffer[bytes] = '\0';
            Serial.printf("Line does not start with http: %s", buffer);
            goto skipline;
        } else {
            Serial.printf("Station %d -> http:", this->numRadioStations + 1);
        }

        this->urloffset[this->numRadioStations] = file.position() - 5;
        this->numRadioStations++;

        if (this->numRadioStations == MAX_WEBRADIO_STATIONS) {
            break;
        }
skipline:
        do {
            bytes = file.readBytesUntil('\n', buffer, MAX_URL_LEN);
            buffer[bytes] = '\0';
            Serial.printf("%s", buffer);
        } while (bytes == 128);
        Serial.println();
    }
    file.close();
}

WebradioPlayer::~WebradioPlayer() {
}

Menu* WebradioPlayer::makeMenu() {
    ButtonMenu *webradioMenu = new ButtonMenu(webPrev, webNext, webPlayPause, this);
    webradioMenu->setEnterCallback(activateWifi);
    webradioMenu->setLeaveCallback(deactivateWifi);

    return webradioMenu;
}

void WebradioPlayer::play(int index) {
    if ((index < 0) || (index >= this->numRadioStations)) return;

    this->currentItem = index;
    Serial.printf("Play index %d by index call\n", index);

    // Load URL from SD
    File file = SD.open("/webradio.txt");
    if (!file) {
        Serial.println("No webradio.txt found");
        return;
    }
    file.seek(this->urloffset[index]);
    char buffer[MAX_URL_LEN + 1] = { 0 };
    size_t bytes = file.readBytesUntil('\n', buffer, MAX_URL_LEN);
    buffer[bytes] = '\0';

    Serial.printf("URL: %s\n", buffer);

    // Stop playback
    if (this->playlist->getState() != PlaybackStateStopped) {
        this->playlist->stopAndClear();
    }

    // Add url to playlist and start playback
    this->playlist->addFilename(buffer);
    this->playlist->addFilename("/system/connection_failed.mp3");
    this->playlist->play();
}


void WebradioPlayer::previous() {
    if (this->playlist->getState() == PlaybackStatePlaying) {
        return;
    }
    if (this->playlist->getState() == PlaybackStatePaused) {
        this->playlist->stopAndClear();
    }
    
    this->currentItem--;
    if (this->currentItem < 0) {
        this->currentItem = this->numRadioStations - 1;
    }

    this->announce(this->currentItem);
}

void WebradioPlayer::next() {
    if (this->playlist->getState() == PlaybackStatePlaying) {
        return;
    }
    if (this->playlist->getState() == PlaybackStatePaused) {
        this->playlist->stopAndClear();
    }

    this->currentItem++;
    if (this->currentItem >= this->numRadioStations) {
        this->currentItem = 0;
    }
    this->announce(this->currentItem);
}

void WebradioPlayer::pause() {
    if ((this->playlist->getState() == PlaybackStatePlaying) || (this->playlist->getState() == PlaybackStatePaused)) {
        this->playlist->stopAndClear();
        this->playlist->addFilename("/system/stopped.mp3");
        this->playlist->play();
    } else {
        this->play(this->currentItem);
    }
}

void WebradioPlayer::announce(int index) {
    char buffer[129] = { 0 };

    if (this->currentItem < 10) {
        snprintf(buffer, 128, "/webradio/0%d.mp3", this->currentItem + 1);
    } else {
        snprintf(buffer, 128, "/webradio/%d.mp3", this->currentItem + 1);
    }
    if (!SD.exists(buffer)) {
        snprintf(buffer, 128, "/system/%d.mp3", this->currentItem + 1);
        Serial.printf("Station %d does not have an announcer, using %s\n", this->currentItem, buffer);
    } else {
        Serial.printf("Station %d has an announcer, using %s\n", this->currentItem, buffer);
    }
    this->playlist->stopAndClear();
    this->playlist->addFilename(buffer);
    this->playlist->play();
}

void WebradioPlayer::reset() {
    this->currentItem = 0;
    this->announce(this->currentItem);
}

static void activateWifi(Menu *menu) {
    WebradioPlayer *player = reinterpret_cast<WebradioPlayer *>(menu->getContext());
    char ssid[128] = { 0 };
    char password[128] = { 0 };

    File file = SD.open("/wifi.txt");
    if (!file) {
        Serial.println("No wifi.txt found");
        return;
    }

    // parse wifi file
    size_t bytes;
    bytes = file.readBytesUntil('\n', ssid, 127);
    bytes = file.readBytesUntil('\n', password, 127);
    file.close();

    // TODO: read wifi config from SD
    // activate wifi
    Serial.printf("Enabling Wifi, connecting to %s...", ssid);
    if (WiFi.getMode() != WIFI_MODE_NULL) {
        WiFi.disconnect();
        WiFi.softAPdisconnect(true);
    };
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        Serial.println(WiFi.status());
        Serial.println("Connecting...");
        delay(500);
    }

    IPAddress addr = WiFi.localIP();
    IPAddress gateway = WiFi.gatewayIP();
    Serial.printf("IP: %s, GW: %s\n", addr.toString().c_str(), gateway.toString().c_str());
    player->reset();
}

static void deactivateWifi(Menu *item) {
    Serial.println("Disabling Wifi");
    WiFi.disconnect();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_MODE_NULL);
}

static void webPrev(Menu *menu) {
    WebradioPlayer *player = reinterpret_cast<WebradioPlayer *>(menu->getContext());
    player->previous();
}

static void webNext(Menu *menu) {
    WebradioPlayer *player = reinterpret_cast<WebradioPlayer *>(menu->getContext());
    player->next();
}

static void webPlayPause(Menu *menu) {
    WebradioPlayer *player = reinterpret_cast<WebradioPlayer *>(menu->getContext());
    player->pause();
}
