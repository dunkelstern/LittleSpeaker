#include "playlist.h"
#include <SD.h>

#include "AudioFileSourceICYStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioFileSourceID3.h"
#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
//#include "AudioGeneratorMP3a.h"
#include "AudioGeneratorAAC.h"

const int maxFilenameLength = 64;
const int preallocateBufferSize = 16*1024;
const int preallocateCodecSize = 85332; // AAC+SBR codec max mem needed

void playbackTaskLoop(void *parameter);
void metadataCallback(void *cbData, const char *type, bool isUnicode, const char *string);
void statusCallback(void *cbData, int code, const char *string);

//
// Playlist implementation
//

Playlist::Playlist(AudioOutput *output, int maxEntries) {
    this->output = output;
    this->ringbufferSize = maxEntries;
    this->itemRingbuffer = (char **)malloc(sizeof(char *) * maxEntries);
    for(int i = 0; i < this->ringbufferSize; i++) {
        this->itemRingbuffer[i] = (char *)malloc(sizeof(char) * (maxFilenameLength + 1));
    }
    this->ringBufferMutex = xSemaphoreCreateBinary();
    xSemaphoreGive(this->ringBufferMutex);

    this->readMarker = -1;
    this->writeMarker = 0;
    this->state = PlaybackStateStopped;

    this->preallocateBuffer = NULL;
    this->preallocateCodec = NULL;
}

Playlist::~Playlist() {
    for(int i = 0; i < this->ringbufferSize; i++) {
        free(this->itemRingbuffer[i]);
    }
    free(this->itemRingbuffer);
    free(this->preallocateBuffer);
    free(this->preallocateCodec);
}

void Playlist::freeAllBuffers() {
    free(this->preallocateBuffer);
    free(this->preallocateCodec);
    this->preallocateBuffer = NULL;
    this->preallocateCodec = NULL;
}

bool Playlist::addFilename(const char *filename) {
    if (strlen(filename) > maxFilenameLength) {
        // Name too long
        Serial.println("Filename too long");
        return false;
    }

    if (xSemaphoreTake(this->ringBufferMutex, 20) == pdFALSE) {
        // Could not acquire mutex
        Serial.println("Could not acquire mutex");
        return false;
    }

    if (this->writeMarker == this->readMarker) {
        // Buffer full
        Serial.println("Buffer full");
        xSemaphoreGive(this->ringBufferMutex);
        return false;
    }
    
    strcpy(this->itemRingbuffer[writeMarker], filename);
    writeMarker++;
    if (writeMarker >= this->ringbufferSize) {
        writeMarker = 0;
    }

    // Buffer has an entry, set read marker to start of buffer
    if (readMarker < 0) {
        readMarker = 0;
    }

    xSemaphoreGive(this->ringBufferMutex);
    return true;
}

char* Playlist::consumeItem() {
    if (xSemaphoreTake(this->ringBufferMutex, 20) == pdFALSE) {
        // Could not acquire mutex
        return NULL;
    }

    if (this->readMarker == this->writeMarker) {
        // Buffer empty
        xSemaphoreGive(this->ringBufferMutex);
        return NULL;
    }

    char *item = strdup(this->itemRingbuffer[this->readMarker]);
    this->readMarker++;
    if (this->readMarker >= this->ringbufferSize) {
        this->readMarker = 0;
    }

    xSemaphoreGive(this->ringBufferMutex);

    Serial.printf_P(PSTR("Consume '%s'\n"), item);
    return item;
}

PlaybackState Playlist::getState() {
    return this->state;
}

void Playlist::play() {
    Serial.println("Play");
    if (this->state == PlaybackStateSkipping) {
        this->state = PlaybackStatePlaying;
        return;
    }
    if (this->state == PlaybackStatePlaying) {
        return;
    }
    this->state = PlaybackStatePlaying;
    xTaskCreate(playbackTaskLoop, "playback", 5000, this, 12, &this->playbackTask);
}

void Playlist::pause() {
    Serial.println("Pause");
    if (this->state == PlaybackStatePlaying) {
        this->state = PlaybackStatePaused;
    }
    if (this->state == PlaybackStatePaused) {
        this->state = PlaybackStatePlaying;
    }
}

void Playlist::skip() {
    Serial.println("Skip");
    if ((this->state == PlaybackStatePlaying) || (this->state = PlaybackStatePaused)) {
        this->state = PlaybackStateSkipping;
    }
}

void Playlist::stopAndClear() {
    Serial.println("Stop and Clear");
    if (xSemaphoreTake(this->ringBufferMutex, 20) == pdFALSE) {
        // Could not acquire mutex
        return;
    }

    this->readMarker = -1;
    this->writeMarker = 0;
    this->state = PlaybackStateStopped;

    xSemaphoreGive(this->ringBufferMutex);
    vTaskDelay(100);
}

AudioFileSource* Playlist::getAudioFileSourceForFile(const char *filename) {    
    if (strcasecmp(".mp3", filename + strlen(filename) - 4) == 0) {
        // MP3 file, get an ID3 source
        Serial.println("MP3");
        AudioFileSourceSD *sdSource = new AudioFileSourceSD(filename);
        AudioFileSourceID3 *id3Source = new AudioFileSourceID3(sdSource);
        // id3Source->RegisterMetadataCB(metadataCallback, (void*)"ID3TAG");

        Serial.printf_P(PSTR("File '%s' is MP3, source created\n"), filename);
        return id3Source;
    }
    if (strncmp("http://", filename, 7) == 0) {
        // Webradio link
        if (!this->preallocateBuffer) {
            this->preallocateBuffer = reinterpret_cast<char *>(malloc(preallocateBufferSize));
        }
        AudioFileSourceICYStream *source = new AudioFileSourceICYStream(filename);
        source->RegisterMetadataCB(metadataCallback, NULL);
        AudioFileSourceBuffer *buff = new AudioFileSourceBuffer(source, this->preallocateBuffer, preallocateBufferSize);
        buff->RegisterStatusCB(statusCallback, NULL);

        Serial.printf_P(PSTR("URL '%s' is Webradio, source created\n"), filename);
        return buff;
    }
    return NULL;
}

AudioGenerator* Playlist::getDecoderForFile(const char *filename) {
    if (strcasecmp(".mp3", filename + strlen(filename) - 4) == 0) {
        if (!this->preallocateCodec) {
            this->preallocateCodec = reinterpret_cast<char *>(malloc(preallocateCodecSize));
        }
        // MP3 file, get the MP3 decoder ready
        AudioGeneratorMP3 *mp3Decoder = new AudioGeneratorMP3(this->preallocateCodec, preallocateCodecSize);
        mp3Decoder->RegisterStatusCB(statusCallback, NULL);
        Serial.printf_P(PSTR("File '%s' is MP3, decoder created\n"), filename);
        return mp3Decoder;
    }
    if (strncmp("http://", filename, 7) == 0) {
        if (!this->preallocateCodec) {
            this->preallocateCodec = reinterpret_cast<char *>(malloc(preallocateCodecSize));
        }
        // Webradio link
        bool aac = (strcasecmp("type=aac", filename + strlen(filename) - 8) == 0);
        AudioGenerator *decoder = NULL;
        if (aac) {
            decoder = new AudioGeneratorAAC(preallocateCodec, preallocateCodecSize);
            Serial.printf_P(PSTR("URL '%s' is AAC Webradio, decoder created\n"), filename);
        } else {
            decoder = new AudioGeneratorMP3(preallocateCodec, preallocateCodecSize);
            Serial.printf_P(PSTR("URL '%s' is MP3 Webradio, decoder created\n"), filename);
        }
        decoder->RegisterStatusCB(statusCallback, NULL);
        return decoder;
    }
    return NULL;
}

AudioOutput* Playlist::getOutput() {
    return this->output;
}


//
// Audio player implementation
//


void metadataCallback(void *cbData, const char *type, bool isUnicode, const char *string) {
    (void)cbData;
    Serial.printf("metadata for: %s = '", type);

    if (isUnicode) {
        string += 2;
    }

    while (*string) {
        char a = *(string++);
        if (isUnicode) {
            string++;
        }
        Serial.printf("%c", a);
    }
    Serial.printf("'\n");
    Serial.flush();
}

void statusCallback(void *cbData, int code, const char *string) {
    const char *ptr = reinterpret_cast<const char *>(cbData);
    (void) ptr;
    Serial.printf("status: %d '%s'\n", code, string);
}

void playbackTaskLoop(void *parameter) {
    const TickType_t loopDelay = 4 / portTICK_PERIOD_MS;
    const TickType_t mutexDelay = 2 / portTICK_PERIOD_MS;
    Playlist *playlist = (Playlist *)parameter;

    Serial.println("Playback task started...");

    char *filename = playlist->consumeItem();
    if (filename == NULL) {
        return;
    }

    AudioFileSource *source = playlist->getAudioFileSourceForFile(filename);
    if (!source) {
        Serial.println("Could not create source, bailing out");
        Serial.println("Playback task finished");
        vTaskDelete(NULL);
        return;
    }
    AudioGenerator *decoder = playlist->getDecoderForFile(filename);
    decoder->begin(source, playlist->getOutput());

    while (decoder) {
        PlaybackState state = playlist->getState();

        if (state == PlaybackStateStopped) {
            Serial.println("Responding to playback stop...");

            if ((decoder) && (decoder->isRunning())) {
                decoder->stop();
            }
            delete decoder;
            decoder = NULL;

            source->close();
            delete source;
            source = NULL;

            free(filename);

            vTaskDelay(loopDelay);
            continue;
        }

        if (state == PlaybackStateSkipping) {
            Serial.println("Responding to skip...");

            if ((decoder) && (decoder->isRunning())) {
                decoder->stop();
            }
            delete decoder;
            decoder = NULL;

            source->close();
            delete source;
            source = NULL;

            free(filename);

            filename = playlist->consumeItem();
            if (filename) {
                source = playlist->getAudioFileSourceForFile(filename);
                if (!source) {
                    Serial.println("Could not create source, bailing out");
                    break;
                }
                decoder = playlist->getDecoderForFile(filename);
                decoder->begin(source, playlist->getOutput());
            } else {
                playlist->stopAndClear();
            }

            playlist->play();

            vTaskDelay(loopDelay);
            continue;
        }

        if (state == PlaybackStatePaused) {
            Serial.println("pause");
            vTaskDelay(loopDelay);
            continue;
        }

        if ((decoder) && (decoder->isRunning())) {
            if (!decoder->loop()) {
                decoder->stop();
                source->close();

                delete decoder;
                delete source;

                decoder = NULL;
                source = NULL;

                Serial.println("Item playback finished");

                // check if next playlist item is available
                free(filename);
                filename = playlist->consumeItem();
                if (filename) {
                    source = playlist->getAudioFileSourceForFile(filename);
                    if (!source) {
                        Serial.println("Could not create source, bailing out");
                        break;
                    }
                    decoder = playlist->getDecoderForFile(filename);
                    decoder->begin(source, playlist->getOutput());
                } else {
                    Serial.println("Playback finished");
                    playlist->stopAndClear();
                }
            }
        }

        vTaskDelay(loopDelay);
    }

    vTaskDelete(NULL);
    Serial.println("Playback task finished");
}
