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

const char *ssid = "Kampftoast";
const char *password = "dunkelstern738";

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

void handleEvent(AceButton*, uint8_t, uint8_t);

//
// MENU
//
#include "menu.h"

void activateBluetooth(Menu *item);
void activateWifi(Menu *item);
void deactivateRadios(Menu *item);

void debugMenu(const char *text);
void announceMenu(const char *filename);

Menu *mainMenu = NULL;
Menu *albumMenu = NULL;
Menu *webradioMenu = NULL;
ButtonMenu *bluetoothMenu = NULL;

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

//
// SDCARD
//
#include "sdcard.h"


void setup() {
  // Serial
  Serial.begin(115200);
  Serial.println();

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

  output = new AudioOutputI2S(0, AudioOutputI2S::EXTERNAL_I2S, 16, AudioOutputI2S::APLL_ENABLE);
  output->SetPinout(13, 26, 14);
  output->SetGain(0.125);

  // SD-Card access
  if (!SD.begin(22, SPI, SPI_SPEED, "/sd", 10, false)) {
    Serial.println("SD Card could not be initialized!");
  }

  // Audio player
  playlist = new Playlist(output, 10);
  btPlayer = new BluetoothPlayer(playlist);

  // Menu definition

  // TODO: album menu is a dynamic menu
  // TODO: track menu is a dynamic menu as a submenu of album
  // TODO: webradio is a dynamic menu
  MenuItem *mainMenuItems[] = {
    new MenuItem("sd", "/system/sd.mp3", albumMenu, NULL),
    new MenuItem("radio", "/system/webradio.mp3", webradioMenu, NULL),
    new MenuItem("bluetooth", "/system/bluetooth.mp3", btPlayer->makeMenu(), NULL),
    NULL
  };
  mainMenu = new Menu(mainMenuItems);
  mainMenu->setDisplayUpdateCallback(debugMenu);
  mainMenu->setAudioAnnounceCallback(announceMenu);
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
}


//
// Webradio specific functionality
//

void activateWifi(Menu *item) {
    // TODO: read wifi config from SD
    // activate wifi
    Serial.println("Enabling Wifi");
    WiFi.disconnect();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        Serial.printf_P(PSTR("...Connecting to WiFi\n"));
        delay(1000);
    }
}

//
// Menu UI
//

void debugMenu(const char *text) {
    Serial.printf_P(PSTR("Menu item '%s' selected...\n"), text);
}

void announceMenu(const char *filename) {
    playlist->stopAndClear();
    playlist->addFilename(filename);
    playlist->play();
}

void handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) {
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

// void playbackAlbumTitle(int album) {
//   stopPlayback();

//   String path("/");
//   if (album < 10) {
//     path.concat("0");
//   }
//   path.concat(album);
//   path.concat("/album.mp3");

//   if (!SD.exists(path.c_str())) {
//     path.clear();
//     path.concat("/system/");
//     path.concat(album);
//     path.concat(".mp3");
//   }

//   Serial.println("Playing back album title...");

//   if (xSemaphoreTake(sourceMutex, 100) == pdTRUE) {
//     resetSource(path.c_str());
//     xSemaphoreGive(sourceMutex);

//     Serial.println(" - Source changed");

//     if (xSemaphoreTake(mp3DecoderMutex, 100) == pdTRUE) {
//       if (mp3Decoder) {
//         mp3Decoder->stop();
//         delete mp3Decoder;
//         mp3Decoder = NULL;
//       }
//       mp3Decoder = new AudioGeneratorMP3();
//       mp3Decoder->begin(source, output);
//       xSemaphoreGive(mp3DecoderMutex);
//       Serial.println(" - Playback started");
//     }
//   }
// }

// void playbackWebradioTitle(int radio) {
//   stopPlayback();

//   String path("/webradio/");
//   if (radio < 10) {
//     path.concat("0");
//   }
//   path.concat(radio);
//   path.concat(".mp3");

//   if (!SD.exists(path.c_str())) {
//     path.clear();
//     path.concat("/system/");
//     path.concat(radio);
//     path.concat(".mp3");
//   }

//   Serial.println("Playing back webradio title...");

//   if (xSemaphoreTake(sourceMutex, 100) == pdTRUE) {
//     resetSource(path.c_str());
//     xSemaphoreGive(sourceMutex);

//     Serial.println(" - Source changed");

//     if (xSemaphoreTake(mp3DecoderMutex, 100) == pdTRUE) {
//       if (mp3Decoder) {
//         mp3Decoder->stop();
//         delete mp3Decoder;
//         mp3Decoder = NULL;
//       }
//       mp3Decoder = new AudioGeneratorMP3();
//       mp3Decoder->begin(source, output);
//       xSemaphoreGive(mp3DecoderMutex);
//       Serial.println(" - Playback started");
//     }
//   }
// }

// File playAlbum(int album) {
//   String path("/");
//   if (album < 10) {
//     path.concat("0");
//   }
//   path.concat(album);

//   Serial.printf_P(PSTR("Opening '%s'...\n"), path.c_str());

//   return SD.open(path.c_str()); 
// }

// void playTrack(int trackNo) {
//   File file;

//   Serial.print("Playing track No ");
//   Serial.println(trackNo);
  
//   File dir = playAlbum(currentItem);
//   for (int i = 0; i < trackNo + 1; i++) {
//     file = dir.openNextFile();
//     if (!String(file.name()).endsWith(".mp3")) {
//       // TODO: Add other formats
//       trackNo++;
//       continue;
//     }
//     if (!file) break;
//   }

//   if (file) {      
//     Serial.printf_P(PSTR("Trying '%s' from SD card...\n"), file.path());
//     stopPlayback();

//     if (xSemaphoreTake(sourceMutex, 100) == pdTRUE) {
//       resetSource(file.path());
  
//       if (!source->isOpen()) { 
//         Serial.printf_P(PSTR("Error opening '%s'\n"), file.path());
//         return;
//       }
//       xSemaphoreGive(sourceMutex);

//       // MP3 files
//       if (String(file.name()).endsWith(".mp3")) {
//         if (xSemaphoreTake(mp3DecoderMutex, 100) == pdTRUE) {
//           if (mp3Decoder) {
//             mp3Decoder->stop();
//             delete mp3Decoder;
//             mp3Decoder = NULL;
//           }
//           mp3Decoder = new AudioGeneratorMP3();
//           mp3Decoder->begin(source, output);
//           xSemaphoreGive(mp3DecoderMutex);
//           Serial.printf_P(PSTR("Playing '%s' from SD card...\n"), file.path());
//         }
//       }
//       // TODO: What to do with non-mp3 files
//     }
//   } else {
//     Serial.println(F("Playback from SD card done\n"));
    
//     // Got to next album
//     switchAlbum(1, 0);
//   }
// }

