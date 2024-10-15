#include "esp_sleep.h"
#include "esp_pm.h"
#include "sdkconfig.h"
#include "cogs_util.h"
#include "cogs_bindings_engine.h"
#include "cogs_power_management.h"

namespace cogs_pm
{
// After the first sleep cycle, allow immediate sleep
#if defined(ESP32) || defined(ESP8266)
        const bool allowImmediateSleep = esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED;
#else
        const bool allowImmediateSleep = false;
#endif

        int fps = 48;
        int boostFPS = 0;

        static void onFPSTag(cogs_rules::IntTagPoint *tp)
        {
                if (tp->value[0] > 0)
                {
                        fps = tp->value[0];
                }
                if (fps > 1000)
                {
                        fps = 1000;
                }
                cogs::wakeMainThread();
        }

        static void onDeepSleepGo(cogs_rules::IntTagPoint *tp)
        {
                if (tp->value[0] > 0)
                {
                        // Don't allow user to make a boot loop accidentally
                        if (cogs::uptime() < 60*5*1000)
                        {
                                if (!allowImmediateSleep)
                                {
                                        return;
                                }
                        }
                        auto t = cogs_rules::IntTagPoint::getTag("$deepsleep.time", 3600);
                        unsigned long tm = t->value[0] * 1000000;
                        cogs::logInfo("Going to deep sleep for max time: " + std::to_string(tm) + "us");
                        esp_sleep_enable_timer_wakeup(tm);
                        esp_deep_sleep_start();
                }
        }

        void begin()
        {
                cogs::mainThreadHandle = xTaskGetCurrentTaskHandle();

#ifdef CONFIG_PM_ENABLE
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 4)
                esp_pm_config_t pm;
                esp_pm_get_configuration(&pm);
                esp_pm_get_configuration(&old_pm);
                pm.light_sleep_enable = true;
                pm.max_freq_mhz = 240;
                pm.min_freq_mhz = 80;
                ESP_ERROR_CHECK_WITHOUT_ABORT(esp_pm_configure(&pm));

#elif defined(CONFIG_IDF_TARGET_ESP32S3)
                esp_pm_config_esp32s3_t pm;
                esp_pm_get_configuration(&pm);
                pm.light_sleep_enable = true;
                pm.max_freq_mhz = 240;
                pm.min_freq_mhz = 80;
                ESP_ERROR_CHECK_WITHOUT_ABORT(esp_pm_configure(&pm));

#elif defined(CONFIG_IDF_TARGET_ESP32S2)
                esp_pm_config_esp32s2_t pm;
                esp_pm_get_configuration(&pm);
                pm.light_sleep_enable = true;
                pm.max_freq_mhz = 240;
                pm.min_freq_mhz = 80;
                ESP_ERROR_CHECK_WITHOUT_ABORT(esp_pm_configure(&pm));

#elif defined(ESP32)
                esp_pm_config_esp32_t pm;
                esp_pm_get_configuration(&pm);
                pm.light_sleep_enable = true;
                pm.max_freq_mhz = 240;
                pm.min_freq_mhz = 80;
                ESP_ERROR_CHECK_WITHOUT_ABORT(esp_pm_configure(&pm));
#elif defined(ESP8266)
                esp_pm_config_esp8266_t pm;
                esp_pm_get_configuration(&pm);
                pm.light_sleep_enable = true;
                pm.max_freq_mhz = 240;
                pm.min_freq_mhz = 80;
                ESP_ERROR_CHECK_WITHOUT_ABORT(esp_pm_configure(&pm));
#else
                cogs::logInfo("Power management not supported");

#endif

#endif

#ifndef CONFIG_PM_ENABLE
                cogs::logInfo("Power management not supported or not enabled at SDK level");
#endif

                auto t = cogs_rules::IntTagPoint::getTag("$deepsleep.time", 3600);
                t->setUnit("s");

                t = cogs_rules::IntTagPoint::getTag("$deepsleep.go", 0);
                t->subscribe(&onDeepSleepGo);
                t->setUnit("bang");

                t = cogs_rules::IntTagPoint::getTag("$fps", 48);
                t->subscribe(&onFPSTag);
                t->setUnit("fps");
                t->min = 1;
                t->max = 1000;
        }
}