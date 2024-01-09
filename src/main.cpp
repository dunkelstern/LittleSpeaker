#include <Arduino.h>
#include <WiFi.h>

/*
 * PIN    GPIO    Function
 *
 * D3     26      I2S L/R Clock
 * D6     14      I2S Data
 * D7     13      I2S Base Clock
 * A0     36      Button Yellow with PullUp
 * A1     39      Button Black with PullUp
 * A2     34      Button Blue with PullUp
 * D10    17      Encoder A with PullUp
 * D11    16      Encoder B with PullUp
 * SDA    21      Encoder Button with PullUp
 * SCK    18      SD-Card SPI Clock
 * MOSI   23      SD-Card SPI MOSI
 * MISO   19      SD-Card SPI MISO
 * SCL    22      SD-Card Chip Select
 */

#define ENCODER_PIN1 17
#define ENCODER_PIN2 16

#define ENCODER_BTN 21
#define YELLOW_BTN 36
#define BLACK_BTN 39
#define BLUE_BTN 34

//
// AUDIO
//
#include "AudioOutputI2S.h"
// You may need a fast SD card. Set this as high as it will work (40MHz max).
#define SPI_SPEED 4000000u
AudioOutputI2S *output = NULL;

// 
// ENCODER
//
#include "RotaryEncoder.h"

RotaryEncoder encoder(ENCODER_PIN1, ENCODER_PIN2, RotaryEncoder::LatchMode::FOUR3);

//
// BUTTONS
//
#include <AceButton.h>
using namespace ace_button;

AceButton encoderButton(ENCODER_BTN);
AceButton yellowButton(YELLOW_BTN);
AceButton blackButton(BLACK_BTN);
AceButton blueButton(BLUE_BTN);

static void handleEvent(AceButton*, uint8_t, uint8_t);

//
// MENU
//
#include "menu.h"

static void debugMenu(const char *text);
static void announceMenu(const char *filename);

Menu *mainMenu = NULL;

//
// AUDIO PLAYBACK
//
#include "playlist.h"

Playlist *playlist = NULL;

//
// BLUETOOTH
//
#include "bluetooth.h"

BluetoothPlayer *btPlayer;

//
// WEBRADIO
//
#include "webradio.h"

WebradioPlayer *webPlayer;

//
// SDCARD
//
#include "sdcard.h"

SDPlayer *sdPlayer;


#include "esp_heap_caps.h"
void heap_caps_alloc_failed_hook(size_t requested_size, uint32_t caps, const char *function_name) {
  printf("%s called, failed to allocate %d bytes with 0x%X capabilities.\n", function_name, requested_size, caps);
  printf("heap free: %d, max block: %d\n", 
    heap_caps_get_free_size(caps),
    heap_caps_get_largest_free_block(caps)
  );
}


void setup() {
  // Serial
  Serial.begin(115200);
  Serial.println();
  esp_err_t error = heap_caps_register_failed_alloc_callback(heap_caps_alloc_failed_hook);

  // Turn off everything
  btStop();
  WiFi.mode(WIFI_MODE_NULL);

  // Buttons, etc.
  encoder.setPosition(0);
  
  pinMode(ENCODER_BTN, INPUT);
  pinMode(YELLOW_BTN, INPUT);
  pinMode(BLACK_BTN, INPUT);
  pinMode(BLUE_BTN, INPUT);

  ButtonConfig* buttonConfig = ButtonConfig::getSystemButtonConfig();
  buttonConfig->setEventHandler(handleEvent);
  buttonConfig->setFeature(ButtonConfig::kFeatureClick);
  buttonConfig->setClickDelay(500);
  buttonConfig->setLongPressDelay(1000);

  // Audio Stuff
  audioLogger = &Serial;  

  output = new AudioOutputI2S(0, AudioOutputI2S::EXTERNAL_I2S, 12, AudioOutputI2S::APLL_ENABLE);
  output->SetPinout(13, 26, 14);
  output->SetGain(0.125);

  // SD-Card access
  if (!SD.begin(22, SPI, SPI_SPEED, "/sd", 5, false)) {
    Serial.println("SD Card could not be initialized!");
  }

  // Audio player
  playlist = new Playlist(output, 5);
  btPlayer = new BluetoothPlayer(playlist);
  webPlayer = new WebradioPlayer(playlist);
  sdPlayer = new SDPlayer(playlist);

  // Menu definition

  MenuItem *mainMenuItems[] = {
    new MenuItem("sd", "/system/sd.mp3", sdPlayer->makeMenu()),
    new MenuItem("radio", "/system/webradio.mp3", webPlayer->makeMenu()),
    new MenuItem("bluetooth", "/system/bluetooth.mp3", btPlayer->makeMenu()),
    NULL
  };
  mainMenu = new Menu(mainMenuItems);
  mainMenu->setDisplayUpdateCallback(debugMenu);
  mainMenu->setAudioAnnounceCallback(announceMenu);

  playlist->addFilename("/system/hello.mp3");
  playlist->addFilename("/system/sd.mp3");
  playlist->play();
}


// Basically Input handling
void loop() {
  encoder.tick();
  encoderButton.check();
  yellowButton.check();
  blackButton.check();
  blueButton.check();
 
  if (encoder.getPosition() != 0) {
    if (encoder.getPosition() < 0) {
      mainMenu->selectPreviousItem();
    } else {
      mainMenu->selectNextItem();
    }
    encoder.setPosition(0);
  }

  playlist->loop();
}


//
// Menu UI
//

static void debugMenu(const char *text) {
    Serial.printf_P(PSTR("Menu item '%s' selected...\n"), text);
}

static void announceMenu(const char *filename) {
    playlist->stopAndClear();
    playlist->addFilename(filename);
    playlist->play();
}

static void handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) {
  switch (eventType) {
    case AceButton::kEventClicked:
      if (button->getPin() == ENCODER_BTN) {
        Serial.println("Encoder button pressed");
        mainMenu->leaveItem();
      }

      if (button->getPin() == YELLOW_BTN) {
        Serial.println("Yellow button pressed");
        mainMenu->selectPreviousItem();
      }

      if (button->getPin() == BLACK_BTN) {
        Serial.println("Black button pressed");
        mainMenu->enterItem();
      }

      if (button->getPin() == BLUE_BTN) {
        Serial.println("Blue button pressed");
        mainMenu->selectNextItem();
      }
      break;
  }
}
