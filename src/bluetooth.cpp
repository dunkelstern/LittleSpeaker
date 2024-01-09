#include "bluetooth.h"
#include <WiFi.h>

// FIXME: Pin config as template?

static void bluetoothPrev(Menu *item);
static void bluetoothNext(Menu *item);
static void bluetoothPlayPause(Menu *item);
static void activateBluetooth(Menu *item);
static void deactivateBluetooth(Menu *item);


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
    this->a2dp->set_pin_config(cfg);
    this->a2dp->set_mono_downmix(true);
    this->a2dp->set_volume(0x0f);
    this->a2dp->start("LittleBox");

    return this->a2dp;
}

void BluetoothPlayer::destroySink() {
    if (!this->a2dp) return;

    this->a2dp->stop();
    vTaskDelay(20);
    delete this->a2dp;
    this->a2dp = NULL;

    btStop();
    vTaskDelay(200);
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
    this->a2dp->pause();
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
    
    if (player->playlist) {
        Serial.println("Freeing buffers");
        player->playlist->stopAndClear();
        vTaskDelay(10);
        player->playlist->freeAllBuffers();
    }

    Serial.println("A2DP enable");
    player->makeSink();
}

static void deactivateBluetooth(Menu *item) {
    Serial.println("Disabling Bluetooth");
    BluetoothPlayer *player = reinterpret_cast<BluetoothPlayer *>(item->getContext());
    player->destroySink();
}
