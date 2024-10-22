#pragma once

#include "esp8266audio_vendored/AudioOutput.h"
#include "esp8266audio_vendored/AudioGenerator.h"
#include "esp8266audio_vendored/AudioGeneratorMP3.h"
#include "esp8266audio_vendored/AudioGeneratorWAV.h"
#include "esp8266audio_vendored/AudioOutputI2S.h"
#include "esp8266audio_vendored/AudioOutputMixer.h"
#include "esp8266audio_vendored/AudioOutputBuffer.h"
#include "esp8266audio_vendored/AudioFileSource.h"
#include "esp8266audio_vendored/AudioFileSourceBuffer.h"
#include "esp8266audio_vendored/AudioFileSourceLittleFS.h"
#include "esp8266audio_vendored/AudioFileSourceID3.h"
#include "esp8266audio_vendored/AudioOutputBuffer.h"
#include "esp8266audio_vendored/AudioOutputFilterBiquad.h"

namespace cogs_sound{
    // Make it so user rules can play sounds
    void begin(AudioOutput * o);
    /// Give a callback cogs can use to enable or disable the hardware DAC
    void setPowerCallback(void (*setHwEnabled)(bool));
}