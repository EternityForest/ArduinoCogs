#pragma once

//esp8266audio_vendored/
#include "AudioOutput.h"
#include "AudioGenerator.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include "AudioOutputMixer.h"
#include "AudioOutputBuffer.h"
#include "AudioFileSource.h"
#include "AudioFileSourceLittleFS.h"
#include "AudioFileSourceID3.h"

namespace cogs_sound{
    // Make it so user rules can play sounds
    void begin(AudioOutput * o);
}