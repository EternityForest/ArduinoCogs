#include <LittleFS.h>
#include "cogs_bindings_engine.h"
#include "cogs_global_events.h"
#include "cogs_sound.h"

#include <string>
static inline bool ends_with(std::string const &value, std::string const &ending)
{
    if (ending.size() > value.size())
        return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}
static unsigned long lastError = 0;

// Called when there's a warning or error (like a buffer underflow or decode hiccup)
void StatusCallback(void *cbData, int code, const char *string)
{
    if (millis() - lastError < 10000)
    {
        return;
    }
    if (code == 257)
    {
        return;
    }
    lastError = millis();
    const char *ptr = reinterpret_cast<const char *>(cbData);
    // Note that the string may be in PROGMEM, so copy it to RAM for printf
    char s1[64];
    strncpy_P(s1, string, sizeof(s1));
    s1[sizeof(s1) - 1] = 0;

    cogs::logError(std::string(ptr) + " " + std::string(s1) + " (" + std::to_string(code) + ")");
}

// pointer to function taking one bool argument
static void (*setHwEnabled)(bool) = 0;

namespace cogs_sound
{
    std::map<std::string, std::shared_ptr<cogs_rules::IntTagPoint>> soundFileMap;

    static void waitForAudioThread();
    static void playSoundTag(cogs_rules::IntTagPoint *t);
    static void playMusicTag(cogs_rules::IntTagPoint *t);

    static unsigned long lastActivity = 0;

    void setPowerCallback(void (*f)(bool))
    {
        setHwEnabled = f;
    }

    static void fileChangeHandler(cogs::GlobalEvent evt, int dummy, const std::string &path)
    {

        if (evt != cogs::fileChangeEvent)
        {
            return;
        }
        if ((path.rfind("/sfx", 0) == 0) || (path.rfind("/music", 0) == 0))
        {
            if (!(ends_with(path, ".mp3") || ends_with(path, ".wav") || ends_with(path, ".opus")))
            {
                return;
            }

            File f = LittleFS.open(path.c_str()); // flawfinder: ignore

            // If the file exists, add it to the map
            if (f)
            {
                if (soundFileMap.count(path) == 1)
                {
                    return;
                }

                std::string fn = f.path();

                char *buf = reinterpret_cast<char *>(malloc(fn.size() + 1));

                if (!buf)
                {
                    throw std::runtime_error("malloc");
                }
                strcpy(buf, fn.c_str()); // flawfinder: ignore

                std::string tn = f.name();

                if (path.rfind("/sfx", 0) == 0)
                {
                    tn = "sfx.files." + tn;
                }
                else
                {
                    tn = "music.files." + tn;
                }

                soundFileMap[path] = cogs_rules::IntTagPoint::getTag(tn, 0, 1);
                soundFileMap[path]->extraData = buf;
                soundFileMap[path]->setUnit("bang");

                if (path.rfind("/music", 0) == 0)
                {
                    soundFileMap[path]->subscribe(playMusicTag);
                }
                else
                {
                    soundFileMap[path]->subscribe(playSoundTag);
                }
            }
            else
            {

                if (soundFileMap.count(path) == 1)
                {

                    if (soundFileMap[path]->extraData)
                    {
                        free(soundFileMap[path]->extraData);

                        soundFileMap[path]->extraData = 0;
                    }
                    soundFileMap[path]->unregister();
                    soundFileMap.erase(path);
                }
            }
        }
    }

    class SoundPlayer
    {
    public:
        std::string fn;
        bool shouldLoop = false;

        bool shouldDelete = 0;

        float vol;
        float fade;
        unsigned long fadeStart;
        float initialVol;
        AudioFileSource *src = 0;
        AudioGenerator *gen = 0;
        AudioFileSourceID3 *id3 = 0;
        AudioOutputMixerStub *stub = 0;

        unsigned long endTime = 0;

        SoundPlayer(AudioOutputMixer *mixer, std::string fn, bool loop, float vol, float fade, float initialVol, AudioOutputMixerStub *stb = 0)
        {

            if (setHwEnabled)
            {
                setHwEnabled(true);
            }

            this->fn = fn;
            this->shouldLoop = loop;
            this->fade = fade;
            this->vol = vol;
            this->initialVol = initialVol;
            if (stb)
            {
                this->stub = stb;
            }
            else
            {
                this->stub = mixer->NewInput();
            }
            if (!this->stub)
            {
                throw std::runtime_error("Bad Stub");
            }
            //cogs::logInfo("stub set");
            this->src = new AudioFileSourceLittleFS(fn.c_str());

            //cogs::logInfo("file set");
            std::string ext = fn.substr(fn.size() - 3, 3);

            if (ext == "mp3")
            {

                this->id3 = new AudioFileSourceID3(this->src);
                this->gen = new AudioGeneratorMP3();
                if (!this->gen->begin(this->id3, this->stub))
                {
                    cogs::logError("SoundPlayer: unable to start generating sound");
                    this->shouldDelete = true;
                }
            }
            else if (ext == "wav")
            {
                this->gen = new AudioGeneratorWAV();
                if (!this->gen->begin(this->src, this->stub))
                {
                    cogs::logError("SoundPlayer: unable to start generating sound");
                    this->shouldDelete = true;
                }
            }

            else if (ext == "pus")
            {
                this->gen = new AudioGeneratorOpus();
                if (!this->gen->begin(this->src, this->stub))
                {
                    cogs::logError("SoundPlayer: unable to start generating sound");
                    this->shouldDelete = true;
                }
            }
            //cogs::logInfo("gen set");

            // this->gen->RegisterStatusCB(&StatusCallback, (void *)"snd");
            this->stub->SetGain(initialVol);
            //cogs::logInfo("gain set");

            this->fadeStart = millis();
        }

        void setGain(float gain)
        {
            if (this->stub)
            {
                this->stub->SetGain(gain);
            }
        }
        bool isRunning()
        {
            if (this->shouldDelete)
            {
                return false;
            }
            if (!this->gen)
            {
                return false;
            }
            return this->gen->isRunning();
        }

        inline bool loop()
        {
            return this->gen->loop();
        }

        void fadeOut(float fade)
        {
            this->fadeStart = millis();
            this->initialVol = this->vol;
            this->fade = fade;
            this->vol = 0;

            this->endTime = this->fadeStart + int(this->fade * 1000);
            if (this->endTime == 0)
            {
                this->endTime = 1;
            }
        }

        void doFade()
        {
            if (this->shouldDelete)
            {
                return;
            }
            if (this->fade > 0.0)
            {
                unsigned long now = millis();
                unsigned long p = now - this->fadeStart;
                float position = float(p) / float(this->fade * 1000);
                if (position >= 1.0)
                {
                    this->fade = 0;
                }

                if (position < 0.0)
                {
                    position = 0.0;
                }
                if (position > 1.0)
                {
                    position = 1.0;
                }

                float v = this->vol * position + this->initialVol * (1 - position);
                this->setGain(v);
            }

            if (this->endTime)
            {
                unsigned long now = millis();

                // rollover compare
                if ((now - this->endTime) > 1000000000L)
                {
                    this->shouldDelete = true;
                    return;
                }
            }
        }

        void stop()
        {
            if (this->gen)
            {
                if (this->stub)
                {
                    this->stub->ignoreStop = false;
                }
                this->gen->stop();
                delete this->gen;
                if (this->stub)
                {
                    delete this->stub;
                }
                this->gen = 0;
                this->stub = 0;
            }
        }

        ~SoundPlayer()
        {
            if (this->src)
            {
                delete this->src;
            }
            if (this->gen)
            {
                delete this->gen;
            }
            if (this->id3)
            {
                delete this->id3;
            }
            if (this->stub)
            {
                delete this->stub;
                this->stub = 0;
            }
        }
    };

    AudioOutput *output;
    AudioOutputMixer *mixer;
    std::vector<AudioOutputMixerStub *> stubs;

    TaskHandle_t audioThreadHandle = 0;

    SoundPlayer *music = 0;
    SoundPlayer *music_old = 0;
    SoundPlayer *fx = 0;

    bool musicLoop = false;
    float musicFadeIn = 0.0;
    float musicFadeOut = 0.0;
    float musicVol = 1.0;

    float fxFadeIn = 0.0;
    float fxFadeOut = 0.0;
    float fxVol = 1.0;

    // Used to sync the bg thread
    int audioThreadIter = 0;

    AudioFileSourceID3 *id3[2] = {0, 0};

    // Need GIL
    static void playMusic(const std::string &fn, bool loop, float vol, float fade, AudioOutputMixerStub *stub = 0)
    {

        // If there is already a background, stop it because we can't have 3
        // at the same time
        if (music_old)
        {
            SoundPlayer *m2 = music_old;
            music_old = 0;
            // Make sure audio thread completed its loop
            // and is done with the object
            waitForAudioThread();
            m2->stop();
            delete m2;
        }

        // Move it to the background slot,
        // let if get stopped after the linger time.
        if (music)
        {
            SoundPlayer *m = music;
            if (musicFadeOut > 0.0)
            {
                m->fadeStart = millis();
                m->endTime = millis() + int(musicFadeOut * 1000);
                m->initialVol = m->vol;
                m->fade = musicFadeOut;
                m->vol = 0.0;
            }
            else
            {
                m->shouldDelete = true;
            }
            music = 0;
            waitForAudioThread();
            music_old = m;
        }

        if (fade > 0.0)
        {
            music = new SoundPlayer(mixer, fn, loop, vol, fade, 0.0, stub);
        }
        else
        {
            music = new SoundPlayer(mixer, fn, loop, vol, 0.0, vol, stub);
        }

        xTaskNotifyGive(audioThreadHandle);
    }

    void playFX(const std::string &fn, float vol, float fadein = 0.0)
    {

        if (fx)
        {
            //cogs::logInfo("stopping fx");
            SoundPlayer *f = fx;
            fx = 0;
            waitForAudioThread();
            f->stop();
            //cogs::logInfo("delete fx");
            delete f;
            //cogs::logInfo("stopped fx");
        }

        //cogs::logInfo("Beguinning fx");
        if (fadein > 0.0)
        {
            fx = new SoundPlayer(mixer, fn, false, vol, fadein, 0);
        }
        else
        {
            fx = new SoundPlayer(mixer, fn, false, vol, 0, vol);
        }
        xTaskNotifyGive(audioThreadHandle);
    }

    static void setMusicVolumeTag(cogs_rules::IntTagPoint *t)
    {
        int32_t vol = t->value[0];
        musicVol = vol / float(cogs_rules::FXP_RES);

        SoundPlayer *m = music;
        if (m)
        {
            m->vol = vol / float(cogs_rules::FXP_RES);
            // Just let fade handle it
            if (m->fade == 0.0)
            {
                m->setGain(m->vol);
            }
        }

        SoundPlayer *m2 = music_old;
        if (m2)
        {
            float v = vol / float(cogs_rules::FXP_RES);
            // I think this is probably about what a user would want?
            if (v < m2->vol)
            {
                m2->vol = v;
                if (m2->fade == 0.0)
                {
                    m2->setGain(m2->vol);
                }
            }
        }
    }

    static void setSfxVolumeTag(cogs_rules::IntTagPoint *t)
    {
        int32_t vol = t->value[0];
        fxVol = vol / float(cogs_rules::FXP_RES);

        SoundPlayer *f = fx;
        if (f)
        {
            f->vol = vol / float(cogs_rules::FXP_RES);
            // Just let fade handle it
            if (f->fade == 0.0)
            {
                f->setGain(f->vol);
            }
        }
    }

    static void setMusicFadeInTag(cogs_rules::IntTagPoint *t)
    {
        int32_t v = t->value[0];
        musicFadeIn = v / float(cogs_rules::FXP_RES);
    }

    static void setMusicFadeOutTag(cogs_rules::IntTagPoint *t)
    {
        int32_t v = t->value[0];
        musicFadeOut = v / float(cogs_rules::FXP_RES);
    }

    static void setSfxFadeInTag(cogs_rules::IntTagPoint *t)
    {
        int32_t v = t->value[0];
        fxFadeIn = v / float(cogs_rules::FXP_RES);
    }

    static void setSfxFadeOutTag(cogs_rules::IntTagPoint *t)
    {
        int32_t v = t->value[0];
        fxFadeOut = v / float(cogs_rules::FXP_RES);
    }

    static void setMusicLoopTag(cogs_rules::IntTagPoint *t)
    {
        bool v = t->value[0] > 0;

        SoundPlayer *m = music;
        if (m)
        {
            m->shouldLoop = v;
        }
        musicLoop = v;
    }

    void playSoundTag(cogs_rules::IntTagPoint *t)
    {

        bool v = t->value[0] > 0;
        if (!v)
        {
            return;
        }
        t->silentResetValue();

        playFX(reinterpret_cast<const char *>(t->extraData),
               fxVol,
               fxFadeIn);
    }

    static void playMusicTag(cogs_rules::IntTagPoint *t)
    {
        bool v = t->value[0] > 0;
        if (!v)
        {
            return;
        }
        t->silentResetValue();

        playMusic(reinterpret_cast<const char *>(t->extraData),
                  musicLoop,
                  musicVol,
                  musicFadeIn);
    }

    static void stopMusicTag(cogs_rules::IntTagPoint *t)
    {
        bool v = t->value[0] > 0;
        if (!v)
        {
            return;
        }
        t->silentResetValue();

        SoundPlayer *m = music;
        if (m)
        {

            // If there is already a background, stop it.
            if (music_old)
            {
                SoundPlayer *m2 = music_old;
                music_old = 0;
                // Make sure audio thread completed its loop
                // and is done with the object
                waitForAudioThread();
                m2->stop();
                delete m2;
            }

            if (musicFadeOut > 0.0)
            {
                m->fadeStart = millis();
                m->endTime = millis() + int(musicFadeOut * 1000);
                m->initialVol = m->vol;
                m->fade = musicFadeOut;
                m->vol = 0.0;
            }
            else
            {
                m->shouldDelete = true;
            }
            music = 0;
            waitForAudioThread();
            music_old = m;
        }
    }

    void stopSfxTag(cogs_rules::IntTagPoint *t)
    {
        bool v = t->value[0] > 0;
        if (!v)
        {
            return;
        }
        t->silentResetValue();
        SoundPlayer *f = fx;
        if (f)
        {
            if (fxFadeOut > 0.0)
            {
                f->initialVol = f->vol;
                f->fade = fxFadeOut;
                f->vol = 0.0;
                f->fadeStart = millis();
                f->endTime = millis() + int(musicFadeOut * 1000);
            }
            else
            {
                fx = 0;
                waitForAudioThread();
                f->stop();
                delete f;
            }
        }
    }

    static void waitForAudioThread()
    {
        //cogs::logInfo("Waiting for audio thread");
        int c = audioThreadIter;
        // In case it's blocked.
        xTaskNotifyGive(audioThreadHandle);
        int safety = 1000;
        while (c == audioThreadIter)
        {
            safety--;
            if (safety == 0)
            {
                break;
            }
            delay(1);
        }
        //cogs::logInfo("Audio thread done");
    }

    // Handles fades and loops.
    // Need GIL
    void audioMaintainer()
    {
        bool en = false;

        if (music)
        {
            en = true;
            if (!music->isRunning())
            {
                SoundPlayer *m = music;
                bool l = music->shouldLoop;
                AudioOutputMixerStub *stb = music->stub;

                // Rescue mission!
                // Take the stub away so it doesn't get trashed
                // We need to reuse it because the start and end need sample perfect alignment.
                if (l)
                {
                    stb->ignoreStop = true;
                    m->stub = 0;
                }
                else
                {
                    stb->ignoreStop = false;
                    stb = 0;
                }

                std::string fn = m->fn;
                music = 0;
                // Make sure audio thread completed its loop
                // and is done with the object
                waitForAudioThread();

                m->stop();
                delete m;

                if (l)
                {
                    playMusic(fn, true, musicVol, 0, stb);
                }
            }
            else
            {
                music->doFade();
            }
        }

        if (music_old)
        {
            en = true;
            if (!music_old->isRunning())
            {
                SoundPlayer *m = music_old;
                music_old = 0;
                // Make sure audio thread completed its loop
                // and is done with the object
                waitForAudioThread();

                m->stop();
                delete m;
            }
            else
            {
                music_old->doFade();
            }
        }

        if (fx)
        {
            en = true;
            if (!fx->isRunning())
            {
                SoundPlayer *m = fx;
                fx = 0;
                // Make sure audio thread completed its loop
                // and is done with the object
                waitForAudioThread();

                m->stop();
                delete m;
            }
            else
            {
                fx->doFade();
            }
        }
        if (en)
        {
            lastActivity = millis();
        }

        if ((millis() - lastActivity) > 10000)
        {
            if (setHwEnabled)
            {
                setHwEnabled(false);
            }
        }
    };

    void audioThread(void *parameter)
    {
        while (1)
        {
            // Idle to reduce power
            if (!(music || music_old || fx))
            {
                // Loop the mixer to be sure it's flushed
                if (millis() - lastActivity > 500)
                {
                    mixer->loop();
                }
                else
                {
                    ulTaskNotifyTake(pdTRUE, 1000);
                }
            }
            else
            {
                SoundPlayer *m = music;
                SoundPlayer *m2 = music_old;
                SoundPlayer *m3 = fx;
                if (m)
                {
                    if (m->isRunning())
                    {
                        if (!m->loop())
                        {
                            m->shouldDelete = true;
                        }
                    }
                }
                if (m2)
                {
                    if (m2->isRunning())
                    {
                        if (!m2->loop())
                        {
                            m2->shouldDelete = true;
                        }
                    }
                }
                if (m3)
                {
                    if (m3->isRunning())
                    {
                        if (!m3->loop())
                        {
                            m3->shouldDelete = true;
                        }
                    }
                }
            }
            audioThreadIter++;
            // Delays are not precise enough for any fancy duration logic here.
            vTaskDelay(2);
        }
    }

    void begin(AudioOutput *op)
    {
        if (!op)
        {
            cogs::logError("Null pointer");
        }

        if (output)
        {
            cogs::logError("Audio output already set");
            return;
        }

        output = op;

        mixer = new AudioOutputMixer(1024, op);

        cogs::registerFastPollHandler(audioMaintainer);

        cogs::ensureDirExists("/sfx");
        cogs::ensureDirExists("/music");

        auto t = cogs_rules::IntTagPoint::getTag("music.volume", cogs_rules::FXP_RES);
        t->setScale(cogs_rules::FXP_RES);
        t->min = 0;
        t->max = 3;
        t->subscribe(&setMusicVolumeTag);

        t = cogs_rules::IntTagPoint::getTag("sfx.volume", cogs_rules::FXP_RES);
        t->setScale(cogs_rules::FXP_RES);
        t->min = 0;
        t->max = 3;

        t->subscribe(&setSfxVolumeTag);

        t = cogs_rules::IntTagPoint::getTag("music.fadein", 0);
        t->setScale(cogs_rules::FXP_RES);
        t->subscribe(&setMusicFadeInTag);
        t->min = 0;
        t->max = 60;
        t->setUnit("s");

        t = cogs_rules::IntTagPoint::getTag("music.fadeout", 0);
        t->setScale(cogs_rules::FXP_RES);
        t->min = 0;
        t->max = 60;
        t->setUnit("s");

        t->subscribe(&setMusicFadeOutTag);

        t = cogs_rules::IntTagPoint::getTag("sfx.fadein", 0);
        t->setScale(cogs_rules::FXP_RES);
        t->min = 0;
        t->max = 60;
        t->subscribe(&setSfxFadeInTag);
        t->setUnit("s");

        t = cogs_rules::IntTagPoint::getTag("sfx.fadeout", 0);
        t->setScale(cogs_rules::FXP_RES);
        t->min = 0;
        t->max = 60;
        t->setUnit("s");

        t->subscribe(&setSfxFadeOutTag);

        t = cogs_rules::IntTagPoint::getTag("music.loop", 0);
        t->subscribe(&setMusicLoopTag);
        t->setUnit("bool");

        t = cogs_rules::IntTagPoint::getTag("sfx.stop", 0);
        t->subscribe(&stopSfxTag);
        t->setUnit("bang");

        t = cogs_rules::IntTagPoint::getTag("music.stop", 0);
        t->subscribe(&stopMusicTag);
        t->setUnit("bang");

        File dir = LittleFS.open("/sfx"); // flawfinder: ignore
        if (dir)
        {
            if (dir.isDirectory())
            {

                File f = dir.openNextFile();
                while (f)
                {

                    if (!f.isDirectory())
                    {
                        fileChangeHandler(cogs::fileChangeEvent, 0, f.path());
                    }
                    f = dir.openNextFile();
                }
            }
        }
        dir = LittleFS.open("/music"); // flawfinder: ignore
        if (dir)
        {
            if (dir.isDirectory())
            {

                File f = dir.openNextFile();
                while (f)
                {

                    if (!f.isDirectory())
                    {
                        fileChangeHandler(cogs::fileChangeEvent, 0, f.path());
                    }
                    f = dir.openNextFile();
                }
            }
        }

        cogs::globalEventHandlers.push_back(fileChangeHandler);

        xTaskCreate(
            audioThread,       // Function name of the task
            "audio",           // Name of the task (e.g. for debugging)
            8192 * 2,          // Stack size (bytes)
            NULL,              // Parameter to pass
            10,                // Task priority
            &audioThreadHandle // Task handle
        );
    }
}
