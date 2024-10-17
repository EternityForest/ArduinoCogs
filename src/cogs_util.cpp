#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "cogs_util.h"
#include "cogs_global_events.h"
#include "cogs_power_management.h"
#include "web/cogs_web.h"

static uint32_t randomState = 1;

static unsigned long errorHorizon = 0;
static int errorCount = 0;

static uint64_t uptimeAccumulator = 0;
static unsigned long lastUptime = millis();

namespace cogs
{
    TaskHandle_t mainThreadHandle = 0;

    const std::string emptyString = "";
    const std::shared_ptr<const std::string> emptySharedString = std::shared_ptr<const std::string>(&emptyString);

    std::vector<std::shared_ptr<const std::string>> stringPool;

    void wakeMainThread()
    {
        TaskHandle_t handle = mainThreadHandle;
        if (handle != 0)
        {
            xTaskNotifyGive(mainThreadHandle);
        }
    }

    void wakeMainThreadISR()
    {
        TaskHandle_t handle = mainThreadHandle;
        if (handle != 0)
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xTaskNotifyFromISR(mainThreadHandle, 0, eNoAction, &xHigherPriorityTaskWoken);
        }
    }

    uint64_t uptime()
    {
        unsigned long now = millis();
        unsigned long delta = now - lastUptime;
        uptimeAccumulator += delta;
        lastUptime = now;
        return uptimeAccumulator;
    }

    std::shared_ptr<const std::string> getSharedString(const std::string &str)
    {
        if (str.size() == 0)
        {
            return emptySharedString;
        }

        for (auto const &e : stringPool)
        {
            if (e->compare(str) == 0)
            {
                return e;
            }
        }
        if (stringPool.size() > 32)
        {
            stringPool.erase(stringPool.begin());
        }

        std::shared_ptr<const std::string> ret = std::make_shared<const std::string>(str);
        if (str.size() < 16)
        {
            stringPool.push_back(ret);
        }

        return ret;
    }

    SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutex();

    void lock()
    {
        if (!xSemaphoreTakeRecursive(mutex, pdMS_TO_TICKS(30000)))
        {
            throw std::runtime_error("Timed out waiting for mutex");
        };
    }

    void unlock()
    {
        xSemaphoreGiveRecursive(mutex);
    }

    int bang(int v)
    {
        v++;
        if (v < 1)
        {
            v = 1;
        }
        return v;
    }

    uint32_t random() // flawfinder: ignore
    {
        /// Adding this seeds it with timing entropy.
        randomState += micros();
        // This is an XORshift function
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;
        return randomState;
    }

    uint32_t random(uint32_t min, uint32_t max) // flawfinder: ignore
    {
        return random() % (max - min) + min; // flawfinder: ignore
    }

    void waitFrame()
    {
        if (cogs_pm::boostFPS > 0)
        {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS((1000 / 48)));
        }
        else
        {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS((1000 / cogs_pm::fps)));
        }
    }

    void ensureDirExists(const std::string &dir)
    {
        int safety = 24;
        std::string dirname = dir;

        while (dirname.size() > 1)
        {
            safety--;
            if (safety < 0)
            {
                break;
            }
            if (!LittleFS.exists(dirname.c_str()))
            {
                LittleFS.mkdir(dirname.c_str());
            }
            dirname = dirname.substr(0, dirname.rfind('/'));
        }
    }

    void setDefaultFile(const std::string &fn, const std::string &content)
    {
        std::string dirname = fn.substr(0, fn.rfind('/'));
        ensureDirExists(dirname);

        File f = LittleFS.open(fn.c_str(), "r"); // flawfinder: ignore
        if (f)
        {
            if (f.size() > 0)
            {
                f.close();
                return;
            }
            f.close();
        }

        File f2 = LittleFS.open(fn.c_str(), FILE_WRITE); // flawfinder: ignore
        if (!f2)
        {
            cogs::logError("Error opening file for writing: " + fn);
            return;
        }
        f2.write(reinterpret_cast<const uint8_t *>(content.c_str()), content.size());
        f2.close();
        cogs::triggerGlobalEvent(cogs::fileChangeEvent, 0, fn);
    };

    void logError(const std::string &msg)
    {
        // Allow an error to be logged every 10 seconds
        // But also we allow bursts of 10
        errorCount -= (millis() - errorHorizon) / 10000;
        if (errorCount < 0)
        {
            errorCount = 0;
        }
        if (errorCount > 10)
        {
            return;
        }

        //Red text
        Serial.print("\033[0;41m");
        Serial.println(msg.c_str());
        Serial.print("\033[0m");

        if (msg.size() < 128)
        {
            Serial.flush();
        }

        if(xSemaphoreTakeRecursive(mutex, pdMS_TO_TICKS(300))){
        cogs_web::wsBroadcast("__ERROR__", msg.c_str());
        cogs::unlock();
        }
        else{
            Serial.println("Timed out waiting for mutex to broadcast error");
        }
    }

    void logInfo(const std::string &msg)
    {
        cogs_web::wsBroadcast("info", msg.c_str());
        Serial.println(msg.c_str());
    }

    std::string getHostname()
    {
        auto f = LittleFS.open("/config/device.json", "r"); // flawfinder: ignore
        if ((!f) || f.isDirectory())
        {
            return "cogs";
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, f);
        if (error)
        {
            return "cogs";
        }

        if (doc["name"].is<const char *>())
        {
            return doc["name"].as<std::string>();
        }
        else
        {
            return "cogs";
        }
    }

};
