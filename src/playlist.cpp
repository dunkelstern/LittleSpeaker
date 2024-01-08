#include "esp_heap_caps.h"
#include "playlist.h"
#include <SD.h>

#include "AudioFileSourceICYStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioFileSourceID3.h"
#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3a.h"

const int maxFilenameLength = 256;
const int preallocateBufferSize = 6*1024;

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
    this->base = NULL;
    this->source = NULL;
    this->decoder = NULL;
    this->endCallback = NULL;
    this->endContext = NULL;
}

Playlist::~Playlist() {
    for(int i = 0; i < this->ringbufferSize; i++) {
        free(this->itemRingbuffer[i]);
    }
    free(this->itemRingbuffer);
    if (this->preallocateBuffer) {
        free(this->preallocateBuffer);
    }
}

void Playlist::freeAllBuffers() {
    if (this->preallocateBuffer) {
        free(this->preallocateBuffer);
        this->preallocateBuffer = NULL;
    }
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

    if (this->writeMarker == this->readMarker - 1) {
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

    char *item = NULL;
    if (this->readMarker >= 0) {
        item = this->itemRingbuffer[this->readMarker];
        this->readMarker++;
        if (this->readMarker >= this->ringbufferSize) {
            this->readMarker = 0;
        }
    } else {
        xSemaphoreGive(this->ringBufferMutex);
        return NULL;
    }
    xSemaphoreGive(this->ringBufferMutex);

    Serial.printf_P(PSTR("Consume '%s'\n"), item);
    return item;
}

PlaybackState Playlist::getState() {
    return this->state;
}

void Playlist::play() {
    if (this->state == PlaybackStateReset) {
        // wait for reset to finish
        Serial.println("Waiting for reset...");
        return;
    }
    if (this->state == PlaybackStateSkipping) {
        Serial.println("Switching from skipping to playing...");
        this->state = PlaybackStatePlaying;
        return;
    }
    if (this->state == PlaybackStatePlaying) {
        Serial.println("Already playing...");
        return;
    }

    Serial.println("Play...");
    this->state = PlaybackStatePlaying;
}

void Playlist::pause() {
    if (this->state == PlaybackStatePlaying) {
        Serial.println("Pausing...");
        this->state = PlaybackStatePaused;
        return;
    }
    if (this->state == PlaybackStatePaused) {
        Serial.println("Restarting Playback...");
        this->state = PlaybackStatePlaying;
        return;
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
    this->state = PlaybackStateReset;
    this->endCallback = NULL;
    this->endContext = NULL;

    xSemaphoreGive(this->ringBufferMutex);
    vTaskDelay(100);
}

void Playlist::registerPlaylistEndCallback(void (*callback)(void *), void *context) {
    this->endCallback = callback;
    this->endContext = context;
}


bool Playlist::setupAudioSourceForFile(const char *filename) {
    if (strncmp("http://", filename, 7) == 0) {
        // Webradio station
        if (!this->preallocateBuffer) {
            this->preallocateBuffer = reinterpret_cast<char *>(malloc(preallocateBufferSize));
        }
        this->base = new AudioFileSourceICYStream(filename);
        this->base->RegisterMetadataCB(metadataCallback, NULL);
        if (this->base == NULL) return false;
        this->source = new AudioFileSourceBuffer(this->base, this->preallocateBuffer, preallocateBufferSize);
        this->source->RegisterStatusCB(statusCallback, NULL);
        if (this->source == NULL) {
            this->base->close();
            delete this->base;
            this->base = NULL;
            return false;
        }
        return true;
    }
    if (strcasecmp(".mp3", filename + strlen(filename) - 4) == 0) {
        // MP3 file, get an ID3 source
        if (this->preallocateBuffer) {
            free(this->preallocateBuffer);
            this->preallocateBuffer = NULL;
        }
        this->base = new AudioFileSourceSD(filename);
        if (this->base == NULL) return false;
        this->source = new AudioFileSourceID3(this->base);
        if (this->source == NULL) {
            this->base->close();
            delete this->base;
            this->base = NULL;
            return false;
        }
        this->source->RegisterMetadataCB(metadataCallback, (void*)"ID3TAG");

        Serial.printf_P(PSTR("File '%s' is MP3, source created\n"), filename);
        return true;
    }
    return false;
}

bool Playlist::setupDecoderForFile(const char *filename) {
    if ((strcasecmp(".mp3", filename + strlen(filename) - 4) == 0) || (strncmp("http://", filename, 7) == 0)) {
        this->decoder = new AudioGeneratorMP3a();
        if (this->decoder == NULL) {
            return false;
        }
        this->decoder->RegisterStatusCB(statusCallback, NULL);
        Serial.printf_P(PSTR("'%s' is MP3, decoder created\n"), filename);
        return true;
    }
    return false;
}

void Playlist::destroyAudioChain() {
    if (this->decoder) {
        if (this->decoder->isRunning()) {
            this->decoder->stop();
        }
        delete this->decoder;
        this->decoder = NULL;
    }
    if (this->source) {
        this->source->close();
        delete this->source;
        this->source = NULL;
    }
    if (this->base) {
        this->base->close();
        delete this->base;
        this->base = NULL;
    }
}

void Playlist::loop() {
    bool closeCurrentItem = false;

    if ((this->decoder == NULL) && (this->state != PlaybackStateStopped) && (this->state != PlaybackStateReset)) {
        char *filename = this->consumeItem();
        if (filename == NULL) {
            if (this->state != PlaybackStateStopped) {
                Serial.println("All items played!");
                this->state = PlaybackStateStopped;
                if (this->endCallback) {
                    void (*savedCallback)(void *context) = this->endCallback;
                    this->endCallback = NULL;
                    savedCallback(this->endContext);
                    this->endContext = NULL;
                }
            }
            return;
        }

        if (!this->setupAudioSourceForFile(filename)) {
            Serial.println("Could not create source, bailing out");
            this->destroyAudioChain();
            return;
        }
        
        if (!this->setupDecoderForFile(filename)) {
            Serial.println("Could not create decoder, bailing out");
            this->destroyAudioChain();
            return;
        }
        this->decoder->begin(this->source, this->output);
    }

    switch (this->state) {
        case PlaybackStateReset:
            if ((this->decoder) && (this->decoder->isRunning())) {
                Serial.println("Responding to playback reset...");
            }
            this->destroyAudioChain();
            this->state = PlaybackStatePlaying;
            break;
        case PlaybackStateStopped:
            if ((this->decoder) && (this->decoder->isRunning())) {
                Serial.println("Responding to playback stop...");
            }
            this->destroyAudioChain();
            break;
        case PlaybackStateSkipping:
            Serial.println("Responding to skip...");
            this->destroyAudioChain();
            this->state = PlaybackStatePlaying;
            break;
        case PlaybackStatePaused:
            return; // Do nothing
        case PlaybackStatePlaying:
            if (this->decoder) {
                if (this->decoder->isRunning()) {
                    if (!this->decoder->loop()) {
                        Serial.println("Playback finished.");
                        this->destroyAudioChain();
                    }
                } else {
                    Serial.println("Playback finished.");
                    this->destroyAudioChain();
                }
            }
            break;
    }
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
