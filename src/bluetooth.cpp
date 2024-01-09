#include "bluetooth.h"
#include <WiFi.h>

// FIXME: Pin config as template?

static void bluetoothPrev(Menu *item);
static void bluetoothNext(Menu *item);
static void bluetoothPlayPause(Menu *item);
static void activateBluetooth(Menu *item);
static void deactivateBluetooth(Menu *item);
static void runBluetooth(void *context);
static void bluetoothDevConnCallback(bool connected, void *context);
static void bluetoothAVRCCallback(esp_avrc_playback_stat_t state, void *context);


BluetoothPlayer::BluetoothPlayer(Playlist *playlist) {
    this->playlist = playlist;
    this->a2dp = NULL;
}
    
BluetoothPlayer::~BluetoothPlayer() {
    this->playlist = NULL;
}

Menu* BluetoothPlayer::makeMenu() {
    ButtonMenu *bluetoothMenu = new ButtonMenu(bluetoothPrev, bluetoothNext, bluetoothPlayPause, this);
    bluetoothMenu->setEnterCallback(activateBluetooth);
    bluetoothMenu->setLeaveCallback(deactivateBluetooth);

    return bluetoothMenu;
}

BluetoothA2DPSink* BluetoothPlayer::makeSink() {
    if (this->a2dp) return this->a2dp;

    this->a2dp = new BluetoothA2DPSink();
    i2s_pin_config_t cfg = {
      .mck_io_num = I2S_PIN_NO_CHANGE,
      .bck_io_num = 13,
      .ws_io_num = 26,
      .data_out_num = 14,
      .data_in_num = I2S_PIN_NO_CHANGE
    };
    Serial.println("Creating A2DP sink...");
    this->a2dp->set_pin_config(cfg);
    this->a2dp->set_mono_downmix(true);
    this->a2dp->set_volume(0x0f);
    this->a2dp->set_avrc_connection_state_callback(bluetoothDevConnCallback);
    this->a2dp->set_avrc_rn_playstatus_callback(bluetoothAVRCCallback);
    this->a2dp->set_callback_context(reinterpret_cast<void *>(this));
    this->a2dp->start("LittleBox");

    return this->a2dp;
}

void BluetoothPlayer::destroySink() {
    if (!this->a2dp) return;

    this->a2dp->stop();
    this->a2dp->end(true);
    vTaskDelay(20);
    delete this->a2dp;
    this->a2dp = NULL;
    vTaskDelay(100);
    ESP.restart();
}

void BluetoothPlayer::previous() {
    if (!this->a2dp) return;
    this->a2dp->previous();
}

void BluetoothPlayer::next() {
    if (!this->a2dp) return;
    this->a2dp->next();
}

void BluetoothPlayer::pause() {
    if (!this->a2dp) return;
    if (this->state == BTStatePlaying) {
        this->a2dp->pause();
    } else {
        this->a2dp->play();
    }
}

void BluetoothPlayer::setState(BTState newState) {
    this->state = newState;
}

static void bluetoothPrev(Menu *menu) {
    BluetoothPlayer *player = reinterpret_cast<BluetoothPlayer *>(menu->getContext());
    player->previous();
}

static void bluetoothNext(Menu *menu) {
    BluetoothPlayer *player = reinterpret_cast<BluetoothPlayer *>(menu->getContext());
    player->next();
}

static void bluetoothPlayPause(Menu *menu) {
    BluetoothPlayer *player = reinterpret_cast<BluetoothPlayer *>(menu->getContext());
    player->pause();
}

static void activateBluetooth(Menu *item) {
    BluetoothPlayer *player = reinterpret_cast<BluetoothPlayer *>(item->getContext());

    // switch to bluetooth mode
    Serial.println("Disabling Wifi");
    WiFi.disconnect();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_MODE_NULL);
    
    player->playlist->addFilename("/system/bluetooth_on.mp3");
    player->playlist->registerPlaylistEndCallback(runBluetooth, player);
    player->playlist->play();
}

static void deactivateBluetooth(Menu *item) {
    Serial.println("Disabling Bluetooth");
    BluetoothPlayer *player = reinterpret_cast<BluetoothPlayer *>(item->getContext());
    player->destroySink();
}

static void runBluetooth(void *context) {
    BluetoothPlayer *player = reinterpret_cast<BluetoothPlayer *>(context);

    Serial.println("Freeing buffers");
    player->playlist->stopAndClear();
    player->playlist->freeAllBuffers();

    Serial.println("A2DP enable");
    player->makeSink();
}

static void bluetoothReinitI2S(void *context) {
    BluetoothA2DPSink *a2dp = reinterpret_cast<BluetoothA2DPSink *>(context);
    a2dp->init_i2s();
    a2dp->set_i2s_active(true);
}

static void bluetoothDevConnCallback(bool connected, void *context) {
    BluetoothPlayer *player = reinterpret_cast<BluetoothPlayer *>(context);

    player->a2dp->set_i2s_active(false);
    if (connected) {
        player->playlist->addFilename("/system/bluetooth_pair.mp3");
        player->playlist->registerPlaylistEndCallback(bluetoothReinitI2S, player->a2dp);
        player->playlist->play();
    } else {
        player->playlist->addFilename("/system/stopped.mp3");
        player->playlist->play();
    }
}

static void bluetoothAVRCCallback(esp_avrc_playback_stat_t state, void *context) {
    BluetoothPlayer *player = reinterpret_cast<BluetoothPlayer *>(context);

    switch (state) {
        case ESP_AVRC_PLAYBACK_STOPPED:
            player->setState(BTStateStopped);
            break;
        case ESP_AVRC_PLAYBACK_PLAYING:
            player->setState(BTStatePlaying);
            break;
        case ESP_AVRC_PLAYBACK_PAUSED:
            player->setState(BTStatePaused);
            break;
        default:
            // ESP_AVRC_PLAYBACK_FWD_SEEK
            // ESP_AVRC_PLAYBACK_REV_SEEK
            // ESP_AVRC_PLAYBACK_ERROR
            break;
    }
}